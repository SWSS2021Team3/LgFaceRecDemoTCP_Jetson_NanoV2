#include "commManager.h"

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