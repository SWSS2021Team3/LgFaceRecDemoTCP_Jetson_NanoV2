#include "faceManager.h"
#include <opencv2/core/cuda.hpp>
#include <opencv2/cudawarping.hpp>
#include <iostream>
#include <cstring>
#include <string>

FaceManager::FaceManager(CommManager *comm) : useCamera(true), rotate180(true), commManager(comm)
{
    bool isCSICam = true;

    // init opencv stuff
    videoStreamer = new VideoStreamer(0, videoFrameWidth, videoFrameHeight, 60, isCSICam);
}

FaceManager::FaceManager(CommManager *comm, const char *filename) : useCamera(false), rotate180(false), commManager(comm)
{
    // init opencv stuff
    videoStreamer = new VideoStreamer(filename, videoFrameWidth, videoFrameHeight);mCurrentUid = -1;
    mCurrentUid = -1;
}

FaceManager::~FaceManager()
{
    videoStreamer->release();

    // instance from init()
    delete mtCNN;
    delete faceNet;
}

bool FaceManager::init()
{
    Logger gLogger = Logger();
    // Register default TRT plugins (e.g. LRelu_TRT)
    if (!initLibNvInferPlugins(&gLogger, ""))
    {
        return false;
    }

    // USER DEFINED VALUES
    const string uffFile = "../facenetModels/facenet.uff";
    const string engineFile = "../facenetModels/facenet.engine";
    DataType dtype = DataType::kHALF;
    //DataType dtype = DataType::kFLOAT;
    bool serializeEngine = true;
    int batchSize = 1;

    int maxFacesPerScene = 8;
    float knownPersonThreshold = 1.;

    // init facenet
    faceNet = new FaceNetClassifier(gLogger, dtype, uffFile, engineFile, batchSize, serializeEngine,
                                    knownPersonThreshold, maxFacesPerScene, videoFrameWidth, videoFrameHeight);

    // init mtCNN
    mtCNN = new mtcnn(videoFrameHeight, videoFrameWidth);

    //init Bbox and allocate memory for "maxFacesPerScene" faces per scene
    std::vector<struct Bbox> outputBbox;
    outputBbox.reserve(maxFacesPerScene);

    // get embeddings of known faces
    std::vector<struct Paths> paths;
    cv::Mat image;
    getFilePaths("../imgs", paths);
    for (int i = 0; i < paths.size(); i++)
    {
        loadInputImage(paths[i].absPath, image, videoFrameWidth, videoFrameHeight);
        outputBbox = mtCNN->findFace(image);
        std::size_t index = paths[i].fileName.find_last_of(".");
        std::string rawName = paths[i].fileName.substr(0, index);
        faceNet->forwardAddFace(image, outputBbox, rawName);
        faceNet->resetVariables();
    }
    outputBbox.clear();
    return true;
}

void FaceManager::start()
{
    if (useCamera)
        openRecordFile("record.avi");
}

void FaceManager::openRecordFile(const char *filename)
{
    writer.open(filename, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
                24 /* FPS */, cv::Size(videoFrameWidth, videoFrameHeight), true);
}

void FaceManager::record(cv::Mat &frame)
{
    writer << frame;
}

void FaceManager::rotateFrame(cv::Mat &frame)
{
    cv::cuda::GpuMat src_gpu, dst_gpu;
    src_gpu.upload(frame);
    cv::cuda::rotate(src_gpu, dst_gpu, src_gpu.size(), 180, src_gpu.size().width, src_gpu.size().height);
    dst_gpu.download(frame);
}

bool FaceManager::processFrame()
{
    cv::Mat frame;
    cv::Mat dst_img;
    std::vector<struct Bbox> outputBbox;

    if (!isVideoPlay) return true;

    videoStreamer->getFrame(frame);
    if (frame.empty())
    {
        std::cout << "Empty frame! Exiting...\n Try restarting nvargus-daemon by "
                     "doing: sudo systemctl restart nvargus-daemon"
                  << std::endl;
        return false;
    }

    // Create a destination to paint the source into.
    dst_img.create(frame.size(), frame.type());

    // Push the images into the GPU
    if (rotate180)
        rotateFrame(frame);

    if (useCamera)
        record(frame);

    // auto startMTCNN = chrono::steady_clock::now();
    outputBbox = mtCNN->findFace(frame);
    // auto endMTCNN = chrono::steady_clock::now();
    // auto startForward = chrono::steady_clock::now();
    faceNet->forward(frame, outputBbox);
    // auto endForward = chrono::steady_clock::now();
    // auto startFeatM = chrono::steady_clock::now();
    vector<string> matchedUser;

    faceNet->featureMatching(frame, matchedUser);
    // auto endFeatM = chrono::steady_clock::now();
    faceNet->resetVariables();

    if (!commManager->sendFrame(frame))
        return false;
    for (std::string &u : matchedUser)
    {
        commManager->sendMatchedUser(u);
    }

    outputBbox.clear();
    // frame.release();

    return true;
}

