#include <iostream>
#include <string>
#include <chrono>

#include "commManager.h"
#include "faceManager.h"
#include "userAuthManager.h"
#include "SecurityManager.h"

#include <termios.h>
#include <pthread.h>

void *listenFunc(void *arg);
void *listenSecureFunc(void *arg);
void *receiverFunc(void *arg);

int kbhit()
{
    struct timeval tv = {0L, 0L};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}

int getch()
{
    int r;
    unsigned char c;
    if ((r = read(0, &c, sizeof(c))) < 0)
    {
        return r;
    }
    else
    {
        return c;
    }
}
CommManager::CommManager(int _port, int _securePort, SecurityManager *securityManager) : port(_port), securePort(_securePort), isConnected(false)
{
    lSecurityManager = securityManager;
    pthread_mutex_init(&sendMutex, NULL);
    pthread_mutex_init(&recvMutex, NULL);

    pthread_mutex_init(&listenMutex, NULL);
    pthread_cond_init(&startConnect, NULL);
    pthread_cond_init(&connectedCond, NULL);

    openPort();

}

CommManager::~CommManager()
{    
    pthread_cancel(tidListen1);
    pthread_cancel(tidListen2);

    if (TcpListenPort != NULL)
    {
        CloseTcpListenPort(&TcpListenPort); // Close listen port
        TcpListenPort = NULL;
    }
    if (TcpListenPortSecured != NULL)
    {
        CloseTcpListenPort(&TcpListenPortSecured);
        TcpListenPortSecured = NULL;
    }    
}

int CommManager::openPort()
{
    if ((TcpListenPort = OpenTcpListenPort(port)) == NULL) // Open TCP Network port
    {
        printf("OpenTcpListenPortFailed\n");
        return -1;
    }

    if ((TcpListenPortSecured = OpenTcpListenPort(securePort)) == NULL) // Open TCP Network port
    {
        printf("OpenTcpListenPortFailed2\n");
        return -1;
    }

    int status;

    if ((status = pthread_create(&tidListen1, NULL, &listenFunc, (void *)this)) != 0)
    {
        std::cout << "thread1 create error" << std::endl;
        return -1;
    }
    if ((status = pthread_create(&tidListen2, NULL, &listenSecureFunc, (void *)this)) != 0)
    {
        std::cout << "thread2 create error" << std::endl;
        return -1;
    }        
}

bool CommManager::listen()
{
    pthread_mutex_lock(&listenMutex);
    pthread_cond_broadcast(&startConnect);
    pthread_mutex_unlock(&listenMutex);


    pthread_mutex_lock(&listenMutex);
    pthread_cond_wait(&connectedCond, &listenMutex);
    pthread_mutex_unlock(&listenMutex);

    printf(" Accepted connection Request\n");

    return true;
}

void CommManager::accept()
{
    struct sockaddr_in cli_addr;

    socklen_t clilen;

    while(true)
    {
        pthread_mutex_lock(&listenMutex);
        pthread_cond_wait(&startConnect, &listenMutex);
        pthread_mutex_unlock(&listenMutex);

        clilen = sizeof(cli_addr);

        printf("Listening for connections\n");

        if ((TcpConnectedPort = AcceptTcpConnection(TcpListenPort, &cli_addr, &clilen)) == NULL)
        {
            printf("AcceptTcpConnection Failed\n");
            continue;
        }


        pthread_mutex_lock(&listenMutex);
        pthread_cond_broadcast(&connectedCond);
        pthread_mutex_unlock(&listenMutex);

        printf("Accepted connection Request\n");
    }
}

