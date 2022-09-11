#include "brepdb/DiskStorageManager.h"
#include "brepdb/Exception.h"

#include <filesystem>

#include <assert.h>

namespace brepdb
{

DiskStorageManager::DiskStorageManager(const std::string& filename, bool overwrite, uint32_t page_size)
	: m_page_size(0)
	, m_next_page(-1)
	, m_buffer(nullptr)
	, m_lru(4096)
{
	Initialize(filename, overwrite, page_size);
}

DiskStorageManager::~DiskStorageManager()
{
	Flush();
	m_index_file.close();
	m_data_file.close();
	if (m_buffer != nullptr) {
		delete[] m_buffer;
	}

	for (auto& v : m_page_index) {
		delete v.second;
	}
}

void DiskStorageManager::LoadByteArray(const id_type page, uint32_t& len, uint8_t** data)
{
	const CachePage* cp = m_lru.Find(page);
	if (cp)
	{
		m_lru.Touch(const_cast<CachePage*>(cp));

		len = cp->GetLength();
		*data = new uint8_t[len];
		assert(*data);
		memcpy(*data, cp->GetData(), len);

		return;
	}

	auto it = m_page_index.find(page);
	if (it == m_page_index.end()) {
		throw InvalidPageException(page);
	}

	std::vector<id_type>& pages = (*it).second->pages;
	uint32_t c_next = 0;
	uint32_t c_total = static_cast<uint32_t>(pages.size());

	len = (*it).second->length;
	*data = new uint8_t[len];
	assert(*data);

	uint8_t* ptr = *data;
	uint32_t c_len = 0;
	uint32_t c_rem = len;

	do
	{
		m_data_file.seekg(pages[c_next] * m_page_size, std::ios_base::beg);
		if (m_data_file.fail()) {
			throw IllegalStateException("DiskStorageManager: Corrupted data file.");
		}

		m_data_file.read(reinterpret_cast<char*>(m_buffer), m_page_size);
		if (m_data_file.fail()) {
			throw IllegalStateException("DiskStorageManager: Corrupted data file.");
		}

		c_len = (c_rem > m_page_size) ? m_page_size : c_rem;
		memcpy(ptr, m_buffer, c_len);

		ptr += c_len;
		c_rem -= c_len;
		++c_next;
	}
	while (c_next < c_total);

	m_lru.AddFront(page, len, *data);
}

void DiskStorageManager::StoreByteArray(id_type& page, const uint32_t len, const uint8_t* const data)
{
	if (page == NewPage)
	{
		Entry* e = new Entry();
		e->length = len;

		const uint8_t* ptr = data;
		id_type c_page = 0;
		uint32_t c_rem = len;
		uint32_t c_len;

		while (c_rem > 0)
		{
			if (!m_empty_pages.empty())
			{
				c_page = *m_empty_pages.begin();
				m_empty_pages.erase(m_empty_pages.begin());
			}
			else
			{
				c_page = m_next_page;
				++m_next_page;
			}

			c_len = (c_rem > m_page_size) ? m_page_size : c_rem;
			memcpy(m_buffer, ptr, c_len);

			m_data_file.seekp(c_page * m_page_size, std::ios_base::beg);
			if (m_data_file.fail()) {
				throw IllegalStateException("DiskStorageManager: Corrupted data file.");
			}

			m_data_file.write(reinterpret_cast<const char*>(m_buffer), m_page_size);
			if (m_data_file.fail()) {
				throw IllegalStateException("DiskStorageManager: Corrupted data file.");
			}

			ptr += c_len;
			c_rem -= c_len;
			e->pages.push_back(c_page);
		}

		page = e->pages[0];
		m_page_index.insert(std::pair<id_type, Entry*>(page, e));

		m_lru.AddFront(page, len, data);
	}
	else
	{
		auto it = m_page_index.find(page);
		if (it == m_page_index.end()) {
			throw InvalidPageException(page);
		}

		Entry* old_entry = (*it).second;

		m_page_index.erase(it);

		Entry* e = new Entry();
		e->length = len;

		const uint8_t* ptr = data;
		id_type c_page = 0;
		uint32_t c_rem = len;
		uint32_t c_len, c_next = 0;

		while (c_rem > 0)
		{
			if (c_next < old_entry->pages.size())
			{
				c_page = old_entry->pages[c_next];
				++c_next;
			}
			else if (! m_empty_pages.empty())
			{
				c_page = *m_empty_pages.begin();
				m_empty_pages.erase(m_empty_pages.begin());
			}
			else
			{
				c_page = m_next_page;
				++m_next_page;
			}

			c_len = (c_rem > m_page_size) ? m_page_size : c_rem;
			memcpy(m_buffer, ptr, c_len);

			m_data_file.seekp(c_page * m_page_size, std::ios_base::beg);
			if (m_data_file.fail()) {
				throw IllegalStateException("DiskStorageManager: Corrupted data file.");
			}

			m_data_file.write(reinterpret_cast<const char*>(m_buffer), m_page_size);
			if (m_data_file.fail()) {
				throw IllegalStateException("DiskStorageManager: Corrupted data file.");
			}

			ptr += c_len;
			c_rem -= c_len;
			e->pages.push_back(c_page);
		}

		while (c_next < old_entry->pages.size())
		{
			m_empty_pages.insert(old_entry->pages[c_next]);
			++c_next;
		}

		m_page_index.insert(std::pair<id_type, Entry*>(page, e));
		delete old_entry;

		m_lru.Modify(page, len, data);
	}	
}

void DiskStorageManager::DeleteByteArray(const id_type page)
{
	auto it = m_page_index.find(page);
	if (it == m_page_index.end()) {
		throw InvalidPageException(page);
	}

	m_lru.Remove(page);

	for (uint32_t i = 0; i < (*it).second->pages.size(); ++i) {
		m_empty_pages.insert((*it).second->pages[i]);
	}

	delete (*it).second;
	m_page_index.erase(it);
}

void DiskStorageManager::Flush()
{
	m_index_file.seekp(0, std::ios_base::beg);
	if (m_index_file.fail()) {
		throw IllegalStateException("DiskStorageManager: Corrupted storage manager index file.");
	}

	m_index_file.write(reinterpret_cast<const char*>(&m_page_size), sizeof(uint32_t));
	if (m_index_file.fail()) {
		throw IllegalStateException("DiskStorageManager: Corrupted storage manager index file.");
	}

	m_index_file.write(reinterpret_cast<const char*>(&m_next_page), sizeof(id_type));
	if (m_index_file.fail()) {
		throw IllegalStateException("DiskStorageManager: Corrupted storage manager index file.");
	}

	uint32_t count = static_cast<uint32_t>(m_empty_pages.size());
	m_index_file.write(reinterpret_cast<const char*>(&count), sizeof(uint32_t));
	if (m_index_file.fail()) {
		throw IllegalStateException("DiskStorageManager: Corrupted storage manager index file.");
	}

	for (auto page : m_empty_pages)
	{
		m_index_file.write(reinterpret_cast<const char*>(&page), sizeof(id_type));
		if (m_index_file.fail()) {
			throw IllegalStateException("DiskStorageManager: Corrupted storage manager index file.");
		}
	}

	count = static_cast<uint32_t>(m_page_index.size());
	m_index_file.write(reinterpret_cast<const char*>(&count), sizeof(uint32_t));
	if (m_index_file.fail()) {
		throw IllegalStateException("DiskStorageManager: Corrupted storage manager index file.");
	}

	for (auto& pair : m_page_index)
	{
		m_index_file.write(reinterpret_cast<const char*>(&(pair.first)), sizeof(id_type));
		if (m_index_file.fail()) {
			throw IllegalStateException("DiskStorageManager: Corrupted storage manager index file.");
		}

		m_index_file.write(reinterpret_cast<const char*>(&(pair.second->length)), sizeof(uint32_t));
		if (m_index_file.fail()) {
			throw IllegalStateException("DiskStorageManager: Corrupted storage manager index file.");
		}

		count = static_cast<uint32_t>(pair.second->pages.size());
		m_index_file.write(reinterpret_cast<const char*>(&count), sizeof(uint32_t));
		if (m_index_file.fail()) {
			throw IllegalStateException("DiskStorageManager: Corrupted storage manager index file.");
		}

		for (auto page : pair.second->pages)
		{
			m_index_file.write(reinterpret_cast<const char*>(&page), sizeof(id_type));
			if (m_index_file.fail()) {
				throw IllegalStateException("DiskStorageManager: Corrupted storage manager index file.");
			}
		}
	}

	m_index_file.flush();
	m_data_file.flush();
}

bool DiskStorageManager::Initialize(const std::string& filename, bool overwrite, uint32_t page_size)
{
	const std::string index_file = filename + ".idx";
	const std::string data_file = filename + ".dat";

	std::ios_base::openmode mode = std::ios::in | std::ios::out | std::ios::binary;
	const bool files_exists = std::filesystem::exists(index_file) && 
		std::filesystem::exists(index_file);
	if (!files_exists || overwrite) {
		mode |= std::ios::trunc;
	}

	m_index_file.open(index_file.c_str(), mode);
	m_data_file.open(data_file.c_str(), mode);
	if (m_index_file.fail() || m_data_file.fail()) {
		return false;
	}

	m_index_file.seekg(0, m_index_file.end);
	std::streamoff length = m_index_file.tellg();
	m_index_file.seekg(0, m_index_file.beg);

	if (overwrite || length == 0 || !files_exists)
	{
		m_page_size = page_size;
		m_next_page = 0;
	}
	else
	{
		m_index_file.read(reinterpret_cast<char*>(&m_page_size), sizeof(uint32_t));
		if (m_index_file.fail()) {
			return false;
		}

		m_index_file.read(reinterpret_cast<char*>(&m_next_page), sizeof(id_type));
		if (m_index_file.fail()) {
			return false;
		}
	}

	m_buffer = new uint8_t[m_page_size];
	assert(m_buffer);
	memset(m_buffer, 0, m_page_size);

	if (!overwrite && length > 0)
	{
		uint32_t count;
		id_type page, id;

		m_index_file.read(reinterpret_cast<char*>(&count), sizeof(uint32_t));
		if (m_index_file.fail()) {
			return false;
		}

		for (uint32_t i = 0; i < count; ++i)
		{
			m_index_file.read(reinterpret_cast<char*>(&page), sizeof(id_type));
			if (m_index_file.fail()) {
				return false;
			}
			m_empty_pages.insert(page);
		}

		m_index_file.read(reinterpret_cast<char*>(&count), sizeof(uint32_t));
		if (m_index_file.fail()) {
			return false;
		}

		for (uint32_t i = 0; i < count; ++i)
		{
			Entry* e = new Entry();

			m_index_file.read(reinterpret_cast<char*>(&id), sizeof(id_type));
			if (m_index_file.fail()) {
				return false;
			}

			m_index_file.read(reinterpret_cast<char*>(&(e->length)), sizeof(uint32_t));
			if (m_index_file.fail()) {
				return false;
			}

			uint32_t count2;
			m_index_file.read(reinterpret_cast<char*>(&count2), sizeof(uint32_t));
			if (m_index_file.fail()) {
				return false;
			}

			for (uint32_t j = 0; j < count2; ++j)
			{
				m_index_file.read(reinterpret_cast<char*>(&page), sizeof(id_type));
				if (m_index_file.fail()) {
					return false;
				}
				e->pages.push_back(page);
			}
			m_page_index.insert(std::pair<id_type, Entry*>(id, e));
		}
	}

	return true;
}

//
// class DiskStorageManager::LRUCollection
//

DiskStorageManager::LRUCollection::~LRUCollection()
{
	for (auto itr : m_map) {
		delete itr.second;
	}
}

bool DiskStorageManager::LRUCollection::AddFront(id_type id, uint32_t len, const uint8_t* data)
{
	while (m_map.size() >= m_capacity) {
		RemoveBack();
	}

	CachePage* front = new CachePage(id, len, data);
	m_map.insert({ id, front });

	front->m_prev = nullptr;
	front->m_next = m_list_begin;

	if (m_list_begin)
	{
		m_list_begin->m_prev = front;
		m_list_begin = front;
	}
	else
	{
		m_list_begin = front;
		m_list_end = front;
	}

	return true;
}

bool DiskStorageManager::LRUCollection::RemoveBack()
{
	if (m_map.empty()) {
		return false;
	}

	assert(m_list_end);
	CachePage* back = m_list_end;

	m_map.erase(back->m_id);

	if (m_list_begin == m_list_end)
	{
		m_list_begin = nullptr;
		m_list_end = nullptr;
	}
	else
	{
		if (back->m_prev) {
			back->m_prev->m_next = nullptr;
		}
		m_list_end = back->m_prev;
		m_list_end->m_next = nullptr;
	}

	delete back;

	return true;
}

bool DiskStorageManager::LRUCollection::Remove(id_type id)
{
	auto itr = m_map.find(id);
	if (itr == m_map.end()) {
		return false;
	}

	auto page = itr->second;

	m_map.erase(itr);

	if (m_list_begin == m_list_end)
	{
		m_list_begin = nullptr;
		m_list_end = nullptr;
	}
	else
	{
		if (page->m_prev) {
			page->m_prev->m_next = page->m_next;
		}
		if (page->m_next) {
			page->m_next->m_prev = page->m_prev;
		}

		if (m_list_begin == page) {
			m_list_begin = page->m_next;
		}
		if (m_list_end == page) {
			m_list_end = page->m_prev;
		}
	}

	delete page;

	return true;
}

bool DiskStorageManager::LRUCollection::Modify(id_type id, uint32_t len, const uint8_t* data)
{
	auto itr = m_map.find(id);
	if (itr == m_map.end()) {
		return false;
	}

	auto page = itr->second;

	page->m_len = len;
	delete[] page->m_data;
	page->m_data = new uint8_t[len];
	assert(page->m_data);
	memcpy(page->m_data, data, len);

	Touch(page);

	return true;
}

const DiskStorageManager::CachePage* 
DiskStorageManager::LRUCollection::Find(id_type id) const
{
	auto itr = m_map.find(id);
	if (itr != m_map.end()) {
		return itr->second;
	} else {
		return nullptr;
	}
}

void DiskStorageManager::LRUCollection::Touch(CachePage* page)
{
	if (m_map.empty() || !page) {
		return;
	}

	CachePage* curr = page;
	if (curr == m_list_begin) {
		return;
	}
	if (curr == m_list_end) {
		m_list_end = curr->m_prev;
	}

	auto prev = curr->m_prev;
	auto next = curr->m_next;
	if (prev) {
		prev->m_next = next;
	}
	if (next) {
		next->m_prev = prev;
	}

	curr->m_next = m_list_begin;
	if (m_list_begin) {
		m_list_begin->m_prev = curr;
	}
	curr->m_prev = nullptr;
	m_list_begin = curr;
}

}