void FaceManager::playVideo(std::string status)
{
    if (status == "start")
        isVideoPlay = true;
    else
        isVideoPlay = false;
}

bool FaceManager::registerFace(string userId, int numberOfImages)
{
    cv::Mat frame;
    cv::Mat croppedFace;

    cv::Mat dst_img;
    std::vector<struct Bbox> outputBbox;

    for (int i=0; i< numberOfImages; i++)
    {
        bool faceDetected = false;
        int cnt = 0;
        while (!faceDetected && cnt < 600)
        {
            videoStreamer->getFrame(frame);

            // TODO: what if frame is empty??
            if (frame.empty())
            {
                std::cout << "frame is empty" << std::endl;
                break;
            }

            // Create a destination to paint the source into.
            dst_img.create(frame.size(), frame.type());

            // Push the images into the GPU
            if (rotate180)
                rotateFrame(frame);

            outputBbox = mtCNN->findFace(frame);
            // if (!commManager->sendFrame(frame))
            //     return false;

            //cv::imshow("VideoSource", frame);
            faceDetected = faceNet->addNewFace(frame, outputBbox, croppedFace);
            cnt++;
        }
        if (!faceDetected) // timeout
        {
            std::cout << "no face detected in camera." << std::endl;
            return false;
        }

        if (!commManager->sendFace(croppedFace))
            return false;

        // Put faceId with current timestamp
        auto now = std::chrono::system_clock::now();
        string timestamp = to_string(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());
        string faceId = userId + timestamp;
        std::cout << "faceId Created with userId + timestamp = " << faceId << std::endl;
        if (!addFaceDB(userId,faceId))
            return false;
        
        frame.release();
        croppedFace.release();
        for (int j=0; j< 60; j++)
        {
            videoStreamer->getFrame(frame);
            frame.release();
        }
    }

    // frame.release();
    return true;
}

void FaceManager::sendFaceImages(string userId)
{
    vector<string> face_list = getFaceListFromDB(userId);
    string imagepath = "../imgs";
    
    // TODO: read user's images
    std::vector<struct Paths> paths;
    cv::Mat image;
    int len = face_list.size();
    if (len > 2) len = 2;   // TODO: limited for test
    for (int i = 0; i < len; i++)
    {
        string absPath = imagepath + "/" + face_list[i] + ".jpg";
        std::cout << "loading " << i << " " << absPath << std::endl;
        loadInputImage(absPath, image, videoFrameWidth, videoFrameHeight);
        commManager->sendRegisteredFace(image);
    }
}

void FaceManager::stop()
{
}

bool FaceManager::deleteFaceDB(string userId, string faceId)
{
    std::cout << "deleteFace: " << userId << " / " << faceId << endl;
    // TODO
    // 1.delete facelist and update DB
    // uid -> student name
    string temp_line;
    vector<faceData> v_fd = readFaceDB();
    ifstream face_db_read("./facelist_read");
    ofstream face_db_write("./facelist_write");
    size_t pos = 0;
    while(face_db_read.good())
    {
        getline(face_db_read, temp_line);
        if(temp_line.size() == 0)
            break;
        // face_db_write << temp_line;
        if((pos = temp_line.find(userId)) != std::string::npos)
        {
            std::cout << "Find userId : " << userId << " pos : " << pos << endl;
            if((pos = temp_line.find(faceId)) != std::string::npos)
            {
                std::cout << "Delte faceId : " << faceId << " pos : " << pos << endl;
                temp_line.erase(pos-1, faceId.size()+1);    //Delete with white_space(ex. " abc")
            }
        }
        face_db_write << temp_line;
        face_db_write << "\n";
    }
    face_db_read.close();
    face_db_write.close();

    for(int i=0; i<v_fd.size(); i++)    //for debug
    {
        if(!strncmp(v_fd[i].userID.c_str(), userId.c_str(), userId.size()))
        {
            std::cout << "Found Face List and add face list" << endl;
            for(int j=0; j<v_fd[i].faceNumber.size(); j++)
            {
                std::cout << "List : " << v_fd[i].faceNumber[j] << endl;
                if(!strncmp(v_fd[i].faceNumber[j].c_str(), faceId.c_str(), faceId.size()))
                {
                    std::cout << "Delete : " << v_fd[i].faceNumber[j] << endl;
                    v_fd[i].faceNumber.erase(v_fd[i].faceNumber.begin(),v_fd[i].faceNumber.begin()+j);
                }
            }
            return true;
        }
    }
    return false;
    // TODO : update AI handler
    // TODO : write data to db file
    // 2.update(reload) FaceManager (AI handler)
    return true;
}

