#include <unordered_map>
#include <iostream>
#include <string>
#include <fstream>
#include <memory>
#include <sstream> //stringstream
#include <iomanip> //setw(), setfill()
#include <climits> //INT_MAX
#include <cstdio>
#include <algorithm> //min
#include <cstring>

#include "SecurityManager.h"

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/cms.h>
#include <openssl/ssl.h>

const std::string SecurityManager::pathVideoDB = "../videodb.bin";
const std::string SecurityManager::pathVideoDBSign = "../videodb.sign";
const std::string SecurityManager::pathUserDB = "../userdb.bin";
const std::string SecurityManager::pathUserDBSign = "../userdb.sign";
const std::string SecurityManager::pathFaceDB = "../facedb.bin";
const std::string SecurityManager::pathFaceDBSign = "../facedb.sign";

// File Util
std::string readFile(const std::string &path)
{
    std::string result;
    FILE *fp = fopen(path.c_str(), "rb");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        result.resize(std::max(ftell(fp), (long int)0));
        fseek(fp, 0, SEEK_SET);
        auto len = fread(&(result[0]), 1, result.size(), fp);
        fclose(fp);
        if (len != result.size()) {
            result.resize(len);
        }
    }
    return result;
}

bool writeFile(const std::string &path, const char *ptr, const size_t size)
{
    FILE *fp = fopen(path.c_str(), "wb");
    if (fp) {
        fwrite(ptr, 1, size, fp);
        fclose(fp);
        return true;
    }
    return false;
}

bool writeFile(const std::string &path, const std::string &contents)
{
    return writeFile(path, contents.c_str(), contents.size());
}

int getFileSize(const std::string path) {
    std::ifstream in(path, std::ios_base::in | std::ios_base::binary);
    if (!in.good()) {
        std::cout << "Error to open \"" << path << "\" file." << std::endl;
        return 0;
    }
    in.seekg(0, std::ios_base::end);
    std::streampos filesize = in.tellg(); //fixme: need to check exceed 32bit interger
    std::cout << "filesize: " << filesize << std::endl;
    in.close();
    if (filesize > INT_MAX) { return -1;} //fixme: int_max is still too large.
    return filesize;
}

// Security Functions
unsigned int sha256_evp(unsigned char* in, unsigned char* out, int len)
{
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    const EVP_MD* md = EVP_sha256();
    unsigned int md_len = 0;

    EVP_DigestInit_ex(mdctx, md, NULL);
    EVP_DigestUpdate(mdctx, in, len);
    EVP_DigestFinal_ex(mdctx, out, &md_len);
    EVP_MD_CTX_free(mdctx);
    return md_len;
}

std::string GetHexString(const std::string &in)
{
    std::stringstream ss;
    for (auto &iter : in) {
        ss << std::setfill('0') << std::setw(2) << std::right << std::hex << std::nouppercase
           << static_cast<unsigned int>(static_cast<unsigned char>(iter));
    }
    return ss.str();
}

bool compareHash(std::string path, std::string hash)
{
    std::cout << "path: " << path << std::endl;
    std::cout << "recorded hash: " << hash << std::endl;
    
    int filesize = getFileSize(path);
    if (filesize > 104857600) { //100MB 
        std::cout << "too large" << std::endl;
        return false;
    } else if (filesize <= 0) {
        std::cout << "wrong file" << std::endl;
        return false;
    }
    std::ifstream in(path, std::ios_base::in | std::ios_base::binary);
    if (!in.good()) {
        std::cout << "Error to open \"" << path << "\" file." << std::endl;
        return 0;
    }
    unsigned char calc_hash[EVP_MAX_MD_SIZE];
    unsigned char *p = new unsigned char[filesize];
    if (p == nullptr) {
        in.close();
        std::cout << "alloc failed" << std::endl;
        return false;
    }
    in.read(reinterpret_cast<char*>(p), filesize);
    size_t read_len = in.tellg();
    std::cout << "read_len: " << read_len << std::endl;
    in.close();
    int hash_len = sha256_evp(p, calc_hash, filesize);
    std::string hexString = GetHexString(std::string(reinterpret_cast<char*>(calc_hash), hash_len));
    std::cout << "calculated hash: " << hexString << std::endl;
    delete[] p;

    return (hash.compare(hexString) == 0);
}

