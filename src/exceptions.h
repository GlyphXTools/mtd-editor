//
// This file defines all the exceptions
//
#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include "types.h"
#include <string>

class wexception
{
	std::wstring m_message;

public:
	const wchar_t* what() const {
		return m_message.c_str();
	}

    wexception(const std::wstring& message) : m_message(message) {}
	virtual ~wexception() {}
};

class wruntime_error : public wexception
{
public:
    wruntime_error(const std::wstring& message) : wexception(message) {}
};

#endif