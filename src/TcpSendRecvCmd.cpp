#include "TcpSendRecvCmd.h"

int TcpSendCommand(TTcpConnectedPort * TcpConnectedPort, Payload* payload)
{
    if (payload == NULL) {
      return -1;
    }
	unsigned int payloadSize = sizeof(payload);
    payloadSize = htonl(payloadSize);
    if (WriteDataTcp(TcpConnectedPort,(unsigned char *)&payloadSize,sizeof(payloadSize))!=sizeof(payloadSize)) {
      return(-1);
    }
    return(WriteDataTcp(TcpConnectedPort,(unsigned char *)payload, sizeof(payload));
}


bool TcpRecvCommand(TTcpConnectedPort * TcpConnectedPort, Payload* payload)
{
  unsigned int payloadSize;
  unsigned char *buff;	/* receive buffer */   
  
  if (ReadDataTcp(TcpConnectedPort,(unsigned char *)&payloadSize,sizeof(payloadSize))!=sizeof(payloadSize)) {
    return(false);
  }
  
  payloadSize=ntohl(payloadSize); // convert image size to host format

  if (payloadSize < 0) {
    return false;
  }

  buff = new (std::nothrow) unsigned char [payloadSize];
  if (buff==NULL) {
    return false;
  }

  if((ReadDataTcp(TcpConnectedPort,buff,payloadSize))==payloadSize) {
    // TODO: convert buff to payload

    delete [] buff;
    return true;
  }
  delete [] buff;
  return false;
}