int encrypt_aes128cbc(unsigned char* plaintext, int plaintext_len, unsigned char* key,
    unsigned char* iv, unsigned char* ciphertext)
{
    EVP_CIPHER_CTX* ctx = NULL;
    int len = 0; //temporary ciphertext length
    int ciphertext_len = 0;

    if (!(ctx = EVP_CIPHER_CTX_new())) {
        std::cout << "failed to EVP_CIPHER_CTX_new()" << std::endl;
        return 0;
    }
    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv)) {
        std::cout << "failed to EVP_EncryptInit_ex(aes_128_cbc)" << std::endl;
        return 0;
    }
    if (1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len)) {
        std::cout << "failed to EVP_EncryptUpdate()" << std::endl;
        return 0;
    }
    ciphertext_len = len;
    if (1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len)) {
        std::cout << "failed to EVP_EncryptFinal_ex()" << std::endl;
        return 0;
    }
    EVP_CIPHER_CTX_free(ctx);
    ciphertext_len += len;
    return ciphertext_len;
}

int decrypt_aes128cbc(const char* ciphertext, int ciphertext_len, const char* key,
    const char* iv, unsigned char* plaintext)
{
    const unsigned char* key_ = reinterpret_cast<const unsigned char*>(key);
    const unsigned char* iv_ = reinterpret_cast<const unsigned char*>(iv);
    const unsigned char* ciphertext_ = reinterpret_cast<const unsigned char*>(ciphertext);
    EVP_CIPHER_CTX* ctx;

    int len;

    int plaintext_len;

    /* Create and initialise the context */
    if (!(ctx = EVP_CIPHER_CTX_new())) {
        std::cout << "failed to EVP_CIPHER_CTX_new()" << std::endl;
        return 0;
    }
    if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key_, iv_)){
        std::cout << "failed to EVP_DecryptInit_ex(aes_128_cbc)" << std::endl;
        return 0;
    }
    if (1 != EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext_, ciphertext_len)){
        std::cout << "failed to EVP_DecryptUpdate()" << std::endl;
        return 0;
    }
    plaintext_len = len;
    if (1 != EVP_DecryptFinal_ex(ctx, plaintext + len, &len)) {
        std::cout << "failed to EVP_DecryptFinal_ex()" << std::endl;
        return 0;
    }
    plaintext_len += len;

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    return plaintext_len;
}

int cmsSign(std::string &signer, std::string &key, std::string &in, std::string &sign) {
    BIO* bio_signer, * bio_key;
    X509* x509;
    EVP_PKEY* evp_key;
    BIO* bio_in;
    BIO* bio_out;

    //signer
    bio_signer = BIO_new(BIO_s_mem());
    BIO_write(bio_signer, signer.c_str(), signer.size());
    x509 = PEM_read_bio_X509(bio_signer, NULL, NULL, NULL);

    //key
    bio_key = BIO_new(BIO_s_mem());
    BIO_write(bio_key, key.c_str(), key.size());
    evp_key = PEM_read_bio_PrivateKey(bio_key, NULL, NULL, NULL);
    
    //in out
    bio_in = BIO_new_mem_buf(in.c_str(), in.size());
    bio_out = BIO_new(BIO_s_mem());

    //cms
    int flag = CMS_PARTIAL | CMS_BINARY | CMS_NOSMIMECAP | CMS_DETACHED;
    CMS_ContentInfo* cms = CMS_sign(x509, evp_key, NULL, bio_in, flag);
    CMS_final(cms, bio_in, NULL, flag);
    i2d_CMS_bio_stream(bio_out, cms, bio_in, flag);

    //out 
    char *p;
    int bioSize = (int) BIO_get_mem_data(bio_out, &p);
    sign = std::string(p, bioSize);
 
    //clean
    CMS_ContentInfo_free(cms);
    BIO_free(bio_out);
    BIO_free(bio_in);
    BIO_free(bio_signer);
    EVP_PKEY_free(evp_key);
    X509_free(x509);
}

