#ifndef _FACE_MANAGER_H
#define _FACE_MANAGER_H
#include <string>
#include <vector>
// #include <map>
#include <opencv2/core.hpp>
#include "faceNet.h"
#include "mtcnn.h"

#include "commManager.h"
#include "videoStreamer.h"

using namespace std;

struct faceData {
    std::string userID;
    std::vector<std::string> faceNumber;
};

class FaceData : public Serializable
{
public:
	size_t serialize_size() const
	{
		return SerializableP<std::string>::serialize_size(userID) +
			SerializableP<std::vector<std::string> >::serialize_size(faceId);
	}
	void serialize(char* buf) const
	{
		buf = SerializableP<std::string>::serialize(buf, userID);
		buf = SerializableP<std::vector<std::string>>::serialize(buf, faceId);
	}
	void deserialize(const char* buf)
	{
		buf = SerializableP<std::string>::deserialize(buf, userID);
		buf = SerializableP<std::vector<std::string>>::deserialize(buf, faceId);
	}  
  std::string userID;
  std::vector<std::string> faceId;
};

template <>
class SerializableP<FaceData>
{
public:
    static size_t serialize_size(FaceData fd)
    {
		return SerializableP<std::string>::serialize_size(fd.userID) +
			SerializableP<std::vector<std::string> >::serialize_size(fd.faceId);
    }

    static char *serialize(char *buf, FaceData fd)
    {
		buf = SerializableP<std::string>::serialize(buf, fd.userID);
		buf = SerializableP<std::vector<std::string>>::serialize(buf, fd.faceId);
        return buf;
    }

    static const char *deserialize(const char *buf, FaceData &fd)
    {
		buf = SerializableP<std::string>::deserialize(buf, fd.userID);
		buf = SerializableP<std::vector<std::string>>::deserialize(buf, fd.faceId);
        return buf;
    }
};


class FaceManager
{
private:
    CommManager *commManager;
    SecurityManager* lSecurityManager;

    // const int videoFrameWidth = 1280;
    // const int videoFrameHeight = 720;
    const int videoFrameWidth = 640;
    const int videoFrameHeight = 480;

    VideoStreamer *videoStreamer;
    bool useCamera;
    bool rotate180;

    cv::VideoWriter writer;

    FaceNetClassifier *faceNet;
    mtcnn *mtCNN;

    int mCurrentUid;
    int maxFacesPerScene;

    bool isVideoPlay = false;
    
    vector<FaceData> faceDB;
    // map<string, string> faceUserMap;
    vector<pair<string,string>> faceUserMap;

    void rotateFrame(cv::Mat &frame);
    void openRecordFile(const char *filename);
    void record(cv::Mat &frame);
    bool addFaceDB(string userID, string faceId);
    bool findUserFromDB(string userID);
    vector<string> getFaceListFromDB(string userID);
public:
    bool deleteFaceDB(string userID, string faceId);
    vector<FaceData>& readFaceDB();
    bool saveFaceDB();
    vector<FaceData>& getFaceDB() { return faceDB; }
    FaceManager(CommManager *comm, SecurityManager* securityManager);
    FaceManager(CommManager *comm, const char *filename, SecurityManager* securityManager);
    ~FaceManager();
    bool init();
    bool loadFaceNet();
    void start();
    bool processFrame();
    bool registerFace(string userID, int numberOfFaces);
    void playVideo(string status);
    void changeVideoSource(string filename);
    void changeVideoSourceLive();
    void stop();

    void sendFaceImages(string userID);
    void setCurrentUid(string userID);
};

#endif // _FACE_MANAGER_H