bool FaceManager::addFaceDB(string userId, string faceId)
{
    std::cout << "addFaceDB: " << userId << " / " << faceId << endl;
    // TODO
    // 1.add FaceList and update DB
    // uid -> student name
    string temp_line;
    vector<faceData> v_fd = readFaceDB();
    ifstream face_db_read("./facelist_read");
    ofstream face_db_write("./facelist_write");
    size_t pos = 0;
    while(face_db_read.good())
    {
        getline(face_db_read, temp_line);
        if(temp_line.size() == 0)
            break;
        face_db_write << temp_line;
        if(pos = temp_line.find(userId) != std::string::npos)
        {
            std::cout << "Find userId : " << userId << " Add : " << faceId << endl;
            face_db_write << " ";
            face_db_write.write(faceId.c_str(),faceId.size());
        }
        face_db_write << "\n";
    }
    face_db_read.close();
    face_db_write.close();

    for(int i=0; i<v_fd.size(); i++)    //for debug
    {
        if(!strncmp(v_fd[i].userID.c_str(), userId.c_str(), userId.size()))
        {
            std::cout << "Found Face List and add face list" << endl;
            v_fd[i].faceNumber.push_back(faceId);
            for(int j=0; j<v_fd[i].faceNumber.size(); j++)
            {
                std::cout << "List : " << v_fd[i].faceNumber[j] << endl;
            }
            return true;
        }
    }
    return false;
    // TODO : update(reload) AI handler
    // TODO : write data to db file
    return true;
}

bool FaceManager::findUserFromDB(string userId)
{
    std::cout << "findUserFromDB: " << userId << endl;

    return true;
}

void FaceManager::setCurrentUid(string userId) {
    int uid = stoi(userId);
    if (uid >= 0) {
        mCurrentUid = uid;
    }
}
vector<string> FaceManager::getFaceListFromDB(string userId)
{
    vector<string> ret;
    vector<faceData> v_fd = readFaceDB();
    for(int i=0; i<v_fd.size(); i++)
    {
        if(!strncmp(v_fd[i].userID.c_str(), userId.c_str(), userId.size()))
        {
            std::cout << "Found Face List" << endl;
            ret = v_fd[i].faceNumber;
            for(int i=0; i<ret.size(); i++)
            {
                std::cout << "List : " << ret[i] << endl;
            }
            return ret;
        }
    }
    return ret; //return NULL
}

vector<faceData> FaceManager::readFaceDB()
{
    std::cout << "readFaceDB" << endl;
    vector<faceData> _facelist;
    
    faceData fd;
    string temp_line;
    string temp_uid;
    string temp_vid;
    
    ifstream face_db("./facelist"); //TODO : read from facedb
    ofstream face_list("./facelist_read");
    while(face_db.good())
    {
        getline(face_db, temp_line);
        if(temp_line.size() == 0)
            break;
        face_list << temp_line;
        face_list << "\n";  //Add new line
        std::string delimiter = " ";
        size_t pos = 0;
        bool is_uid = true;
        std::string token;
        
        while ((pos = temp_line.find(delimiter)) != std::string::npos) {
            token = temp_line.substr(0, pos);

            if(is_uid == true){
                temp_uid = token;
                fd.userID = temp_uid;
                is_uid = false; //First token is UID
            }
            else{
                temp_vid = token;
                fd.faceNumber.push_back(temp_vid);
            }
            temp_line.erase(0, pos + delimiter.length());
        }
        //read last token
        token = temp_line.substr(0, pos);
        temp_vid = token;
        fd.faceNumber.push_back(temp_vid);
        _facelist.push_back(fd);
        fd.faceNumber.clear();
    }
    face_db.close();
    face_list.close();
    return _facelist;
}