#include "userAuthManager.h"
#include "SecurityManager.h"
#include <iostream>
//#include <cstring>

using namespace std;

SerializedUserData sUserDB[NUM_STUDENTS];

UserAuthManager::UserAuthManager(SecurityManager* securityManager) : mSecurityManager(securityManager) {
    resetCurrentUser();
    loadUserDB();
}

UserAuthManager::~UserAuthManager() {
//    saveUserDB(); // use when user changes passwd
    resetCurrentUser();
}

void UserAuthManager::resetCurrentUser() {
	mCurrentUserData.userID = "";
    mCurrentUserData.password = "";
    mCurrentUserData.uid = -1;
    mIsFound = false;
}

void UserAuthManager::loadUserDB() {
    cout << "[UserAuthManager] loadUserDB" << endl;
    size_t readSize;
    size_t readLen;

    readSize = mSecurityManager->getSizeUserDB();
    unsigned char* readData = new unsigned char[readSize];

    int ret = mSecurityManager->readUserDB(readData, readSize, &readLen);
    if (ret < 0) {
        cout << "failed to read UserDB" << endl;
        return;
    }

    unsigned char* pbuf = readData;
    for (int i=0; i<NUM_STUDENTS; i++) {
        sUserDB[i].deserialize((const char*)pbuf);
        pbuf += sUserDB[i].serialize_size();
        //cout << "()()()()userID: " << sUserDB[i].userID << endl;
        //cout << "()()()()passwd: " << sUserDB[i].password << endl;
        //cout << "()()()()uid: " << sUserDB[i].uid << endl;
    }
}

int UserAuthManager::saveUserDB() {
    cout << "[UserAuthManager] saveUserDB" << endl;
    size_t writeLen;
    int ret = -1;

    size_t totalSizeDB = 0;
    for (int i=0; i<NUM_STUDENTS; i++) {
        totalSizeDB += sUserDB[i].serialize_size();
    }
    cout << "totalSizeDB = " << totalSizeDB << endl;
    unsigned char writeBuf[totalSizeDB];

    size_t wittenSize = 0;
    for (int i=0; i<NUM_STUDENTS; i++) {
        //cout << "sUser id " << i << ": " << sUserDB[i].userID << endl;
        //cout << "sUser pw " << i << ": " << sUserDB[i].password << endl;
        //cout << "sUser uid " << i << ": " << sUserDB[i].uid << endl;
        size_t sSize = sUserDB[i].serialize_size();
        char* buf = new char[sSize];
        sUserDB[i].serialize(buf);
        memcpy(writeBuf+wittenSize, buf, sSize);
        wittenSize += sSize;
    }

    mSecurityManager->writeUserDB(writeBuf, totalSizeDB, &writeLen);
    cout << "ret = " << ret << endl;
    cout << "writeLen = " << writeLen << endl;

// read test +++++++++++++
/*
    SerializedUserData ruser[NUM_STUDENTS];

    unsigned char* pbuf = writeBuf;
    for (int i=0; i<NUM_STUDENTS; i++) {
        ruser[i].deserialize((const char*)pbuf);
        pbuf+= ruser[i].serialize_size();
        cout << "()()()()userID: " << ruser[i].userID << endl;
        cout << "()()()()passwd: " << ruser[i].password << endl;
        cout << "()()()()uid: " << ruser[i].uid << endl;
    }
*/
// read test -------------
	return ret;
}

bool UserAuthManager::findUserFromDB(string userid) {
    for (int i=0; i<NUM_STUDENTS; i++) {
        //cout << ">>> " << sUserDB[i].userID << endl;
		if (!strcmp(sUserDB[i].userID.c_str(), userid.c_str())) {
			mCurrentUserData.userID = sUserDB[i].userID;
			mCurrentUserData.password = sUserDB[i].password;
			mCurrentUserData.uid = sUserDB[i].uid;
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
    unsigned char hashedPW[LIMIT_PW_LENGTH] = {0,};
    size_t outLen;
    //cout << ">>>> pass : " << (unsigned char*)passwd.c_str() << endl;
    size_t passLen = strlen(passwd.c_str());

    mSecurityManager->makeHashA((unsigned char*)passwd.c_str(), strlen(passwd.c_str()), hashedPW, &outLen);
    //cout << "hashed pw = " << hashedPW << endl;
    if (mIsFound) {
		return !strncasecmp((const char*)hashedPW, mCurrentUserData.password.c_str(), pLen);
	}
	return false;
}

int UserAuthManager::getCurrentUid() {
    return mCurrentUserData.uid;
}

