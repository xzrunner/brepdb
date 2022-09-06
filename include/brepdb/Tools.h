#pragma once

#include <cstdint>

namespace brepdb
{

class IObject
{
public:
	virtual ~IObject() = default;

	virtual IObject* Clone() = 0;

}; // IObject

class ISerializable
{
public:
	virtual ~ISerializable() = default;

	virtual uint32_t GetByteArraySize() const = 0;
	virtual void LoadFromByteArray(const uint8_t* data) = 0;
	virtual void StoreToByteArray(uint8_t** data, uint32_t& length) const = 0;

}; // ISerializable

class IObjectStream
{
public:
	virtual ~IObjectStream() = default;

	virtual IObject* GetNext() = 0;
	virtual bool HasNext() = 0;
	virtual uint32_t Size() = 0;
	virtual void Rewind() = 0;

}; // IObjectStream

}