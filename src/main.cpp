#include <string>

#include "commManager.h"
#include "faceManager.h"
#include "SecurityManager.h"

// Uncomment to print timings in milliseconds
#define LOG_TIMES

using namespace nvinfer1;
using namespace nvuffparser;

class SerializableTestClass : public Serializable
{
public:
	size_t serialize_size() const
	{
		return SerializableP<uint16_t>::serialize_size(uid) +
			SerializableP<std::vector<std::string> >::serialize_size(names);
	}
	void serialize(char* buf) const
	{
		buf = SerializableP<uint16_t>::serialize(buf, uid);
		buf = SerializableP<std::vector<std::string>>::serialize(buf, names);
	}
	void deserialize(const char* buf)
	{
		buf = SerializableP<uint16_t>::deserialize(buf, uid);
		buf = SerializableP<std::vector<std::string>>::deserialize(buf, names);
	}  
  uint16_t uid;
  std::vector<std::string> names;
};

int main(int argc, char *argv[])
{
  CommManager *commManager;
  FaceManager *faceManager;
  SecurityManager *secuManager;

  bool UseCamera = false;


  // serializable test --- wonyoung will delete soon..
  SerializablePayload p1;
  SerializablePayload p2;

  p1.data_id = 4;
  p1.str1 = "hello - world";

  printf("p1 data %d %s\n", p1.data_id, p1.str1.c_str());
  printf("p2 data %d %s\n", p2.data_id, p2.str1.c_str());

  char* buf = new char[p1.serialize_size()];
  p1.serialize(buf);
  p2.deserialize(buf);
  delete [] buf;

  printf("p1 data %d %s\n", p1.data_id, p1.str1.c_str());
  printf("p2 data %d %s\n", p2.data_id, p2.str1.c_str());

  SerializableTestClass c1;
  SerializableTestClass c2;
  c1.names.push_back("hello");
  c1.names.push_back("world");
  c1.uid = 322;

  printf("c1 data %d size %lu\n", c1.uid, c1.names.size());
  for (std::string& s : c1.names)
    printf("c1 string %s\n", s.c_str());

  buf = new char[c1.serialize_size()];
  c1.serialize(buf);
  c2.deserialize(buf);
  delete [] buf;

  printf("c2 data %d size %lu\n", c2.uid, c2.names.size());
  for (std::string& s : c2.names)
    printf("c2 string %s\n", s.c_str());


  // end of serializable test

  if (argc < 2)
  {
    fprintf(stderr, "usage %s [port] [filename]\n", argv[0]);
    exit(0); 
  }

  secuManager = new SecurityManager;
  if (secuManager->checkEngine("../facenetModels", "../mtCNNModels") != 0) {
    //engine hash is changed
    //abort server??? we need policy
  }

  int portNumber = atoi(argv[1]);
  int portNumberSecured = portNumber + 10;//6000;
  commManager = new CommManager(portNumber, portNumberSecured, secuManager);

  if (argc == 2)
    faceManager = new FaceManager(commManager, secuManager);
  else
    faceManager = new FaceManager(commManager, argv[2], secuManager);

  if (!faceManager->init())
    return 1;

  bool inLoop = true;
  while (inLoop)
  {
    if (commManager->listen() == false)
    {
      printf("CommManager - connect failed.\n");
      continue;
    }

    faceManager->start();

    inLoop = commManager->do_loop(faceManager);

    faceManager->stop();

    commManager->disconnect();
  }

  delete faceManager;
  delete commManager;

  return 0;
}