int cmsVerify(std::string &sign, std::string &content, std::string &rootca) {
    BIO *bio_sign;
    CMS_ContentInfo* cms;
    BIO* bio_content;
    BIO* bio_ca;
    X509* x509_ca;
    int ret = -1;

    auto cms_der_in     = reinterpret_cast<const unsigned char *>(sign.c_str());
    long cms_der_length = sign.size();

    cms = d2i_CMS_ContentInfo(NULL, &cms_der_in, cms_der_length);

    bio_content = BIO_new_mem_buf(content.c_str(), content.size());
    bio_ca = BIO_new_mem_buf(rootca.c_str(), rootca.size());
    x509_ca = PEM_read_bio_X509(bio_ca, NULL, NULL, NULL);

    X509_STORE* store = X509_STORE_new();
    X509_STORE_add_cert(store, x509_ca);

    int flags = CMS_BINARY;
    if (CMS_verify(cms, NULL, store, bio_content, NULL, flags) == 1) {
        ret = 1;
    }

    X509_STORE_free(store);
    X509_free(x509_ca);
    BIO_free(bio_ca);
    BIO_free(bio_content);
    CMS_ContentInfo_free(cms);
    //BIO_free(bio_sign);

    return ret;
}


//public:
SecurityManager::SecurityManager() : healthy(false), secureNetworkContext(nullptr)
{
}

SecurityManager::~SecurityManager()
{
    hashFaceNet.clear();
    hashMtCnn.clear();
}

int SecurityManager::writeVideoDB(unsigned char* buffer, size_t bufferSize, size_t* writeLen)
{ 
    std::string ciphertext = std::string();
    ciphertext.resize((bufferSize / 16 + 1) * 16);
    std::string &cipherkey = symmetricKey["videodb"];
    std::string &cipheriv = iv["videodb"];
    int ciphertextLen = encrypt_aes128cbc(buffer, bufferSize,
        reinterpret_cast<unsigned char*>(const_cast<char*>(cipherkey.c_str())),
        reinterpret_cast<unsigned char*>(const_cast<char*>(cipheriv.c_str())),
        reinterpret_cast<unsigned char*>(const_cast<char*>(ciphertext.c_str())));
    
    ciphertext.resize(ciphertextLen);

    std::cout << "buffer len:" << bufferSize << " hex:" << GetHexString(std::string((char*)buffer, std::min((int)bufferSize, 32))) << std::endl;
    std::cout << "cipher len:" << ciphertextLen << " hex:" << GetHexString(ciphertext.substr(0, std::min((int)ciphertext.size(), 32))) << std::endl;

    std::string &signer = certificate["videodb"];
    std::string &signkey = asymmetricKey["videodb"];
    std::string sign;
    cmsSign(signer, signkey, ciphertext, sign);

    writeFile(pathVideoDB, ciphertext);
    writeFile(pathVideoDBSign, sign);
    std::cout << "sign len:" << sign.size() << " hex:" << GetHexString(sign.substr(0, 32)) << std::endl;

    *writeLen = ciphertext.size();
    return 0; //error code
}

