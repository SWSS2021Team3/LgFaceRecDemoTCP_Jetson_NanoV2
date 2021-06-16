
#include <string>
#include <cstring>

#define NUM_USERS 9
#define LIMIT_ID_LENGTH 20
#define LIMIT_PW_LENGTH 64

class SecurityManager;

class SerializableUserIF
{
public:
    virtual size_t serialize_size() const = 0;
    virtual void serialize(char *buf) const = 0;
    virtual void deserialize(const char *buf) = 0;
};

template <typename T>
class SerializableU
{
public:
    static size_t serialize_size(T v)
    {
        return sizeof(T);
    }

    static char *serialize(char *target, T v)
    {
        size_t size = serialize_size(v);
        memcpy(target, &v, size);
        return target + size;
    }

    static const char *deserialize(const char *src, T &lv)
    {
        size_t size = serialize_size(lv);
        memcpy(&lv, src, size);
        return src + size;
    }
};

template <>
class SerializableU<std::string>
{
public:
    static size_t serialize_size(std::string v)
    {
        return sizeof(size_t) + v.length() + 1;
    }

    static char *serialize(char *target, std::string v)
    {
        size_t length = v.length();
        memcpy(target, &length, sizeof(size_t));
        memcpy(target + sizeof(size_t), v.c_str(), length);
        memset(target + sizeof(size_t) + length, 0, 1); // end of string
        return target + serialize_size(v);
    }

    static const char *deserialize(const char *src, std::string &v)
    {
        size_t length;
        memcpy(&length, src, sizeof(size_t));
        v = (src + sizeof(size_t));
        return src + serialize_size(v);
    }
};

class SerializedUserData : public SerializableUserIF
{
public:
    std::string userID;
    std::string password;
    uint16_t uid;

    size_t serialize_size() const
    {
        return SerializableU<std::string>::serialize_size(userID) +
            SerializableU<std::string>::serialize_size(password) +
            SerializableU<uint16_t>::serialize_size(uid);
    }

    void serialize(char* buf) const
    {
        buf = SerializableU<std::string>::serialize(buf, userID);
        buf = SerializableU<std::string>::serialize(buf, password);
        buf = SerializableU<uint16_t>::serialize(buf, uid);
    }

    void deserialize(const char* buf)
    {
        buf = SerializableU<std::string>::deserialize(buf, userID);
        
        buf = SerializableU<std::string>::deserialize(buf, password);
        buf = SerializableU<uint16_t>::deserialize(buf, uid);
    }

};



struct UserData {
    std::string userID;
    std::string password;
    int uid;
};

class UserAuthManager
{
public:
    UserAuthManager(SecurityManager* securityManager);
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
