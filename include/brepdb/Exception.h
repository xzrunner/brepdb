#pragma once

#include "brepdb/typedef.h"

#include <string>

namespace brepdb
{

class Exception
{
public:
	virtual std::string what() = 0;
	virtual ~Exception() = default;
};

class IndexOutOfBoundsException : public Exception
{
public:
	IndexOutOfBoundsException(size_t i);
	~IndexOutOfBoundsException() override = default;
	std::string what() override;

private:
	std::string m_error;
}; // IndexOutOfBoundsException

class IllegalArgumentException : public Exception
{
public:
	IllegalArgumentException(std::string s);
	~IllegalArgumentException() override = default;
	std::string what() override;

private:
	std::string m_error;
}; // IllegalArgumentException

class IllegalStateException : public Exception
{
public:
	IllegalStateException(std::string s);
	~IllegalStateException() override = default;
	std::string what() override;

private:
	std::string m_error;
}; // IllegalStateException

class EndOfStreamException : public Exception
{
public:
	EndOfStreamException(std::string s);
	~EndOfStreamException() override = default;
	std::string what() override;

private:
	std::string m_error;
}; // EndOfStreamException

class ResourceLockedException : public Exception
{
public:
	ResourceLockedException(std::string s);
	~ResourceLockedException() override = default;
	std::string what() override;

private:
	std::string m_error;
}; // ResourceLockedException

class NotSupportedException : public Exception
{
public:
	NotSupportedException(std::string s);
	~NotSupportedException() override = default;
	std::string what() override;

private:
	std::string m_error;
}; // NotSupportedException

class InvalidPageException : public Exception
{
public:
	InvalidPageException(id_type id);
	~InvalidPageException() override = default;
	std::string what() override;

private:
	std::string m_error;
}; // InvalidPageException

}