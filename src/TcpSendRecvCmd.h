#include "NetworkTCP.h"
#include "commManager.h"

class Payload;

int TcpSendCommand(TTcpConnectedPort * TcpConnectedPort, Payload* payload);
bool TcpRecvCommand(TTcpConnectedPort * TcpConnectedPort, Payload* payload);

