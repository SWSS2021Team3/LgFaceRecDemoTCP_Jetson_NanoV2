#include "userAuthManager.h"
#include <iostream>
#include <cstring>

using namespace std;

struct UserData UserDB[NUM_STUDENTS] = {
	{"kyuwoon.kim", "FE758D46C0DF2F9D1C1A040CB666CC1B176AEC49CC82BF005D3ECA5C2501C10E", 0},
	{"gyeonghun.ro", "D2CA35D606FF8CBA06839528D3DFA1E80D6F2D6C697C09A4A9FD0590974FC9B4", 1},
	{"wonyoung.chang", "364A613EE71B94D9284DA520D7053DE55012731F3C590BB7BA2D8C55CC46BE65", 2},
	{"soohyun.yi", "2FA60557BA96536E9AB739F6F84661CD23E662D0C3577A309A3BB8A299D49E57", 3},
	{"hyejin.oh", "0C63D9C897C47C1FD2B9370E4335D05BB901ED03F2CC2CF1B9F55AB3409940DA", 4},
	{"hyungjin.choi", "AE9AC2601930DA6FAC36A4F11E245470F0AB8D1887ECBF28B2BB7126A13890D4", 5},
	{"vibhanshu.dhote", "359C2ED7EDAE76006EDB1A2F620F83636435EA780E9FA41150671640CF079ACC", 6},
	{"cliff.huff", "0DD7E3B752B856287D76BD07BE7AEA96A46476221219257B4F8A90FBE44885CE", 7}
};


UserAuthManager::UserAuthManager() {
    resetCurrentUser();
    mSecurityManager = new SecurityManager();
//    loadUserDB();
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
    size_t readSize;
    size_t readLen;

    readSize = mSecurityManager->getSizeUserDB();
    unsigned char* readData = new unsigned char[readSize];

    int ret = mSecurityManager->readUserDB(readData, readSize, &readLen);
    if (ret < 0) {
        cout << "failed to read UserDB" << endl;
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
    size_t dataSize = sizeof(UserDB);
    size_t writeLen;
    int ret = -1;

    cout << "dataSize = " << dataSize << endl;

    ret = mSecurityManager->writeUserDB((unsigned char*)&UserDB, dataSize, &writeLen);
    cout << "ret = " << ret << endl;
    cout << "writeLen = " << writeLen << endl;

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

    // make password digest using sha-256
    unsigned char hashedPW[LIMIT_PW_LENGTH];
    size_t outLen;
    mSecurityManager->makeHash((unsigned char*)passwd.c_str(), LIMIT_PW_LENGTH, hashedPW, &outLen);
	if (mIsFound) {
		return !strncmp((const char*)hashedPW, mCurrentUserData.password.c_str(), pLen);
	}
	return false;
}

int UserAuthManager::getCurrentUid() {
    return mCurrentUserData.uid;
}

