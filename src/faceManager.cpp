#include "faceManager.h"
#include <opencv2/core/cuda.hpp>
#include <opencv2/cudawarping.hpp>
#include <iostream>
#include <cstring>
#include <string>

FaceManager::FaceManager(CommManager *comm, SecurityManager *securityManager) : useCamera(true), rotate180(true), commManager(comm), lSecurityManager(securityManager)
{
    bool isCSICam = true;
    // init opencv stuff
    videoStreamer = new VideoStreamer(0, videoFrameWidth, videoFrameHeight, 60, isCSICam);
}

FaceManager::FaceManager(CommManager *comm, const char *filename, SecurityManager *securityManager) : useCamera(false), rotate180(false), commManager(comm), lSecurityManager(securityManager)
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
    delete videoStreamer;
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

    maxFacesPerScene = 8;
    float knownPersonThreshold = 1.;

    // init facenet
    faceNet = new FaceNetClassifier(gLogger, dtype, uffFile, engineFile, batchSize, serializeEngine,
                                    knownPersonThreshold, maxFacesPerScene, videoFrameWidth, videoFrameHeight);

    // init mtCNN
    mtCNN = new mtcnn(videoFrameHeight, videoFrameWidth);

    faceDB = readFaceDB();

    if(loadFaceNet() == true)
        return true;
}

