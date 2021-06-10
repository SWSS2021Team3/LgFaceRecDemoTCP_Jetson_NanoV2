#include <string>

#include "commManager.h"
#include "faceManager.h"

// Uncomment to print timings in milliseconds
#define LOG_TIMES

using namespace nvinfer1;
using namespace nvuffparser;

int main(int argc, char *argv[])
{
  CommManager *commManager;
  FaceManager *faceManager;

  bool UseCamera = false;

  if (argc < 2)
  {
    fprintf(stderr, "usage %s [port] [filename]\n", argv[0]);
    exit(0);
  }

  int portNumber = atoi(argv[1]);
  commManager = new CommManager(portNumber);

  if (argc == 2)
    faceManager = new FaceManager(commManager);
  else
    faceManager = new FaceManager(commManager, argv[2]);

  if (!faceManager->init())
    return 1;

  bool inLoop = true;

  while (inLoop)
  {
    if (commManager->connect() == false)
    {
      printf("CommManager - connect failed.\n");
      return (-1);
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
