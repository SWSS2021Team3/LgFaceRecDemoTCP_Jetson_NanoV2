#ifndef _SERIALIZE_H
#define _SERIALIZE_H
#include <string.h>
using namespace std;

class Serializable
{
public:
    virtual size_t serialize_size() const = 0;
    virtual void serialize(char *buf) const = 0;
    virtual void deserialize(const char *buf) = 0;
};

template <typename T>
class SerializableP
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
class SerializableP<std::string>
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

template <typename T>
class SerializableP<std::vector<T>>
{
public:
    static size_t serialize_size(std::vector<T> vs)
    {
        size_t s = 0;
        for (T &v : vs)
        {
            s += SerializableP<T>::serialize_size(v);
        }
        return sizeof(size_t) + s;
    }

    static char *serialize(char *target, std::vector<T> vs)
    {
        size_t length = vs.size();
        memcpy(target, &length, sizeof(size_t));
        target += sizeof(size_t);
        for (T &v : vs)
        {
            target = SerializableP<T>::serialize(target, v);
        }

        return target;
    }

    static const char *deserialize(const char *src, std::vector<T> &vs)
    {
        size_t length;
        memcpy(&length, src, sizeof(size_t));
        src += sizeof(size_t);
        for (int i = 0; i < length; i++)
        {
            vs.emplace_back();
            T &v = vs.back();
            src = SerializableP<T>::deserialize(src, v);
        }
        return src;
    }
};

#endif // _SERIALIZE_H