int SecurityManager::writeFaceDB(unsigned char* buffer, size_t bufferSize, size_t* writeLen) {
    std::string ciphertext = std::string();
    ciphertext.resize((bufferSize / 16 + 1) * 16);
    std::string &cipherkey = symmetricKey["facedb"];
    std::string &cipheriv = iv["facedb"];
    int ciphertextLen = encrypt_aes128cbc(buffer, bufferSize,
        reinterpret_cast<unsigned char*>(const_cast<char*>(cipherkey.c_str())),
        reinterpret_cast<unsigned char*>(const_cast<char*>(cipheriv.c_str())),
        reinterpret_cast<unsigned char*>(const_cast<char*>(ciphertext.c_str())));
    
    ciphertext.resize(ciphertextLen);

    std::cout << "buffer len:" << bufferSize << " hex:" << GetHexString(std::string((char*)buffer, std::min((int)bufferSize, 32))) << std::endl;
    std::cout << "cipher len:" << ciphertextLen << " hex:" << GetHexString(ciphertext.substr(0, std::min((int)ciphertext.size(), 32))) << std::endl;

    std::string &signer = certificate["facedb"];
    std::string &signkey = asymmetricKey["facedb"];
    std::string sign;
    cmsSign(signer, signkey, ciphertext, sign);

    writeFile(pathFaceDB, ciphertext);
    writeFile(pathFaceDBSign, sign);
    std::cout << "sign len:" << sign.size() << " hex:" << GetHexString(sign.substr(0, 32)) << std::endl;

    *writeLen = ciphertext.size();
    return 0; //error code
}
int SecurityManager::writeUserDB(unsigned char* buffer, size_t bufferSize, size_t* writeLen) {
        std::string ciphertext = std::string();
    ciphertext.resize((bufferSize / 16 + 1) * 16);
    std::string &cipherkey = symmetricKey["userdb"];
    std::string &cipheriv = iv["userdb"];
    int ciphertextLen = encrypt_aes128cbc(buffer, bufferSize,
        reinterpret_cast<unsigned char*>(const_cast<char*>(cipherkey.c_str())),
        reinterpret_cast<unsigned char*>(const_cast<char*>(cipheriv.c_str())),
        reinterpret_cast<unsigned char*>(const_cast<char*>(ciphertext.c_str())));
    
    ciphertext.resize(ciphertextLen);

    std::cout << "buffer len:" << bufferSize << " hex:" << GetHexString(std::string((char*)buffer, std::min((int)bufferSize, 32))) << std::endl;
    std::cout << "cipher len:" << ciphertextLen << " hex:" << GetHexString(ciphertext.substr(0, std::min((int)ciphertext.size(), 32))) << std::endl;

    std::string &signer = certificate["userdb"];
    std::string &signkey = asymmetricKey["userdb"];
    std::string sign;
    cmsSign(signer, signkey, ciphertext, sign);

    writeFile(pathUserDB, ciphertext);
    writeFile(pathUserDBSign, sign);
    std::cout << "sign len:" << sign.size() << " hex:" << GetHexString(sign.substr(0, 32)) << std::endl;

    *writeLen = ciphertext.size();
    return 0; //error code
 }
int SecurityManager::checkEngine(const std::string facePath, std::string mtCnnPath)
{
    int countChecked = 0;
    for (auto &it: hashFaceNet) {
        std::string path = facePath + "/" + it.first;
        bool result = compareHash(path, it.second);
        if (result) {
            std::cout << path <<": OK" << std::endl;
            ++countChecked;
        }
        else {
            std::cout << path <<": FAILED HASH" << std::endl;
        }
    }
    for (auto &it: hashMtCnn) {
        std::string path = mtCnnPath + "/" + it.first;
        bool result = compareHash(path, it.second);
        if (result) {
            std::cout << path <<": OK" << std::endl;
            ++countChecked;
        }
        else {
            std::cout << path <<": FAILED HASH" << std::endl;
        }
    }

    std::cout << "check hash count:" << countChecked << " must be " <<  hashFaceNet.size() + hashMtCnn.size() << "." << std::endl;
    if (healthy && countChecked == hashFaceNet.size() + hashMtCnn.size()) {
        return 0; //OK
    }

    return -(countChecked);
}

size_t SecurityManager:: getSizeVideoDB() {
    return getFileSize(pathVideoDB);
}
size_t SecurityManager:: getSizeFaceDB() {
    return getFileSize(pathFaceDB);
}
size_t SecurityManager:: getSizeUserDB() {
    return getFileSize(pathUserDB);
}

int SecurityManager::readDB(std::string &dbenc, std::string &dbsign, std::string &cipherkey, std::string &iv, std::string name, unsigned char *buffer, size_t bufferSize, size_t *readLen) {
    std::string &rootca = certificate["rootca"];
    if (dbenc.size() == 0 || dbsign.size() == 0 || rootca.size() == 0) {
        *readLen = 0;
        return -3;
    }

    int ret = cmsVerify(dbsign, dbenc, rootca);
    if(ret != 1) { 
        std::cout << name << " verify failed!!!" << std::endl;
        return ret;
    }
    std::cout << name << " verify ok" << std::endl;

    int len = decrypt_aes128cbc(dbenc.c_str(), dbenc.size(), cipherkey.c_str(), iv.c_str(), buffer);
    if (len == 0) { return -2; }
    *readLen = len;

    std::cout << name << "dec len:" << bufferSize << " hex:" << GetHexString(std::string((char*)buffer, std::min(len, 32))) << std::endl;
    return 1;
}

