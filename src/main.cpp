#include <string>

#include "commManager.h"
#include "faceManager.h"
#include "SecurityManager.h"
#include "userAuthManager.h"

using namespace nvinfer1;
using namespace nvuffparser;

int main(int argc, char *argv[])
{
  CommManager *commManager;
  FaceManager *faceManager;
  SecurityManager *secuManager;
  UserAuthManager *userAuthManager;

  bool UseCamera = false;

  if (argc < 2)
  {
    std::cerr << "usage " << argv[0] << " [port] [filename]" << std::endl;
    exit(0); 
  }

  int portNumber = 5000;
  {
    try
    {
      portNumber = std::stoi(string(argv[1]));
    }
    catch (std::invalid_argument &)
    {
      std::cerr << "port invalid. use default port " << portNumber << std::endl;
    }
  }
  
  int portNumberSecured = portNumber + 10;

  secuManager = new SecurityManager;
  if (secuManager->readKey() <= 0)
  {
    delete secuManager;
    std::cerr << "Program exit. check usb keys" << std::endl;
    exit(-1);
  }
  if (secuManager->checkEngine("../facenetModels", "../mtCNNModels") != 0)
  {
    delete secuManager;
    std::cerr << "Program exit. check facenet and mtcnn model files." << std::endl;
    exit(-1);
  }
  userAuthManager = new UserAuthManager(secuManager);

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

    inLoop = commManager->do_loop(faceManager, userAuthManager);

    faceManager->stop();

    commManager->disconnect();
  }

  delete faceManager;
  delete commManager;
  delete userAuthManager;

  return 0;
}
