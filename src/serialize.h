#ifndef _SERIALIZE_H
#define _SERIALIZE_H
#include <string.h>
using namespace std;

class Serializable
{
public:
    virtual size_t serialize_size() const = 0;
    virtual void serialize(char* buf) const = 0;
    virtual void deserialize(const char* buf) = 0;
};

template<typename T>
class SerializableP
{
public:
    static size_t serialize_size(T v);
    static char* serialize(char* target, T v);
    static const char* deserialize(const char* src, T& lv);
};

#endif // _SERIALIZE_H