int SecurityManager::readVideoDB(unsigned char* buffer, size_t bufferSize, size_t* readLen) {
    std::string dbenc = readFile(pathVideoDB);
    std::string dbsign = readFile(pathVideoDBSign);
    std::string &cipherkey = symmetricKey["videodb"];
    std::string &cipheriv = iv["videodb"];
    std::string name = "videodb";

    //debug_print
    std::cout << name << " enc len:" << dbenc.size() << " hex:" << GetHexString(dbenc.substr(0, std::min((int)dbenc.size(), 32))) << std::endl;
    std::cout << name << "sign len:" << dbsign.size() << " hex:" << GetHexString(dbsign.substr(0, std::min((int)dbsign.size(), 32))) << std::endl;
    
    return readDB(dbenc, dbsign, cipherkey, cipheriv, name, buffer, bufferSize, readLen);
}
int SecurityManager::readFaceDB(unsigned char* buffer, size_t bufferSize, size_t* readLen) {
    std::string dbenc = readFile(pathFaceDB);
    std::string dbsign = readFile(pathFaceDBSign);
 
    //debug_print
    std::cout << "facedbenc len:" << dbenc.size() << " hex:" << GetHexString(dbenc.substr(0, std::min((int)dbenc.size(), 32))) << std::endl;
    std::cout << "facedbsign len:" << dbsign.size() << " hex:" << GetHexString(dbsign.substr(0, std::min((int)dbsign.size(), 32))) << std::endl;

    std::string &rootca = certificate["rootca"];
    if (dbenc.size() == 0 || dbsign.size() == 0 || rootca.size() == 0) {
        *readLen = 0;
        return -3;
    }

    int ret = cmsVerify(dbsign, dbenc, rootca);
    if(ret != 1) { 
        std::cout << "facedb verify failed!!!" << std::endl;
        return ret;
    }
    std::cout << "facedb verify ok" << std::endl;

    std::string &cipherkey = symmetricKey["facedb"];
    std::string &cipheriv = iv["facedb"];
    int len = decrypt_aes128cbc(dbenc.c_str(), dbenc.size(), cipherkey.c_str(), cipheriv.c_str(), buffer);
    if (len == 0) { return -2; }
    else { *readLen = len; }

    std::cout << "buffer len:" << bufferSize << " hex:" << GetHexString(std::string((char*)buffer, std::min(len, 32))) << std::endl;
    return 1;
}
int SecurityManager::readUserDB(unsigned char* buffer, size_t bufferSize, size_t* readLen) { 
    std::string dbenc = readFile(pathUserDB);
    std::string dbsign = readFile(pathUserDBSign);
 
    //debug_print
    std::cout << "userdbenc len:" << dbenc.size() << " hex:" << GetHexString(dbenc.substr(0, std::min((int)dbenc.size(), 32))) << std::endl;
    std::cout << "userdbsign len:" << dbsign.size() << " hex:" << GetHexString(dbsign.substr(0, std::min((int)dbsign.size(), 32))) << std::endl;

    std::string &rootca = certificate["rootca"];
    if (dbenc.size() == 0 || dbsign.size() == 0 || rootca.size() == 0) {
        *readLen = 0;
        return -3;
    }
    int ret = cmsVerify(dbsign, dbenc, rootca);
    if(ret != 1) { 
        std::cout << "userdb verify failed!!!" << std::endl;
        return ret;
    }
    std::cout << "userdb verify ok" << std::endl;

    std::string &cipherkey = symmetricKey["userdb"];
    std::string &cipheriv = iv["userdb"];
    int len = decrypt_aes128cbc(dbenc.c_str(), dbenc.size(), cipherkey.c_str(), cipheriv.c_str(), buffer);
    if (len == 0) { return -2; }
    else { *readLen = len; }

    std::cout << "buffer len:" << bufferSize << " hex:" << GetHexString(std::string((char*)buffer, std::min(len, 32))) << std::endl;
    return 1;
}
int SecurityManager::readEngine(unsigned char* buffer, size_t bufferSize, size_t* readLen) { 
    //no plan to implement
    return 0;
}

