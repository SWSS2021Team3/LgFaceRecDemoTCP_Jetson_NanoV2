#include <iostream>
#include <string>
#include <chrono>

#include "commManager.h"
#include "faceManager.h"

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
    return TcpSendImageAsJpeg(TcpConnectedPort, frame) >= 0;
}

bool CommManager::sendFace(cv::Mat &frame)
{
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

    while (true)
    {
        if (!faceManager->processFrame())
            break;

        //cv::imshow("VideoSource", frame);
        nbFrames++;

        if (kbhit())
        {
            // Stores the pressed key in ch
            char keyboard = getch();

            if (keyboard == 'q')
            {
                return false;
            }
            else if (keyboard == 'n')
            {

                auto dTimeStart = chrono::steady_clock::now();
                if (!faceManager->registerFace())
                    break;
                auto dTimeEnd = chrono::steady_clock::now();
                globalTimeStart += (dTimeEnd - dTimeStart);
            }
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

    return true;
}

bool CommManager::receiveMessage()
{
    bool ret = false;

    // Payload
    return ret;
}