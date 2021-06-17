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
#include "SecurityManager.h"
#include "userAuthManager.h"

class FaceManager;

enum class Command
{
    GET_FACES,
    ADD_FACE,
    DELETE_FACE,
    LOGIN,
    VIDEO,
    DISCONNECT,
    CHANGE_VIDEO_SRC,
    CHANGE_VIDEO_LIVE
};

class CommandMessage
{
public:
    CommandMessage(Command _c) : cmd(_c) {}
    CommandMessage(Command _c, int _uid) : cmd(_c), uid(_uid) {}
    CommandMessage(Command _c, int _uid, int _n) : cmd(_c), uid(_uid), n(_n) {}
    CommandMessage(Command _c, std::string _userId, std::string _password)
        : cmd(_c), userID(_userId), password(_password) {}
    CommandMessage(Command _c, std::string _param) : cmd(_c), param(_param), userID(_param) {}
    Command cmd;
    std::string userID;
    std::string password;
    int uid;
    std::string param;
    int n;
};

class CommManager
{
private:
    int port;
    int securePort;
    TTcpListenPort *TcpListenPort = NULL;
    TTcpListenPort *TcpListenPortSecured = NULL;

    pthread_mutex_t listenMutex;
    pthread_cond_t startConnect;
    pthread_cond_t connectedCond;
    pthread_t tidListen1;
    pthread_t tidListen2;
    bool isConnected;

    TTcpConnectedPort *TcpConnectedPort;
    pthread_mutex_t sendMutex;

    SecurityManager* lSecurityManager;
    pthread_mutex_t recvMutex;
    std::queue<CommandMessage> commandQueue;

    int openPort();

public:
    CommManager(int _port, int _securePort, SecurityManager *securityManager);
    ~CommManager();
    bool open();
    bool listen();
    bool connect();
    void accept();
    void acceptSecure();
    void disconnect();
    bool sendFace(cv::Mat &frame);
    bool sendFrame(cv::Mat &frame);
    bool sendMatchedUser(std::string username);
    bool sendRegisteredFace(cv::Mat &frame);
    bool sendLoginResp(bool result_ok, int uid);
    bool sendDeleteFaceResp();
    void receive();
    bool do_loop(FaceManager *faceManager, UserAuthManager *userAuthManager);
    bool sendCommand(int cmd);
    bool sendCommand(int cmd, std::string str);
    bool sendCommand(int cmd, std::string str1, std::string str2);
    bool sendCommand(int cmd, int v);
    bool sendCommand(int cmd, int v, std::string str);
};

#endif // _COMM_MANAGER_H
