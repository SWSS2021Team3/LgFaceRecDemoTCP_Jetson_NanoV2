#include <iostream>
#include <string>
#include <chrono>

#include "commManager.h"
#include "faceManager.h"
#include "userAuthManager.h"

#include <termios.h>

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
    Payload* payload = createCmdPacket(SIGNAL_FM_RESP_GET_FACES);
    if (payload == NULL) {
        std::cout << "unable to create a payload" << endl;
        return false;
    }
    int ret = TcpSendCommand(TcpConnectedPort, payload);
    if (ret < 0) {
        std::cout << "failed to send command" << endl;
        return false;
    }
    return TcpSendImageAsJpeg(TcpConnectedPort, frame) >= 0;
}

bool CommManager::sendFace(cv::Mat &frame)
{
    Payload* payload = createCmdPacket(SIGNAL_FM_RESP_FACE_ADD);
    if (payload == NULL) {
        std::cout << "unable to create a payload" << endl;
        return false;
    }
    int ret = TcpSendCommand(TcpConnectedPort, payload);
    if (ret < 0) {
        std::cout << "failed to send command" << endl;
        return false;
    }
    return TcpSendImageAsJpeg(TcpConnectedPort, frame) >= 0;
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
    Payload* payload = NULL;
    UserAuthManager* userAuthManager = new UserAuthManager();

    while (true)
    {
        if (!TcpRecvCommand(TcpConnectedPort, payload)) {
            std::cout << "failed to receive payload" << endl;
            continue;
        }
        if (payload == NULL) {
            std::cout << "command payload is null" << endl;
            continue;
        }
        std::cout << "payload data_id: " << payload->data_id << endl;

        if (!faceManager->processFrame())
            break;

        //cv::imshow("VideoSource", frame);
        nbFrames++;

        if (payload->data_id == SIGNAL_FM_REQ_FACE_ADD) {
            std::cout << "SIGNAL_FM_REQ_FACE_ADD" << endl;
            // TODO: get num of pictures from payload
            auto dTimeStart = chrono::steady_clock::now();
            if (!faceManager->registerFace()) {
                std::cout << "[ERR] failed to register face" << endl;
                break;
            }
            auto dTimeEnd = chrono::steady_clock::now();
            globalTimeStart += (dTimeEnd - dTimeStart);
        }
        else if (payload->data_id == SIGNAL_FM_REQ_FACE_DELETE) {
            std::cout << "SIGNAL_FM_REQ_FACE_DELETE" << endl;

        }
        else if (payload->data_id == SIGNAL_FM_REQ_LOGIN) {
            std::cout << "SIGNAL_FM_REQ_LOGIN" << endl;
            // TODO: get login data from payload
            string userid, passwd;
            bool loginResult = userAuthManager->verifyUser(userid, passwd);
            std::cout << "login result : " << loginResult << endl;
            // TODO: send response payload with login result
        }
        if (kbhit())
        {
            // Stores the pressed key in ch
            char keyboard = getch();

            if (keyboard == 'q')
            {
                delete userAuthManager;
                return false;
            }
            /*
            else if (keyboard == 'n')
            {

                auto dTimeStart = chrono::steady_clock::now();
                if (!faceManager->registerFace())
                    break;
                auto dTimeEnd = chrono::steady_clock::now();
                globalTimeStart += (dTimeEnd - dTimeStart);
            }
            */
        }

#ifdef LOG_TIMES
        // std::cout << "mtCNN took " << std::chrono::duration_cast<chrono::milliseconds>(endMTCNN - startMTCNN).count() << "ms\n";
        // std::cout << "Forward took " << std::chrono::duration_cast<chrono::milliseconds>(endForward - startForward).count() << "ms\n";
        // std::cout << "Feature matching took " << std::chrono::duration_cast<chrono::milliseconds>(endFeatM - startFeatM).count() << "ms\n\n";
#endif // LOG_TIMES
    }

    auto globalTimeEnd = chrono::steady_clock::now();

    auto milliseconds = chrono::duration_cast<chrono::milliseconds>(globalTimeEnd - globalTimeStart).count();
    double seconds = double(milliseconds) / 1000.;
    double fps = nbFrames / seconds;

    std::cout << "Counted " << nbFrames << " frames in " << double(milliseconds) / 1000. << " seconds!"
              << " This equals " << fps << "fps.\n";

    delete userAuthManager;
    return true;
}

bool CommManager::receiveMessage()
{
    bool ret = false;

    // Payload
    return ret;
}

Payload* CommManager::createCmdPacket(int cmd)
{
    Payload* payload = new Payload();
    payload->data_id = cmd;
    payload->data_length = 0;
    // payload->data = NULL; // TODO: set to NULL
    return payload;
}
