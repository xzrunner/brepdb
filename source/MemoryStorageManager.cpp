#include "brepdb/MemoryStorageManager.h"
#include "brepdb/Exception.h"

#include <stdexcept>

namespace brepdb
{
	
MemoryStorageManager::~MemoryStorageManager()
{
	for (auto buf : m_buffer) {
		delete buf;
	}
}

void MemoryStorageManager::LoadByteArray(const id_type page, uint32_t& len, uint8_t** data)
{
	Entry* e;
	try
	{
		e = m_buffer.at(page);
		if (e == nullptr) {
			throw InvalidPageException(page);
		}
	}
	catch (std::out_of_range&)
	{
		throw InvalidPageException(page);
	}

	len = e->m_length;
	*data = new uint8_t[len];

	memcpy(*data, e->m_data, len);
}

void MemoryStorageManager::StoreByteArray(id_type& page, const uint32_t len, const uint8_t* const data)
{
	if (page == NewPage)
	{
		Entry* e = new Entry(len, data);

		if (m_empty_pages.empty())
		{
			m_buffer.push_back(e);
			page = m_buffer.size() - 1;
		}
		else
		{
			page = m_empty_pages.top(); m_empty_pages.pop();
			m_buffer[page] = e;
		}
	}
	else
	{
		Entry* e_old;
		try
		{
			e_old = m_buffer.at(page);
			if (e_old == nullptr) {
				throw InvalidPageException(page);
			}
		}
		catch (std::out_of_range&)
		{
			throw InvalidPageException(page);
		}

		Entry* e = new Entry(len, data);

		delete e_old;
		m_buffer[page] = e;
	}
}

void MemoryStorageManager::DeleteByteArray(const id_type page)
{
	Entry* e;
	try
	{
		e = m_buffer.at(page);
		if (e == nullptr) throw InvalidPageException(page);
	}
	catch (std::out_of_range&)
	{
		throw InvalidPageException(page);
	}

	m_buffer[page] = nullptr;
	m_empty_pages.push(page);

	delete e;
}

void MemoryStorageManager::Flush()
{
}

}