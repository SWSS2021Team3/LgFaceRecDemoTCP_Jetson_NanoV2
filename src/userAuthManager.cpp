#include "userAuthManager.h"
#include <iostream>
#include <cstring>

using namespace std;

struct UserData UserDB[NUM_STUDENTS] = {
	{"kyuwoon.kim", "hashedpassword", 0},
	{"gyeonghun.ro", "hashedpassword", 1},
	{"wonyoung.chang", "hashedpassword", 2},
	{"soohyun.yi", "hashedpassword", 3},
	{"hyejin.oh", "hashedpassword", 4},
	{"hyungjin.choi", "hashedpassword", 5},
	{"vibhanshu.dhote", "hashedpassword", 6},
	{"cliff.huff", "hashedpassword", 7}
};




UserAuthManager::UserAuthManager() {
    resetCurrentUser();
    mSecurityManager = new SecurityManager();
    loadUserDB();
}

UserAuthManager::~UserAuthManager() {
    resetCurrentUser();
    if (mSecurityManager != NULL) {
        delete mSecurityManager;
    }
}

void UserAuthManager::resetCurrentUser() {
	mCurrentUserData.userID = "";
    mCurrentUserData.password = "";
    mCurrentUserData.uid = -1;
    mIsFound = false;
}

void UserAuthManager::loadUserDB() {
    unsigned char* readData = NULL;
    size_t readSize;
    size_t readLen;
    // TODO: calculate readSize
    /*
    int ret = mSecurityManager->readUserDB(readData, readSize, &readLen);
    if (ret < 0) {
        cout << "failed to read UserDB" << endl;
        return;
    }*/

    if (readData == NULL) {
        cout << "[ERR] failed to load UserDB" << endl;
        return;
    }
    struct UserData* udata = (struct UserData*)readData;
	for (int i=0; i<NUM_STUDENTS; i++) {
        UserDB[i].userID = udata->userID;
		UserDB[i].password = udata->password;
        UserDB[i].uid = udata->uid;
	}
}

int UserAuthManager::saveUserDB() {
	unsigned char* saveData = (unsigned char*)UserDB;
    size_t dataSize = strlen((const char*)saveData);
    size_t writeLen;
    int ret = -1;

    cout << "dataSize = " << dataSize << endl;

    // TODO: verify dataSize
    ret = mSecurityManager->writeUserDB(saveData, dataSize, &writeLen);

	return ret;
}

bool UserAuthManager::findUserFromDB(string userid) {
    for (int i=0; i<NUM_STUDENTS; i++) {
		if (!strcmp(UserDB[i].userID.c_str(), userid.c_str())) {
			mCurrentUserData.userID = UserDB[i].userID;
			mCurrentUserData.password = UserDB[i].password;
			mCurrentUserData.uid = UserDB[i].uid;
            mIsFound = true;
			break;;
		}
	}
    return mIsFound;
}

// passwd : hashed digest
bool UserAuthManager::verifyUser(string userid, string passwd) {
    resetCurrentUser();
	size_t iLen = strlen(userid.c_str());
	if (iLen > LIMIT_ID_LENGTH) {
		cout << "invalid id length" << endl;
		return false;
	}
    size_t pLen = strlen(passwd.c_str());
	if (pLen > LIMIT_PW_LENGTH) {
		cout << "invalid password legnth" << endl;
		return false;
	}
	if(!findUserFromDB(userid)) {
		cout << "unable to find user : " << userid << endl;
		return false;
	}

	if (mIsFound) {
		return !strncmp(passwd.c_str(), mCurrentUserData.password.c_str(), pLen);
	}
	return false;
}

int UserAuthManager::getCurrentUid() {
    return mCurrentUserData.uid;
}

