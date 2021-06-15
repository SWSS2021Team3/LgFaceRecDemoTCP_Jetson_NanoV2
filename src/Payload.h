#ifndef _PAYLOAD_H
#define _PAYLOAD_H

#include <opencv2/core/core.hpp>
#include <vector>
#include "serialize.h"
#include <iostream>
class Payload
{
public:
	uint16_t data_id;
	uint16_t data_length;
	std::vector<uchar> data;
};

class SerializablePayload : public Serializable
{
public:
	size_t serialize_size() const
	{
		return SerializableP<uint16_t>::serialize_size(data_id) +
			SerializableP<uint16_t>::serialize_size(i1) +
			SerializableP<uint16_t>::serialize_size(i2) +
			SerializableP<std::string>::serialize_size(str1) +
			SerializableP<std::string>::serialize_size(str2);
	}
	void serialize(char* buf) const
	{
		buf = SerializableP<uint16_t>::serialize(buf, data_id);
		buf = SerializableP<uint16_t>::serialize(buf, i1);
		buf = SerializableP<uint16_t>::serialize(buf, i2);
		buf = SerializableP<std::string>::serialize(buf, str1);
		buf = SerializableP<std::string>::serialize(buf, str2);
	}
	void deserialize(const char* buf)
	{
		buf = SerializableP<uint16_t>::deserialize(buf, data_id);
		buf = SerializableP<uint16_t>::deserialize(buf, i1);
		buf = SerializableP<uint16_t>::deserialize(buf, i2);
		buf = SerializableP<std::string>::deserialize(buf, str1);
		buf = SerializableP<std::string>::deserialize(buf, str2);
	}

	uint16_t data_id;
	uint16_t i1;
	uint16_t i2;
	std::string str1;
	std::string str2;
};

#endif // _PAYLOAD_H