bool FaceManager::loadFaceNet()
{
    std::cout << "[FaceManager] loadFaceNet" << std::endl;
    if(faceNet != NULL && mtCNN != NULL)
    {
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
    return false;
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
        std::cout << "[FaceManager]Empty frame! Exiting...\n Try restarting nvargus-daemon by "
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

        // Put faceId with current timestamp
        auto now = std::chrono::system_clock::now();
        string timestamp = to_string(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());
        string faceId = timestamp;
        faceId.erase(faceId.begin(),faceId.begin()+6);  //openCV vulnerbility (fileName should be ascii)

        for(int i=0; i<faceDB.size(); i++)
        {
            if(!strncmp(faceDB[i].userId.c_str(), userId.c_str(), userId.size()))
            {
                int size = faceDB[i].faceId.size();
                faceId += to_string(size);
            }
        }

        while (!faceDetected && cnt < 600)
        {
            videoStreamer->getFrame(frame);

            // TODO: what if frame is empty??
            if (frame.empty())
            {
                std::cout << "[FaceManager]frame is empty" << std::endl;
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
            faceDetected = faceNet->addNewFace(frame, outputBbox, croppedFace, userId, faceId);
            cnt++;
        }

        frame.release();
        for (int j=0; j< 60; j++)   //TODO : check
        {
            videoStreamer->getFrame(frame);
            frame.release();
        }

        if (!faceDetected) // timeout
        {
            std::cout << "[FaceManager]no face detected in camera." << std::endl;
            return false;
        }

        if (!commManager->sendFace(croppedFace))
            return false;

        croppedFace.release();

        std::cout << "[FaceManager] faceId Created with userId_timestamp = " << faceId << std::endl;
        if (!addFaceDB(userId,faceId))
            return false;

        if (!loadFaceNet())
            return false;
    }

    // frame.release();
    return true;
}

void FaceManager::sendFaceImages(string userId)
{
    std::cout << "[FaceManager] sendFaceImages : " << userId << endl;
    vector<string> face_list = getFaceListFromDB(userId);   //Read facelist from FaceDB
    string imagepath = "../imgs";
    
    std::vector<struct Paths> paths;
    cv::Mat image;
    int len = face_list.size();
    // if (len > 2) len = 2;   // TODO: limited for test
    for (int i = 0; i < len; i++)
    {
        string absPath = imagepath + "/" + face_list[i] + ".jpg";
        std::cout << "[FaceManager] loading " << i << " " << absPath << std::endl;
        loadInputImage(absPath, image, videoFrameWidth, videoFrameHeight);
        commManager->sendRegisteredFace(image);
    }
}

void FaceManager::stop()
{
}

void FaceManager::changeVideoSource(string filename)
{
    videoStreamer->release();
    delete videoStreamer;

    videoStreamer = new VideoStreamer("../" + filename, videoFrameWidth, videoFrameHeight);
    useCamera = false;
    rotate180 = false;
}

void FaceManager::changeVideoSourceLive()
{
    videoStreamer->release();
    delete videoStreamer;

    bool isCSICam = true;
    videoStreamer = new VideoStreamer(0, videoFrameWidth, videoFrameHeight, 60, isCSICam);
    useCamera = true;
    rotate180 = true;
}

bool FaceManager::deleteFaceDB(string userId, string faceId)
{
    std::cout << "[FaceManager] deleteFace: " << userId << " / " << faceId << endl;
    for(int i=0; i<faceDB.size(); i++)
    {
        if(!strncmp(faceDB[i].userId.c_str(), userId.c_str(), userId.size()))
        {
            for(int j=0; j<faceDB[i].faceId.size(); j++)
            {
                std::cout << "[FaceManager] Find userId : " << userId << " faceId : " << faceId << " Delete." << endl;
                faceDB[i].faceId.erase(faceDB[i].faceId.begin()+j);
            }
        }
    }
    saveFaceDB(); //TODO : check return value
    /*if (!saveFaceDB())
        return false;*/
    // TODO: Serialize FaceDB
    if (!loadFaceNet())
        return false;
    return true;
}

bool FaceManager::saveFaceDB()
{
    std::cout << "[FaceManager] saveFaceDB" << endl;
    char *buf = new char[SerializableP<vector<FaceData>>::serialize_size(faceDB)];
    SerializableP<vector<FaceData>>::serialize(buf, faceDB);

    size_t dataSize = SerializableP<vector<FaceData>>::serialize_size(faceDB);
    size_t writeLen;
    bool ret = false;

    cout << "dataSize = " << dataSize << endl;

    ret = lSecurityManager->writeFaceDB((unsigned char*)buf, dataSize, &writeLen);
    cout << "ret = " << ret << endl;
    cout << "writeLen = " << writeLen << endl;

    delete [] buf;
	return ret;
}

bool FaceManager::addFaceDB(string userId, string faceId)
{
    std::cout << "[FaceManager] addFaceDB: " << userId << " / " << faceId << endl;
    bool user_found = false;
    for(int i=0; i<faceDB.size(); i++)
    {
        if(!strncmp(faceDB[i].userId.c_str(), userId.c_str(), userId.size()))
        {
            std::cout << "[FaceManager] Find userId : " << userId << " Add : " << faceId << endl;
            faceDB[i].faceId.push_back(faceId);
            user_found = true;
            break;
        }
    }
    if(!user_found)
    {
        std::cout << "[FaceManager] add New User: " << userId << endl;
        FaceData fd;
        fd.userId = userId;
        fd.faceId.push_back(faceId);
        faceDB.push_back(fd);
        for(int i=0; i<faceDB.size(); i++)
        {
            std::cout << "[FaceManager] faceDB.userId " << faceDB[i].userId << endl;
            for(int j=0; j<faceDB[i].faceId.size(); j++)
            {
                std::cout << "[FaceManager] faceDB.faceId " << faceDB[i].faceId[j] << endl;
            }
        }
    }
    saveFaceDB(); //TODO : check return value
    /*if (!saveFaceDB())
        return false;*/
    //TODO: Serialize FaceDB
    if (!loadFaceNet())
        return false;
    return true;
}

bool FaceManager::findUserFromDB(string userId)
{
    std::cout << "[FaceManager] findUserFromDB: " << userId << endl;

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
    // vector<FaceData> v_fd = readFaceDB();
    for(int i=0; i<faceDB.size(); i++)
    {
        if(!strncmp(faceDB[i].userId.c_str(), userId.c_str(), userId.size()))
        {
            std::cout << "[FaceManager] Found Face List" << endl;
            ret = faceDB[i].faceId;
            for(int i=0; i<ret.size(); i++) //DEBUG
            {
                std::cout << "[FaceManager] List : " << ret[i] << endl;
            }
            return ret;
        }
    }
    return ret; //return NULL
}

vector<FaceData>& FaceManager::readFaceDB()
{
    std::cout << "[FaceManager] readFaceDB" << endl;
    
    size_t readSize;
    size_t readLen;

    readSize = lSecurityManager->getSizeFaceDB();

    if (readSize == 0)
    {
        // create empty file and return;
        std::cout << "[FaceManager] file is empty. Initilizing..." << std::endl;
        saveFaceDB();
        return faceDB;
    }
    unsigned char* readData = new unsigned char[readSize];

    int ret = lSecurityManager->readFaceDB(readData, readSize, &readLen);
    if (ret < 0) {
        cout << "[FaceManager] failed to read faceDB" << endl;
        return faceDB;
    }

    std::cout << "[FaceManager] readSize : " << readSize << " readLen : " << readLen << endl;

    //TODO: deserialize from DB file to faceDB
    SerializableP<vector<FaceData>>::deserialize(reinterpret_cast<const char*>(readData), faceDB);
    /*unsigned char* pbuf = readData;
    for (int i=0; i<1; i++)
    {
        // SerializableP<vector<FaceData>>::deserialize(reinterpret_cast<const char*>(readData), faceDB[i]);
        // faceDB[i].deserialize(reinterpret_cast<const char*>(readData));
        faceDB[i].deserialize((const char*)pbuf);
    }*/
    
    delete [] readData;
    return faceDB;
}