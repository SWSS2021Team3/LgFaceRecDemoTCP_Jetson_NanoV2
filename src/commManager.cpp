#include <iostream>
#include <string>
#include <chrono>

#include "commManager.h"
#include "faceManager.h"
#include "userAuthManager.h"

#include <termios.h>
#include <pthread.h>

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

bool CommManager::connect()
{
    struct sockaddr_in cli_addr;

    socklen_t clilen;

    if ((TcpListenPort = OpenTcpListenPort(port)) == NULL) // Open TCP Network port
    {
        printf("OpenTcpListenPortFailed\n");
        return false;
    }

    clilen = sizeof(cli_addr);

    printf("Listening for connections\n");

    if ((TcpConnectedPort = AcceptTcpConnection(TcpListenPort, &cli_addr, &clilen)) == NULL)
    {
        printf("AcceptTcpConnection Failed\n");
        return false;
    }

    printf("Accepted connection Request\n");

    return true;
}

bool CommManager::sendMessage()
{
    Payload *p = new Payload();
    p->data_id = 0x1001;
    p->data_length = 0;
    // p->data = NULL;
    return true;
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

void CommManager::disconnect()
{
    CloseTcpConnectedPort(&TcpConnectedPort); // Close network port;
    CloseTcpListenPort(&TcpListenPort);       // Close listen port
}

bool CommManager::do_loop(FaceManager *faceManager)
{
    // loop over frames with inference
    int nbFrames = 0;
    auto globalTimeStart = chrono::steady_clock::now();
    Payload *payload = NULL;
    UserAuthManager* userAuthManager = new UserAuthManager();

    lFaceManager = faceManager;
    int status;
    pthread_t tid;
    if ((status = pthread_create(&tid, NULL, &receiverFunc, (void *)this)) != 0)
    {
        std::cout << "thread create error" << std::endl;
        return (-1);
    }
    long cnt = 0;

    while (true)
    {
        // if (cnt == 1000000) std::cout << "start processFrame" << std::endl;
        // if (cnt > 1000000)
        // {
        //     if (!faceManager->processFrame())
        //         break;
        // }

        // nbFrames++;

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
            Command cmd = commandQueue.front().first;
            int param = commandQueue.front().second;
            commandQueue.pop();

            switch (cmd)
            {
            case Command::GET_FACES:
            {
                std::cout << "get-face" << std::endl;
                int uid = param;
                faceManager->sendFaceImages(uid);
                break;
            }
            case Command::ADD_FACE:
            {
                // TODO: get num of pictures from payload
                if (!faceManager->registerFace())
                {
                    std::cout << "[ERR] failed to register face" << endl;
                }
                break;
            }
            default:
                break;
            }
        }
        pthread_mutex_unlock(&recvMutex);

        pthread_mutex_lock(&sendMutex);
        if (!sendCommand(SIGNAL_FM_BASE)) // PING
        {
            std::cout << " failed to send command" << endl;
            pthread_mutex_unlock(&sendMutex);
            break;
        }
        pthread_mutex_unlock(&sendMutex);
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

    delete userAuthManager;
    return true;
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

    Payload payload;

    while (true)
    {
        std::cout << "wait cmd" << std::endl;

        if (!TcpRecvCommand(connection, &payload))
        {
            std::cout << "failed to receive payload" << std::endl;
            continue;
        }

        std::cout << "payload data_id: " << payload.data_id << std::endl;

        switch (payload.data_id)
        {
        case SIGNAL_FM_REQ_GET_FACES:
            std::cout << "SIGNAL_FM_REQ_GET_FACES" << endl;
            pthread_mutex_lock(&recvMutex);
            commandQueue.push(std::make_pair(Command::GET_FACES, 11));  // TODO: user id
            pthread_mutex_unlock(&recvMutex);
            break;
        case SIGNAL_FM_REQ_FACE_ADD:
        {
            std::cout << "SIGNAL_FM_REQ_FACE_ADD" << endl;
            pthread_mutex_lock(&recvMutex);
            commandQueue.push(std::make_pair(Command::ADD_FACE, 11));
            pthread_mutex_unlock(&recvMutex);
            break;
        }
        case SIGNAL_FM_REQ_FACE_DELETE:
            std::cout << "SIGNAL_FM_REQ_FACE_DELETE" << std::endl;
            break;
            //case SIGNAL_FM_REQ_LOGIN:
            //{
            //	std::cout << "SIGNAL_FM_REQ_LOGIN" << std::endl;
            //	 TODO: get login data from payload
            //	string userid, passwd;
            //	bool loginResult = userAuthManager->verifyUser(userid, passwd);
            //	std::cout << "login result : " << loginResult << endl;
            //	break;
            //}

        default:
            break;
        }
    }
    return;
}

bool CommManager::sendCommand(int cmd)
{
    Payload payload;
    payload.data_id = cmd;
    payload.data_length = 0;

    int ret = TcpSendCommand(TcpConnectedPort, &payload);
    if (ret < 0)
    {
        std::cout << "failed to send command" << endl;
        return false;
    }

    return true;
}
