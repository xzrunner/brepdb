#include "brepdb/Exception.h"

#include <sstream>

namespace brepdb
{

IndexOutOfBoundsException::IndexOutOfBoundsException(size_t i)
{
	std::ostringstream s;
	s << "Invalid index " << i;
	m_error = s.str();
}

std::string IndexOutOfBoundsException::what()
{
	return "IndexOutOfBoundsException: " + m_error;
}

IllegalArgumentException::IllegalArgumentException(std::string s) : m_error(s)
{
}

std::string IllegalArgumentException::what()
{
	return "IllegalArgumentException: " + m_error;
}

IllegalStateException::IllegalStateException(std::string s) : m_error(s)
{
}

std::string IllegalStateException::what()
{
	return "IllegalStateException: " + m_error;
}

EndOfStreamException::EndOfStreamException(std::string s) : m_error(s)
{
}

std::string EndOfStreamException::what()
{
	return "EndOfStreamException: " + m_error;
}

ResourceLockedException::ResourceLockedException(std::string s) : m_error(s)
{
}

std::string ResourceLockedException::what()
{
	return "ResourceLockedException: " + m_error;
}

NotSupportedException::NotSupportedException(std::string s) : m_error(s)
{
}

std::string NotSupportedException::what()
{
	return "NotSupportedException: " + m_error;
}

InvalidPageException::InvalidPageException(id_type id)
{
	std::ostringstream s;
	s << "Unknown page id " << id;
	m_error = s.str();
}

std::string InvalidPageException::what()
{
	return "InvalidPageException: " + m_error;
}

}