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

  bool stop = false;

  while (!stop)
  {
    if (commManager->connect() == false)
    {
      printf("CommManager - connect failed.\n");
      return (-1);
    }

    faceManager->start();

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
          stop = true;
          break;
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

    faceManager->stop();

    commManager->disconnect();

    auto globalTimeEnd = chrono::steady_clock::now();

    auto milliseconds = chrono::duration_cast<chrono::milliseconds>(globalTimeEnd - globalTimeStart).count();
    double seconds = double(milliseconds) / 1000.;
    double fps = nbFrames / seconds;

    std::cout << "Counted " << nbFrames << " frames in " << double(milliseconds) / 1000. << " seconds!"
              << " This equals " << fps << "fps.\n";
  }

  delete faceManager;
  delete commManager;

  return 0;
}
