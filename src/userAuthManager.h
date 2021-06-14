
#include <string>
#include "SecurityManager.h"

#define NUM_STUDENTS 8
#define LIMIT_ID_LENGTH 20
#define LIMIT_PW_LENGTH 32

struct UserData {
    std::string userID;
    std::string password;
    int uid;
};

class UserAuthManager
{
public:
    UserAuthManager();
    ~UserAuthManager();
    bool verifyUser(std::string userid, std::string passwd);
    int getCurrentUid();
private:
    void resetCurrentUser();
    void loadUserDB();
    int saveUserDB();
	bool findUserFromDB(std::string userid);

    bool mIsFound;
    struct UserData mCurrentUserData;
    SecurityManager* mSecurityManager;
};
