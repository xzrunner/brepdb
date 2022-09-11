#pragma once

#include "brepdb/SpatialIndex.h"

#include <fstream>
#include <set>
#include <map>

namespace brepdb
{

class DiskStorageManager : public IStorageManager
{
public:
	DiskStorageManager(const std::string& filename, bool overwrite = true, uint32_t page_size = 4096);
	virtual ~DiskStorageManager() override;

	virtual void LoadByteArray(const id_type id, uint32_t& len, uint8_t** data) override;
	virtual void StoreByteArray(id_type& id, const uint32_t len, const uint8_t* const data) override;
	virtual void DeleteByteArray(const id_type id) override;
	virtual void Flush() override;

private:
	bool Initialize(const std::string& filename, bool overwrite, uint32_t page_size);

private:
	class Entry
	{
	public:
		uint32_t length;
		std::vector<id_type> pages;
	};

	class LRUCollection;

	class CachePage
	{
	public:
		CachePage(id_type id, uint32_t len, const uint8_t* data) 
			: m_id(id)
			, m_len(len)
		{
			m_data = new uint8_t[len];
			memcpy(m_data, data, m_len);
		}
		~CachePage() {
			delete[] m_data;
		}

		uint32_t GetLength() const { return m_len; }
		const uint8_t* GetData() const { return m_data; }

	private:
		id_type m_id = 0;
		uint32_t m_len = 0;
		uint8_t* m_data = nullptr;

		CachePage *m_prev = nullptr, *m_next = nullptr;

		friend class LRUCollection;

	}; // CachePage

	class LRUCollection
	{
	public:
		LRUCollection(size_t capacity) 
			: m_capacity(capacity) 
		{
		}
		~LRUCollection();

		bool AddFront(id_type id, uint32_t len, const uint8_t* data);
		bool RemoveBack();

		bool Remove(id_type id);
		bool Modify(id_type id, uint32_t len, const uint8_t* data);

		const CachePage* Find(id_type id) const;
		void Touch(CachePage* page);

	private:
		size_t m_capacity = 0;

		std::map<id_type, CachePage*> m_map;
		CachePage *m_list_begin = nullptr, *m_list_end = nullptr;

	}; // LRUCollection

protected:
	std::fstream m_data_file;
	std::fstream m_index_file;

	uint32_t m_page_size = 0;
	id_type m_next_page = 0;

	std::set<id_type> m_empty_pages;
	std::map<id_type, Entry*> m_page_index;

	uint8_t* m_buffer = nullptr;

	LRUCollection m_lru;

}; // DiskStorageManager

}