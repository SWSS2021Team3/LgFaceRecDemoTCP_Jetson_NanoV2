#include "userAuthManager.h"
#include "SecurityManager.h"
#include <iostream>

using namespace std;

SerializedUserData sUserDB[NUM_USERS];

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
    if (mSecurityManager == NULL) {
        cout << "security manager is null" << endl;
        return;
    }
    size_t readSize;
    size_t readLen;

    readSize = mSecurityManager->getSizeUserDB();
    unsigned char* readData = new unsigned char[readSize];
    if (readData == NULL) {
        cout << "failed to allocate memory" << endl;
        return;
    }

    int ret = mSecurityManager->readUserDB(readData, readSize, &readLen);
    if (ret < 0) {
        cout << "failed to read UserDB" << endl;
        return;
    }

    unsigned char* pbuf = readData;
    for (int i=0; i<NUM_USERS; i++) {
        sUserDB[i].deserialize((const char*)pbuf);
        pbuf += sUserDB[i].serialize_size();
        //cout << "()()()()userID: " << sUserDB[i].userID << endl;
        //cout << "()()()()passwd: " << sUserDB[i].password << endl;
        //cout << "()()()()uid: " << sUserDB[i].uid << endl;
    }

    if (readData != NULL) {
        delete readData;
    }
}

int UserAuthManager::saveUserDB() {
    cout << "[UserAuthManager] saveUserDB" << endl;
    if (mSecurityManager == NULL) {
        cout << "security manager is null" << endl;
        return -1;
    }
    size_t writeLen;

    size_t totalSizeDB = 0;
    for (int i=0; i<NUM_USERS; i++) {
        totalSizeDB += sUserDB[i].serialize_size();
    }
    cout << "totalSizeDB = " << totalSizeDB << endl;
    unsigned char writeBuf[totalSizeDB];

    size_t writtenSize = 0;
    for (int i=0; i<NUM_USERS; i++) {
        //cout << "sUser id " << i << ": " << sUserDB[i].userID << endl;
        //cout << "sUser pw " << i << ": " << sUserDB[i].password << endl;
        //cout << "sUser uid " << i << ": " << sUserDB[i].uid << endl;
        size_t sSize = sUserDB[i].serialize_size();
        char* buf = new char[sSize];
        if (buf == NULL) {
            cout << "failed to allocate buffer" << endl;
            return -1;
        }
        sUserDB[i].serialize(buf);
        if (totalSizeDB >= writtenSize + sSize) {
            memcpy(writeBuf+writtenSize, buf, sSize);
        }
        writtenSize += sSize;
        if (buf != NULL) {
            delete buf;
        }
    }

    int ret = mSecurityManager->writeUserDB(writeBuf, totalSizeDB, &writeLen);
    if (ret < 0) {
        cout << "failed to write UserDB" << endl;
        return ret;
    }
    cout << "writeLen = " << writeLen << endl;

// read test +++++++++++++
/*
    SerializedUserData ruser[NUM_USERS];

    unsigned char* pbuf = writeBuf;
    for (int i=0; i<NUM_USERS; i++) {
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
    for (int i=0; i<NUM_USERS; i++) {
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
	if (mSecurityManager == NULL) {
        cout << "security manager is null" << endl;
        return false;
    }
    size_t iLen = userid.length();
	if (iLen > LIMIT_ID_LENGTH) {
		cout << "invalid id length" << endl;
		return false;
	}
    size_t pLen = passwd.length();
	if (pLen > LIMIT_PW_LENGTH) {
		cout << "invalid password legnth" << endl;
		return false;
	}
	if(!findUserFromDB(userid)) {
		cout << "unable to find user : " << userid << endl;
		return false;
	}
    if (passwd == "") {
        cout << "passwd is null" << endl;
        return false;
    }

    // make password digest using sha-256
    unsigned char hashedPW[LIMIT_PW_LENGTH] = {0,};
    size_t outLen;
    //cout << ">>>> pass : " << (unsigned char*)passwd.c_str() << endl;
    size_t passLen = passwd.length();

    mSecurityManager->makeHashStr((unsigned char*)passwd.c_str(), passwd.length(), hashedPW, &outLen);
    //cout << "hashed pw = " << hashedPW << endl;
    if (mIsFound) {
		return !strncasecmp((const char*)hashedPW, mCurrentUserData.password.c_str(), pLen);
	}
	return false;
}

int UserAuthManager::getCurrentUid() {
    return mCurrentUserData.uid;
}

std::vector<struct UserData> UserAuthManager::getAllUsers()
{
    vector<struct UserData> allUsers;

    for (int i=0; i<NUM_USERS; i++) {
        allUsers.emplace_back();
        struct UserData& user = allUsers.back();

        user.uid = sUserDB[i].uid;
        user.userID = sUserDB[i].userID;
    }
    return allUsers;
}