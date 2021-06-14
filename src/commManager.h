#ifndef _COMM_MANAGER_H
#define _COMM_MANAGER_H

#include <opencv2/core.hpp>
#include <pthread.h>
#include "NetworkTCP.h"
#include "TcpSendRecvJpeg.h"
#include "TcpSendRecvCmd.h"
#include "MSG_DB.h"

class FaceManager;
class Payload;

class CommManager
{
private:
    int port;
    TTcpListenPort *TcpListenPort;
    TTcpConnectedPort *TcpConnectedPort;
    pthread_mutex_t sendMutex;

    FaceManager* lFaceManager;

public:
    CommManager(int _port) : port(_port)
    {
        pthread_mutex_init(&sendMutex, NULL);
    }
    bool connect();
    void disconnect();
    bool sendFace(cv::Mat &frame);
    bool sendFrame(cv::Mat &frame);
    bool sendMessage();
    void receive();
    bool do_loop(FaceManager *faceManager);
    bool sendCommand(int cmd);
};

class Payload
{
public:
    uint16_t data_id;
    uint16_t data_length;
    std::vector<uchar> data;
};

#endif // _COMM_MANAGER_H