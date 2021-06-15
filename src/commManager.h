#ifndef _COMM_MANAGER_H
#define _COMM_MANAGER_H

#include <opencv2/core.hpp>
#include <pthread.h>
#include <queue>
#include "NetworkTCP.h"
#include "TcpSendRecvJpeg.h"
#include "TcpSendRecvCmd.h"
#include "MSG_DB.h"
#include "Payload.h"

class FaceManager;

enum class Command
{
    GET_FACES,
    ADD_FACE,
    LOGIN,
};

class CommandMessage
{
public:
    CommandMessage(Command _c) : cmd(_c) {}
    CommandMessage(Command _c, int _uid) : cmd(_c), uid(_uid) {}
    CommandMessage(Command _c, std::string userId, std::string password) {}
    Command cmd;
    std::string userId;
    std::string password;
    int uid;
};

class CommManager
{
private:
    int port;
    TTcpListenPort *TcpListenPort;
    TTcpConnectedPort *TcpConnectedPort;
    pthread_mutex_t sendMutex;

    FaceManager* lFaceManager;
    pthread_mutex_t recvMutex;
    std::queue<CommandMessage> commandQueue;

public:
    CommManager(int _port) : port(_port)
    {
        pthread_mutex_init(&sendMutex, NULL);
        pthread_mutex_init(&recvMutex, NULL);
    }
    bool connect();
    void disconnect();
    bool sendFace(cv::Mat &frame);
    bool sendFrame(cv::Mat &frame);
    bool sendRegisteredFace(cv::Mat &frame);
    bool sendLoginResp(bool result_ok);
    bool sendMessage();
    void receive();
    bool do_loop(FaceManager *faceManager);
    bool sendCommand(int cmd);
};

#endif // _COMM_MANAGER_H