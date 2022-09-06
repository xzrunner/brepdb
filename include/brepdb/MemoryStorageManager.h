#pragma once

#include "brepdb/SpatialIndex.h"

#include <stack>

namespace brepdb
{

class MemoryStorageManager : public IStorageManager
{
public:
	MemoryStorageManager() {}
	virtual ~MemoryStorageManager() override;

	virtual void LoadByteArray(const id_type id, uint32_t& len, uint8_t** data) override;
	virtual void StoreByteArray(id_type& id, const uint32_t len, const uint8_t* const data) override;
	virtual void DeleteByteArray(const id_type id) override;
	virtual void Flush() override;

private:
	class Entry
	{
	public:
		uint8_t* m_data;
		uint32_t m_length;

		Entry(uint32_t l, const uint8_t* const d) 
			: m_data(nullptr), m_length(l)
		{
			m_data = new uint8_t[m_length];
			memcpy(m_data, d, m_length);
		}

		~Entry() { delete[] m_data; }
	}; // Entry

	std::vector<Entry*> m_buffer;
	std::stack<id_type> m_empty_pages;

}; // MemoryStorageManager

}