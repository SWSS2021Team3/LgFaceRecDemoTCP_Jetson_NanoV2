//design pattern: facade pattern
#pragma once
#include <unordered_map>
#include <string>

class SecurityManager
{
  private:
    std::unordered_map<std::string, std::string> symmetricKey;
    std::unordered_map<std::string, std::string> asymmetricKey;
    std::unordered_map<std::string, std::string> certificate;
    std::unordered_map<std::string, std::string> iv;
    std::unordered_map<std::string, std::string> hashFaceNet;
    std::unordered_map<std::string, std::string> hashMtCnn;
    bool healthy;

  public:
    SecurityManager();
    ~SecurityManager();

    int writeVideoDB(unsigned char* buffer, size_t bufferSize, size_t* writeLen);
    int writeFaceDB(unsigned char* buffer, size_t bufferSize, size_t* writeLen);
    int writeUserDB(unsigned char* buffer, size_t bufferSize, size_t* writeLen);
    int checkEngine(const std::string facePath, std::string mtCnnPath);

    size_t getSizeVideoDB();
    size_t getSizeFaceDB();
    size_t getSizeUserDB();
    
    int readVideoDB(unsigned char* buffer, size_t bufferSize, size_t* readLen);
    int readFaceDB(unsigned char* buffer, size_t bufferSize, size_t* readLen);
    int readUserDB(unsigned char* buffer, size_t bufferSize, size_t* readLen);
    int readEngine(unsigned char* buffer, size_t bufferSize, size_t* readLen);

    int makeHash(unsigned char* buffer, size_t bufferSize, unsigned char* out, size_t* outLen);

    void* getSecureNeworkContext();
    int freeSecureNetworkContext(void* p);

  private:
    void* secureNetworkContext;
    static const std::string pathVideoDB;
    static const std::string pathVideoDBSign;
    static const std::string pathUserDB;
    static const std::string pathUserDBSign;
    static const std::string pathFaceDB;
    static const std::string pathFaceDBSign;
   
    int readKey();
    int changeKey(); //unencrypt && generateKey && readKey && sign/enc
    int generateKey();
    //int getFileSize(const std::string path);

    void debug_print_key();
    void debug_print_size();
    void debug_verify_db();
};