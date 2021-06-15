#include <iostream>
#include "TcpSendRecvCmd.h"

using namespace std;

int TcpSendCommand(TTcpConnectedPort * TcpConnectedPort, Payload* payload)
{
    if (payload == NULL) {
      return -1;
    }
	unsigned int payloadSize = sizeof(Payload);
    payloadSize = htonl(payloadSize);
    if (WriteDataTcp(TcpConnectedPort,(unsigned char *)&payloadSize,sizeof(payloadSize))!=sizeof(payloadSize)) {
      return(-1);
    }
    return static_cast<int>(WriteDataTcp(TcpConnectedPort,reinterpret_cast<unsigned char *>(payload), sizeof(Payload)));
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
    payload->data_id = ((Payload*)buff)->data_id;
    payload->data_length = ((Payload*)buff)->data_length;
    //payload->data = ((Payload*)buff)->data; // deep copy

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

int TcpSendObject(TTcpConnectedPort* TcpConnectedPort, Serializable* s)
{
    if (s == NULL)
        return -1;

    size_t s_size = s->serialize_size();

    unsigned int payloadSize = htonl(static_cast<u_long>(s_size));
    if (WriteDataTcp(TcpConnectedPort, reinterpret_cast<unsigned char*>(&payloadSize), sizeof(payloadSize)) != sizeof(payloadSize)) {
        return(-1);
    }
    char* buf = new (std::nothrow) char[s_size];
    if (buf == NULL)
        return false;
    s->serialize(buf);
    int ret = static_cast<int>(WriteDataTcp(TcpConnectedPort, reinterpret_cast<unsigned char*>(buf), s_size));
    delete[] buf;
    return ret;
}


bool TcpRecvObject(TTcpConnectedPort* TcpConnectedPort, Serializable* s)
{
    if (s == NULL)
        return false;

    unsigned int payloadSize;

    if (ReadDataTcp(TcpConnectedPort, reinterpret_cast<unsigned char*>(&payloadSize), sizeof(payloadSize)) != sizeof(payloadSize)) {
        return(false);
    }

    size_t s_size = ntohl(payloadSize); // convert image size to host format

    char* buf = new (std::nothrow) char[s_size];
    if (buf == NULL)
        return false;

    if ((ReadDataTcp(TcpConnectedPort, reinterpret_cast<unsigned char*>(buf), s_size)) == s_size) {
        s->deserialize(buf);

        delete[] buf;
        return true;
    }
    delete[] buf;
    return false;
}
