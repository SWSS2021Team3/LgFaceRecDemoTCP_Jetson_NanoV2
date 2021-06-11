#include <iostream>
#include "TcpSendRecvCmd.h"

using namespace std;

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
    return(WriteDataTcp(TcpConnectedPort,(unsigned char *)payload, sizeof(payload)));
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

  // TODO:: apply nothrow buff = new (std::nothrow) unsigned char [payloadSize];
  buff = new unsigned char [payloadSize];
  if (buff==NULL) {
    return false;
  }

  if((ReadDataTcp(TcpConnectedPort,buff,payloadSize))==payloadSize) {
    payload = new Payload();
    payload->data_id = ((Payload*)buff)->data_id;
    payload->data_length = ((Payload*)buff)->data_length;
    payload->data = ((Payload*)buff)->data; // deep copy

    cout << "data_id: " << ((Payload*)buff)->data_id << endl;
    cout << "data_length: " << payload->data_length << endl;
    for (int i=0; i<payload->data.size(); i++) {
        cout << " data at " << i << " : " << payload->data.at(i) << endl;
    }

    delete [] buff;
    return true;
  }
  delete [] buff;
  return false;
}
