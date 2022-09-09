#include "brepdb/DiskStorageManager.h"
#include "brepdb/Exception.h"

#include <filesystem>

namespace brepdb
{

DiskStorageManager::DiskStorageManager(const std::string& filename, bool overwrite, uint32_t page_size)
	: m_page_size(0)
	, m_next_page(-1)
	, m_buffer(nullptr)
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
	auto it = m_page_index.find(page);
	if (it == m_page_index.end()) {
		throw InvalidPageException(page);
	}

	std::vector<id_type>& pages = (*it).second->pages;
	uint32_t c_next = 0;
	uint32_t c_total = static_cast<uint32_t>(pages.size());

	len = (*it).second->length;
	*data = new uint8_t[len];

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
	}
}

void DiskStorageManager::DeleteByteArray(const id_type page)
{
	auto it = m_page_index.find(page);
	if (it == m_page_index.end()) {
		throw InvalidPageException(page);
	}

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

}