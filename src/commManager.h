#ifndef _COMM_MANAGER_H
#define _COMM_MANAGER_H

#include <opencv2/core.hpp>
#include "NetworkTCP.h"
#include "TcpSendRecvJpeg.h"
#include "MSG_DB.h"

class FaceManager;
class Payload;

class CommManager
{
private:
    int port;
    TTcpListenPort *TcpListenPort;
    TTcpConnectedPort *TcpConnectedPort;

public:
    CommManager(int _port) : port(_port) {}
    bool connect();
    void disconnect();
    bool sendFace(cv::Mat &frame);
    bool sendFrame(cv::Mat &frame);
    bool sendMessage();
    bool receiveMessage();
    bool do_loop(FaceManager *faceManager);
};

class Payload
{
public:
    uint16_t data_id;
    uint16_t data_length;
    std::vector<uchar> data;
};

#endif // _COMM_MANAGER_H