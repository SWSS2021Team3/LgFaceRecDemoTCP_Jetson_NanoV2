#ifndef _FACE_MANAGER_H
#define _FACE_MANAGER_H
#include <opencv2/core.hpp>
#include "faceNet.h"
#include "mtcnn.h"

#include "commManager.h"
#include "videoStreamer.h"

class FaceManager
{
private:
    CommManager *commManager;

    // const int videoFrameWidth = 1280;
    // const int videoFrameHeight = 720;
    const int videoFrameWidth = 640;
    const int videoFrameHeight = 480;

    VideoStreamer *videoStreamer;
    bool useCamera;
    bool rotate180;

    cv::VideoWriter writer;

    FaceNetClassifier *faceNet;
    mtcnn *mtCNN;

    void rotateFrame(cv::Mat &frame);
    void openRecordFile(const char *filename);
    void record(cv::Mat &frame);

public:
    FaceManager(CommManager *comm);
    FaceManager(CommManager *comm, const char *filename);
    ~FaceManager();
    bool init();
    void start();
    bool processFrame();
    bool registerFace();
    void stop();
};

#endif // _FACE_MANAGER_H