void CommManager::acceptSecure()
{
    struct sockaddr_in cli_addr;

    socklen_t clilen;

    while(true)
    {
        pthread_mutex_lock(&listenMutex);
        pthread_cond_wait(&startConnect, &listenMutex);
        pthread_mutex_unlock(&listenMutex);

        clilen = sizeof(cli_addr);

        printf("Listening for connections2\n");

        if ((TcpConnectedPort = AcceptTcpConnection(TcpListenPortSecured, &cli_addr, &clilen)) == NULL)
        {
            printf("AcceptTcpConnection Failed2\n");
            continue;
        }
        //make secure connection
        if (TcpConnectedPort->ssl == nullptr) {
            TcpConnectedPort->ssl = lSecurityManager->getSecureNeworkContext();
            if (TcpConnectedPort->ssl == nullptr) {
                std::cerr << "fail to make secure network context" << std::endl;
            }
        }
        TcpConnectedPort->secureMode = true;
        lSecurityManager->setSecureNetwork(TcpConnectedPort->ssl, TcpConnectedPort->ConnectedFd);

        pthread_mutex_lock(&listenMutex);
        pthread_cond_broadcast(&connectedCond);
        pthread_mutex_unlock(&listenMutex);

        printf("Accepted connection Request2\n");
    }
}

bool CommManager::sendFrame(cv::Mat &frame)
{
    pthread_mutex_lock(&sendMutex);

    if (!sendCommand(SIGNAL_FM_RESP_VIDEO_FRAME))
    {
        pthread_mutex_unlock(&sendMutex);
        return false;
    }

    int ret = TcpSendImageAsJpeg(TcpConnectedPort, frame);
    pthread_mutex_unlock(&sendMutex);

    return ret >= 0;
}

bool CommManager::sendMatchedUser(std::string username)
{
    pthread_mutex_lock(&sendMutex);

    if (!sendCommand(SIGNAL_FM_RESP_USER_ATTEND, username))
    {
        pthread_mutex_unlock(&sendMutex);
        return false;
    }

    pthread_mutex_unlock(&sendMutex);
    return true;
}

bool CommManager::sendRegisteredFace(cv::Mat &frame)
{
    pthread_mutex_lock(&sendMutex);

    if (!sendCommand(SIGNAL_FM_RESP_GET_FACES))
    {
        pthread_mutex_unlock(&sendMutex);
        return false;
    }

    int ret = TcpSendImageAsJpeg(TcpConnectedPort, frame);
    pthread_mutex_unlock(&sendMutex);

    return ret >= 0;
}

bool CommManager::sendFace(cv::Mat &frame)
{
    pthread_mutex_lock(&sendMutex);

    if (!sendCommand(SIGNAL_FM_RESP_FACE_ADD))
    {
        pthread_mutex_unlock(&sendMutex);
        return false;
    }

    int ret = TcpSendImageAsJpeg(TcpConnectedPort, frame);
    pthread_mutex_unlock(&sendMutex);

    return ret >= 0;
}

bool CommManager::sendDeleteFaceResp()
{
    pthread_mutex_lock(&sendMutex);

    if (!sendCommand(SIGNAL_FM_RESP_FACE_DELETE))
    {
        pthread_mutex_unlock(&sendMutex);
        return false;
    }

    pthread_mutex_unlock(&sendMutex);
    return true;
}

bool CommManager::sendLoginResp(bool result_ok, int uid)
{
    pthread_mutex_lock(&sendMutex);

    if (result_ok)
    {
        if (uid == 0)
        {
            std::cout << "administrator login" << endl;
            if (!sendCommand(SIGNAL_FM_RESP_LOGIN_OK, uid, "admin"))
            {
                pthread_mutex_unlock(&sendMutex);
                return false;
            }
        }
        else
        {
            std::cout << "user login" << endl;
            if (!sendCommand(SIGNAL_FM_RESP_LOGIN_OK, uid))
            {
                pthread_mutex_unlock(&sendMutex);
                return false;
            }
        }
    }
    else
    {
        if (!sendCommand(SIGNAL_FM_RESP_LOGIN_FAILED))
        {
            pthread_mutex_unlock(&sendMutex);
            return false;
        }
    }

    pthread_mutex_unlock(&sendMutex);
    return true;
}

