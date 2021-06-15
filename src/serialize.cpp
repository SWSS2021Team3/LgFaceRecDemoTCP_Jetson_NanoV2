#include "serialize.h"
#include <string>

using namespace std;

template<typename T>
size_t SerializableP<T>::serialize_size(T v)
{
	return sizeof(T);
}

template<typename T>
char* SerializableP<T>::serialize(char* target, T v)
{
	size_t size = serialize_size(v);
	memcpy(target, &v, size);
	return target + size;
}

template<typename T>
const char* SerializableP<T>::deserialize(const char* src, T& lv)
{
	size_t size = serialize_size(lv);
	memcpy(&lv, src, size);
	return src + size;
}

template<>
size_t SerializableP<std::string>::serialize_size(std::string v)
{
	return sizeof(size_t) + v.length() + 1;
}

template<>
char* SerializableP<std::string>::serialize(char* target, std::string v)
{
	size_t length = v.length();
	memcpy(target, &length, sizeof(size_t));
	memcpy(target + sizeof(size_t), v.c_str(), length);
	memset(target + sizeof(size_t) + length, 0, 1);	// end of string
	return target + serialize_size(v);
}

template<>
const char* SerializableP<std::string>::deserialize(const char* src, std::string& v)
{
	size_t length;
	memcpy(&length, src, sizeof(size_t));
	v = (src + sizeof(size_t));
	return src + serialize_size(v);
}

void GenerateTemplateMethods()
{
	int i = 9;

	char* buf = new char[SerializableP<int>::serialize_size(i)];
	SerializableP<int>::serialize(buf, i);
	SerializableP<int>::deserialize(buf, i);
	delete[] buf;

	uint16_t us = 0;

	buf = new char[SerializableP<uint16_t>::serialize_size(us)];
	SerializableP<uint16_t>::serialize(buf, us);
	SerializableP<uint16_t>::deserialize(buf, us);
	delete[] buf;

}