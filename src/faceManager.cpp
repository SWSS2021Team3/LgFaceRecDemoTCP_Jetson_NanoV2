#include "faceManager.h"
#include <opencv2/core/cuda.hpp>
#include <opencv2/cudawarping.hpp>

FaceManager::FaceManager(CommManager *comm) : useCamera(true), rotate180(true), commManager(comm)
{
    bool isCSICam = true;

    // init opencv stuff
    videoStreamer = new VideoStreamer(0, videoFrameWidth, videoFrameHeight, 60, isCSICam);
}

FaceManager::FaceManager(CommManager *comm, const char *filename) : useCamera(false), rotate180(false), commManager(comm)
{
    // init opencv stuff
    videoStreamer = new VideoStreamer(filename, videoFrameWidth, videoFrameHeight);
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
    faceNet->featureMatching(frame);
    // auto endFeatM = chrono::steady_clock::now();
    faceNet->resetVariables();

    if (!commManager->sendFrame(frame))
        return false;

    outputBbox.clear();
    // frame.release();

    return true;
}

bool FaceManager::registerFace()
{
    cv::Mat frame;
    cv::Mat croppedFace;

    cv::Mat dst_img;
    std::vector<struct Bbox> outputBbox;

    videoStreamer->getFrame(frame);
    // Create a destination to paint the source into.
    dst_img.create(frame.size(), frame.type());

    // Push the images into the GPU
    if (rotate180)
        rotateFrame(frame);

    outputBbox = mtCNN->findFace(frame);
    // if (!commManager->sendFrame(frame))
    //     return false;

    //cv::imshow("VideoSource", frame);
    faceNet->addNewFace(frame, outputBbox, croppedFace);

    if (!commManager->sendFace(croppedFace))
        return false;

    // frame.release();
    return true;
}

void FaceManager::sendFaceImages(int userId)
{
    // TODO: read user's images
    std::vector<struct Paths> paths;
    cv::Mat image;
    getFilePaths("../imgs", paths);
    int len = paths.size();
    if (len > 5) len = 5;
    for (int i = 0; i < len; i++)
    {
        std::cout << "loading " << i << " " << paths[i].absPath << std::endl;
        loadInputImage(paths[i].absPath, image, videoFrameWidth, videoFrameHeight);
        commManager->sendRegisteredFace(image);
    }
}

void FaceManager::stop()
{
}

bool FaceManager::deleteFace(int uid, int pNum)
{
    std::cout << "deleteFace: " << uid << " / " << pNum << endl;
    // TODO
    // 1.delete face DB
    // uid -> student name

    // 2.update AI handler
}

void FaceManager::setCurrentUid(int uid) {
    if (uid >= 0) {
        mCurrentUid = uid;
    }
}