bool CommManager::sendStudentList(vector<struct UserData>& allUsers)
{
    vector<string> userIDs;
    for (struct UserData& user : allUsers)
    {
        userIDs.push_back(user.userID);
    }

    pthread_mutex_lock(&sendMutex);
    if (!sendCommand(SIGNAL_FM_RESP_STUDENT_LIST))
    {
        pthread_mutex_unlock(&sendMutex);
        return false;
    }

    if (TcpSendStringVector(TcpConnectedPort, userIDs) < 0)
    {
        pthread_mutex_unlock(&sendMutex);
        return false;
    }

    pthread_mutex_unlock(&sendMutex);
    return true;
}

void CommManager::disconnect()
{
    if (TcpConnectedPort != NULL)
    {
        if (TcpConnectedPort->ssl != nullptr)
        {
            //lSecurityManager->resetSecureNetwork(TcpConnectedPort->ssl);
            lSecurityManager->shutdownSecureNetwork(TcpConnectedPort->ssl);
            lSecurityManager->freeSecureNetworkContext(TcpConnectedPort->ssl);
            TcpConnectedPort->ssl = nullptr;
        }
        CloseTcpConnectedPort(&TcpConnectedPort); // Close network port;
        TcpConnectedPort = NULL;
    }
}

bool CommManager::do_loop(FaceManager *faceManager, UserAuthManager *userAuthManager)
{
    // loop over frames with inference
    int nbFrames = 0;
    auto globalTimeStart = chrono::steady_clock::now();

    int status;
    pthread_t tid;
    if ((status = pthread_create(&tid, NULL, &receiverFunc, (void *)this)) != 0)
    {
        std::cout << "thread create error" << std::endl;
        return false;
    }

    std::vector<struct UserData> allUsers = userAuthManager->getAllUsers();
    long cnt = 0;

    bool alive = true;
    while (alive)
    {
        if (!faceManager->processFrame())
        {
            continue;
        }

        if (kbhit())
        {
            // Stores the pressed key in ch
            char keyboard = getch();

            if (keyboard == 'q')
            {
                break;
            }
        }
        cnt++;

        pthread_mutex_lock(&recvMutex);
        if (!commandQueue.empty())
        {
            CommandMessage cmdMsg = commandQueue.front();
            // int param = commandQueue.front().second;
            commandQueue.pop();

            switch (cmdMsg.cmd)
            {
            case Command::GET_FACES:
            {
                string userID = "";
                for (struct UserData & ud : allUsers)
                {
                    if (ud.uid == cmdMsg.uid)
                    {
                        userID = ud.userID;
                        break;
                    }
                }
                if (userID == "")
                {
                    std::cout << "[ERR] no users. uid " << cmdMsg.uid << std::endl;
                    break;
                }

                // TODO: get uid from payload
                std::cout << "get-face" << std::endl;
                // int uid = param;
                faceManager->sendFaceImages(userID);
                break;
            }
            case Command::ADD_FACE:
            {
                // TODO: get uid/num of pictures from payload
                string userID = "";
                for (struct UserData & ud : allUsers)
                {
                    if (ud.uid == cmdMsg.uid)
                    {
                        userID = ud.userID;
                        break;
                    }
                }
                if (userID == "")
                {
                    std::cout << "[ERR] no users. uid " << cmdMsg.uid << std::endl;
                    break;
                }

                if (!faceManager->registerFace(userID, cmdMsg.n))
                {
                    std::cout << "[ERR] failed to Add face" << endl;
                }
                break;
            }
            case Command::DELETE_FACE:
            {
                string userID = cmdMsg.userID;
                string faceId = "LastFace"; //Delete last face
                if(!faceManager->deleteFaceDB(cmdMsg.userID,faceId))
                {
                    std::cout << "[ERR] failed to Delete face" << endl;
                }
                break;
            }
            case Command::LOGIN:
            {
                std::cout << "process LOGIN" << endl;
                string userid = cmdMsg.userID;
                string password = cmdMsg.password;
                bool loginResult = userAuthManager->verifyUser(userid, password);
                std::cout << "login result : " << loginResult << endl;

                // send response payload with login result
                int uid = userAuthManager->getCurrentUid();
                if (uid >= 0)
                {
                    // set uid to faceManager here
                    faceManager->setCurrentUid(to_string(uid));
                }
                sendLoginResp(loginResult, uid);
                break;
            }
            case Command::VIDEO:
            {
                std::cout << "control VIDEO " << cmdMsg.param << std::endl;
                faceManager->playVideo(cmdMsg.param);
                break;
            }
            case Command::CHANGE_VIDEO_SRC:
            {
                std::cout << "change video source " << cmdMsg.param << std::endl;
                faceManager->changeVideoSource(cmdMsg.param);
                break;
            }
            case Command::CHANGE_VIDEO_LIVE:
            {
                std::cout << "change video live" << std::endl;
                faceManager->changeVideoSourceLive();
                break;
            }
            case Command::GET_STUDENT_LIST:
            {
                std::cout << "get student list" << std::endl;
                vector<struct UserData> allUsers = userAuthManager->getAllUsers();
                
                sendStudentList(allUsers);
                break;
            }
            case Command::DISCONNECT:
            {
                std::cout << "disconnect" << std::endl;
                alive = false;
                break;
            }
            default:
                break;
            }
        }
        pthread_mutex_unlock(&recvMutex);

#ifdef LOG_TIMES
        // std::cout << "mtCNN took " << std::chrono::duration_cast<chrono::milliseconds>(endMTCNN - startMTCNN).count() << "ms\n";
        // std::cout << "Forward took " << std::chrono::duration_cast<chrono::milliseconds>(endForward - startForward).count() << "ms\n";
        // std::cout << "Feature matching took " << std::chrono::duration_cast<chrono::milliseconds>(endFeatM - startFeatM).count() << "ms\n\n";
#endif // LOG_TIMES
    }

    pthread_cancel(tid);

    auto globalTimeEnd = chrono::steady_clock::now();

    auto milliseconds = chrono::duration_cast<chrono::milliseconds>(globalTimeEnd - globalTimeStart).count();
    double seconds = double(milliseconds) / 1000.;
    double fps = nbFrames / seconds;

    std::cout << "Counted " << nbFrames << " frames in " << double(milliseconds) / 1000. << " seconds!"
              << " This equals " << fps << "fps.\n";

    return true;
}

