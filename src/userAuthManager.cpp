#include "userAuthManager.h"
#include <iostream>
#include <cstring>

using namespace std;

// passwd : hashed digest
bool UserAuthManager::verifyUser(string userid, string passwd) {
    string passwdFromDB;
    // TODO: get user passwd from UserDB via security manager

    size_t pLen = strlen(passwd.c_str());
    return strncmp(passwd.c_str(), passwdFromDB.c_str(), pLen);
}