int SecurityManager::makeHash(unsigned char* buffer, size_t bufferSize, unsigned char* out, size_t* outLen) {
    *outLen = SHA256_DIGEST_LENGTH;
    sha256_evp(buffer, out, bufferSize);
    return 0;
}

int SecurityManager::makeHashStr(unsigned char* buffer, size_t bufferSize, unsigned char* out, size_t* outLen) {
    *outLen = SHA256_DIGEST_LENGTH * 2;
    sha256_evp(buffer, out, bufferSize);
    //std::cout << "out=" << out << std::endl;
    std::string hexString = GetHexString(std::string(reinterpret_cast<char*>(out), bufferSize));
    //std::cout << "hexString = " << hexString << std::endl;
    memcpy(out, hexString.c_str(), hexString.size());
    return 0;
}

void* SecurityManager::getSecureNeworkContext() {
    std::cout << "try to make ssl init" << std::endl;
    const SSL_METHOD* meth = TLS_server_method();
    SSL_CTX* ctx = SSL_CTX_new(meth);
    const char* cipher_list = "TLS_AES_128_GCM_SHA256";

    if (1 != SSL_CTX_set_ciphersuites(ctx, cipher_list)) {
        SSL_CTX_free(ctx);
        return nullptr;
    }
    if (1 != SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION)) {
        SSL_CTX_free(ctx);
        return nullptr;
    }
#ifdef USE_USB_KEY
    if (1 != SSL_CTX_use_certificate_chain_file(ctx, "/mnt/usb/cert/server.crt")) {
        SSL_CTX_free(ctx);
        return nullptr;
    }
    if (1 != SSL_CTX_use_PrivateKey_file(ctx, "/mnt/usb/cert/server.key", SSL_FILETYPE_PEM)) {
        SSL_CTX_free(ctx);
        return nullptr;
    }
    if (1 != SSL_CTX_load_verify_locations(ctx, "/mnt/usb/cert/rootca.crt", NULL)) {
        SSL_CTX_free(ctx);
        return nullptr;
    }
#else
    if (1 != SSL_CTX_use_certificate_chain_file(ctx, "../server.crt")) {
        SSL_CTX_free(ctx);
        return nullptr;
    }
    if (1 != SSL_CTX_use_PrivateKey_file(ctx, "../server.key", SSL_FILETYPE_PEM)) {
        SSL_CTX_free(ctx);
        return nullptr;
    }
    if (1 != SSL_CTX_load_verify_locations(ctx, "../rootca.crt", NULL)) {
        SSL_CTX_free(ctx);
        return nullptr;
    }
#endif
    if (1 != SSL_CTX_check_private_key(ctx)) {
        SSL_CTX_free(ctx);
        return nullptr;
    }
    int mode = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT | SSL_VERIFY_CLIENT_ONCE;
    SSL_CTX_set_verify(ctx, mode, NULL);
    SSL_CTX_set_verify_depth(ctx, 1);
    SSL* ssl = SSL_new(ctx); 
    if (ssl == NULL) {
        SSL_CTX_free(ctx);
        return nullptr;
    }
    std::cout << "--> complete to make ssl init" << std::endl;
    secureNetworkContext = reinterpret_cast<void*>(ctx);
    return reinterpret_cast<void*>(ssl);
}
int SecurityManager::resetSecureNetwork(void* p) {
    if (p != nullptr) {
        return SSL_clear(reinterpret_cast<SSL*>(p));
    }
    return 0;
}
int SecurityManager::shutdownSecureNetwork(void* p) {
    if (p != nullptr) {
        return SSL_shutdown(reinterpret_cast<SSL*>(p));
    }
    return 0;
}
int SecurityManager::freeSecureNetworkContext(void* p) {
    if (p != nullptr) {
        std::cout << "try to free tls connect" << std::endl;
        SSL_free(reinterpret_cast<SSL*>(p));
        SSL_CTX_free(reinterpret_cast<SSL_CTX*>(secureNetworkContext));
        std::cout << "--> complete to free tls connect" << std::endl;
    }
    return 1;
}

