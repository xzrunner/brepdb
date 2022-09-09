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

protected:
	std::fstream m_data_file;
	std::fstream m_index_file;

	uint32_t m_page_size = 0;
	id_type m_next_page = 0;

	std::set<id_type> m_empty_pages;
	std::map<id_type, Entry*> m_page_index;

	uint8_t* m_buffer = nullptr;

}; // DiskStorageManager

}