void *listenFunc(void *arg)
{
    CommManager *commManager = (CommManager *)arg;

    commManager->accept();

    return NULL;
}

void *listenSecureFunc(void *arg)
{
    CommManager *commManager = (CommManager *)arg;

    commManager->acceptSecure();

    return NULL;
}

void *receiverFunc(void *arg)
{
    CommManager *commManager = (CommManager *)arg;

    commManager->receive();

    return NULL;
}


void CommManager::receive()
{
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    TTcpConnectedPort *connection = TcpConnectedPort;

    SerializablePayload payload;

    while (true)
    {
        std::cout << "wait cmd" << std::endl;

        if (!TcpRecvObject(connection, &payload))
        {
            std::cout << "failed to receive payload" << std::endl;
            continue;
        }

        std::cout << "payload data_id: " << payload.data_id << std::endl;

        switch (payload.data_id)
        {
        case SIGNAL_FM_REQ_GET_FACES:
        {
            std::cout << "SIGNAL_FM_REQ_GET_FACES" << endl;
            pthread_mutex_lock(&recvMutex);
            int uid = std::stoi(payload.str1);
            commandQueue.push(CommandMessage(Command::GET_FACES, uid));
            pthread_mutex_unlock(&recvMutex);
            break;
        }
        case SIGNAL_FM_REQ_FACE_ADD:
        {
            std::cout << "SIGNAL_FM_REQ_FACE_ADD" << endl;
            pthread_mutex_lock(&recvMutex);
            int uid = std::stoi(payload.str1);
            commandQueue.push(CommandMessage(Command::ADD_FACE, uid, payload.i1));
            pthread_mutex_unlock(&recvMutex);
            break;
        }
        case SIGNAL_FM_REQ_FACE_DELETE:
            std::cout << "SIGNAL_FM_REQ_FACE_DELETE" << std::endl;
            pthread_mutex_lock(&recvMutex);
            commandQueue.push(CommandMessage(Command::DELETE_FACE, payload.str1));
            pthread_mutex_unlock(&recvMutex);
            break;
        case SIGNAL_FM_REQ_LOGIN:
        {
            std::cout << "SIGNAL_FM_REQ_LOGIN" << endl;
            pthread_mutex_lock(&recvMutex);
            commandQueue.push(CommandMessage(Command::LOGIN, payload.str1, payload.str2));
            pthread_mutex_unlock(&recvMutex);
            break;
        }
        case SIGNAL_FM_REQ_VIDEO_START:
        {
            std::cout << "SIGNAL_FM_REQ_VIDEO_START" << endl;
            pthread_mutex_lock(&recvMutex);
            commandQueue.push(CommandMessage(Command::VIDEO, "start"));
            pthread_mutex_unlock(&recvMutex);
            break;
        }
        case SIGNAL_FM_REQ_VIDEO_END:
        {
            std::cout << "SIGNAL_FM_REQ_VIDEO_END" << endl;
            pthread_mutex_lock(&recvMutex);
            commandQueue.push(CommandMessage(Command::VIDEO, "end"));
            pthread_mutex_unlock(&recvMutex);
            break;
        }
        case SIGNAL_FM_REQ_DISCONNECT:
        {
            std::cout << "SIGNAL_FM_REQ_DISCONNECT" << std::endl;
            pthread_mutex_lock(&recvMutex);
            commandQueue.push(CommandMessage(Command::DISCONNECT));
            pthread_mutex_unlock(&recvMutex);
            break;
        }
        case SIGNAL_FM_REQ_VIDEO_RECORD:
        {
            std::cout << "SIGNAL_FM_REQ_VIDEO_RECORD" << std::endl;
            pthread_mutex_lock(&recvMutex);
            commandQueue.push(CommandMessage(Command::CHANGE_VIDEO_SRC, payload.str1));
            pthread_mutex_unlock(&recvMutex);
            break;
        }
        case SIGNAL_FM_REQ_VIDEO_LIVE:
        {
            std::cout << "SIGNAL_FM_REQ_VIDEO_LIVE" << std::endl;
            pthread_mutex_lock(&recvMutex);
            commandQueue.push(CommandMessage(Command::CHANGE_VIDEO_LIVE));
            pthread_mutex_unlock(&recvMutex);
            break;
        }
        case SIGNAL_FM_REQ_STUDENT_LIST:
        {
            std::cout << "SIGNAL_FM_REQ_STUDENT_LIST" << std::endl;
            pthread_mutex_lock(&recvMutex);
            commandQueue.push(CommandMessage(Command::GET_STUDENT_LIST));
            pthread_mutex_unlock(&recvMutex);
            break;
        }
        default:
            break;
        }
    }
    return;
}

bool CommManager::sendCommand(int cmd)
{
    SerializablePayload payload;
    payload.data_id = cmd;

    return TcpSendObject(TcpConnectedPort, &payload) >= 0;
}
bool CommManager::sendCommand(int cmd, std::string str)
{
    SerializablePayload payload;
    payload.data_id = cmd;
    payload.str1 = str;

    return TcpSendObject(TcpConnectedPort, &payload) >= 0;
}

bool CommManager::sendCommand(int cmd, std::string str1, std::string str2)
{
    SerializablePayload payload;
    payload.data_id = cmd;
    payload.str1 = str1;
    payload.str2 = str2;

    return TcpSendObject(TcpConnectedPort, &payload) >= 0;
}

bool CommManager::sendCommand(int cmd, int v)
{
    SerializablePayload payload;
    payload.data_id = cmd;
    payload.i1 = v;

    return TcpSendObject(TcpConnectedPort, &payload) >= 0;
}

bool CommManager::sendCommand(int cmd, int v, std::string str)
{
    SerializablePayload payload;
    payload.data_id = cmd;
    payload.i1 = v;
    payload.str1 = str;

    return TcpSendObject(TcpConnectedPort, &payload) >= 0;
}