int SecurityManager::setSecureNetwork(void* p, int sd) {
    if (p != nullptr) {
        std::cout << "try to make tls connect" << std::endl;
        SSL* ssl = reinterpret_cast<SSL*>(p);
        SSL_set_fd(ssl, sd);
        int ret = SSL_accept(ssl);
        if (ret <= 0) {
            std::cerr << "--> fail to make tls connect" << std::endl;
        }
        std::cout << "--> complete to make tls connect" << std::endl;
    }
    return 1;
}

bool CheckKeyExist(std::unordered_map<std::string, std::string> & map) {
    bool ret = true;
    for (auto &it : map) {
        if (it.second.size() == 0) {
            std::cout << "missing :" << it.first << std::endl;
            ret = false;
        }
    }
    return ret;
}

//private
int SecurityManager::readKey() {
    std::cout << "try to loading key & certificate" << std::endl;
    hashFaceNet["facenet.engine"] = "71493446240e3f9286437c5a0baab41aae6a1e47ddbcb21a24079440ba0e5d86";
    hashFaceNet["facenet.uff"] = "7049d18ad472b535ccd022edf80707d6598ad81472028f1c0024274524ff6ce4";
    hashMtCnn["det1_relu1.engine"] = "3c26255262c050185c5fa45521a96fedda59f635457b5c8795268a749f7a4c70";
    hashMtCnn["det1_relu2.engine"] = "9aab5f7788dc385391324c1bf6e51e92c5a2fdbf2f337490bf6cbbb063032a00";
    hashMtCnn["det1_relu3.engine"] = "0c7a81f29c930acb1ce67ca64a107b74ff3917760a8e7bdcd45343d6db4f022c";
    hashMtCnn["det1_relu4.engine"] = "63866e8b088bd3e12fb4122dc569ee78e6b8c769dd529ffe62fb7f938d6659d5";
    hashMtCnn["det1_relu5.engine"] = "749a2f60a17677f3052cfca78fc6040d1b0a540766acfa70777203ed2f59f463";
    hashMtCnn["det1_relu6.engine"] = "e4ac1e0b5ff0383fb3fd612826d0ac94fa67f094350c6439ab07b88d12fd6df9";
    hashMtCnn["det1_relu7.engine"] = "fafbf0d0bd89ee02929940878b45edb80a0497bb99f881aab7b618a511434877"; 
    hashMtCnn["det2_relu.engine"] = "2253f2a34e568d0a9c033ae5028e9339918fff4d4c2832bf351d28f06b5b3ac5";
    hashMtCnn["det3_relu.engine"] = "be3c6934f2c112f34f20a3a99f1ee8bb7d272894a2fa772c99fcacf1ae419507";
    hashMtCnn["det1_relu.caffemodel"] = "b46dbcf61b858c1ec67ffdf86b805a050e25b095b5f172b8bdedd48149f2dbbf";
    hashMtCnn["det1_relu.prototxt"] = "6e1bedd5b73017623249445b43a1ca07eeb68343ff310bce4348bda9e3a50567";
    hashMtCnn["det2_relu.caffemodel"] = "053a4445c392878649aeed457ea1f3f7f5a1e23bfe29cb038c451131ed96a469";
    hashMtCnn["det2_relu.prototxt"] = "3d6986b38f98954f57be8108f1b09794e2890906f2006526172139c3b5a2bff6";
    hashMtCnn["det3_relu.caffemodel"] = "f5bf43cd05feea8fb5f7250dcc610065308e66f44b1fb2cd956bfcd43ae58c79";
    hashMtCnn["det3_relu.prototxt"] = "59f75d1ca76a78333646ff7d6c92e5866f187831f3579345ab7b62406efccf7e";

#ifdef USE_USB_KEY
    std::cout << "Read Secure Memory" << std::endl;
    std::cout << "Read Secure Memory" << std::endl;
    std::cout << "Read Secure Memory" << std::endl;
    symmetricKey["videodb"] = readFile("/mnt/usb/db/videodb.cipherkey"); //128bit aes key
    symmetricKey["facedb"] = readFile("/mnt/usb/db/facedb.cipherkey"); 
    symmetricKey["userdb"] = readFile("/mnt/usb/db/userdb.cipherkey"); 
    iv["videodb"] = readFile("/mnt/usb/db/videodb.iv");
    iv["facedb"] = readFile("/mnt/usb/db/facedb.iv");
    iv["userdb"] = readFile("/mnt/usb/db/userdb.iv");
    asymmetricKey["videodb"] = readFile("/mnt/usb/cert/videodb.key");
    asymmetricKey["userdb"] = readFile("/mnt/usb/cert/userdb.key");
    asymmetricKey["facedb"] = readFile("/mnt/usb/cert/facedb.key");
    asymmetricKey["server"] =  readFile("/mnt/usb/cert/server.key");
    certificate["videodb"] = readFile("/mnt/usb/cert/videodb.crt");
    certificate["userdb"] =  readFile("/mnt/usb/cert/userdb.crt");
    certificate["facedb"] = readFile("/mnt/usb/cert/facedb.crt");
    certificate["rootca"] = readFile("/mnt/usb/cert/rootca.crt");
    certificate["server"] = readFile("/mnt/usb/cert/server.crt");
#else
    std::cout << "Read Un-Secure Memory. compile with \"add_definitions(-DUSE_USB_KEY)\" at CMakeList.txt" << std::endl;
    std::cout << "Read Un-Secure Memory. compile with \"add_definitions(-DUSE_USB_KEY)\" at CMakeList.txt" << std::endl;
    std::cout << "Read Un-Secure Memory. compile with \"add_definitions(-DUSE_USB_KEY)\" at CMakeList.txt" << std::endl;
    symmetricKey["videodb"] = readFile("../videodb.cipherkey"); //128bit aes key
    symmetricKey["facedb"] = readFile("../facedb.cipherkey"); 
    symmetricKey["userdb"] = readFile("../userdb.cipherkey"); 
    iv["videodb"] = readFile("../videodb.iv");
    iv["facedb"] = readFile("../facedb.iv");
    iv["userdb"] = readFile("../userdb.iv");
    asymmetricKey["videodb"] = readFile("../videodb.key");
    asymmetricKey["userdb"] = readFile("../userdb.key");
    asymmetricKey["facedb"] = readFile("../facedb.key");
    asymmetricKey["server"] =  readFile("../server.key");
    certificate["videodb"] = readFile("../videodb.crt");
    certificate["userdb"] =  readFile("../userdb.crt");
    certificate["facedb"] = readFile("../facedb.crt");
    certificate["rootca"] = readFile("../rootca.crt");
    certificate["server"] = readFile("../server.crt");
#endif
    healthy = CheckKeyExist(hashFaceNet);
    healthy &= CheckKeyExist(hashMtCnn);
    healthy &= CheckKeyExist(symmetricKey);
    healthy &= CheckKeyExist(iv);
    healthy &= CheckKeyExist(asymmetricKey);
    healthy &= CheckKeyExist(certificate);
    if (!healthy) {
        std::cout << "Warning. check key & certi files" << std::endl;
        std::cout << "Warning. check key & certi files" << std::endl;
        std::cout << "Warning. check key & certi files" << std::endl << std::endl << std::endl;
        return -1;
    }
    std::cout << "key loading complete" << std::endl;
    //debug_print
    // std::cout << "symmetricKey[\"videodb\"] : size:" << symmetricKey["videodb"].size() << std::endl;
    // std::cout << "iv[\"videodb\"] : size:" << iv["videodb"].size() << std::endl;
    // std::cout << "asymmetricKey[\"videodb\"] : size:" << asymmetricKey["videodb"].size() << std::endl;
    // std::cout << "certificate[\"videodb\"] : size:" << certificate["videodb"].size() << std::endl;
    // std::cout << "certificate[\"rootca\"] : size:" << certificate["rootca"].size() << std::endl;
    return 1;
}

int SecurityManager::changeKey() { return 0;} //no plan to implement //unencrypt && generateKey && readKey && sign/enc
int SecurityManager::generateKey() { return 0;} //no plan to implement //use openssl command in shell
void debug_print_key() { }
void debug_print_size() { }
void debug_verify_db() { }


