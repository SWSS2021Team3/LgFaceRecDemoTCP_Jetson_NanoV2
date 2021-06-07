#include <opencv2/core.hpp>
#include "NetworkTCP.h"
#include "TcpSendRecvJpeg.h"

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
    bool sendFrame(cv::Mat &frame);
};
