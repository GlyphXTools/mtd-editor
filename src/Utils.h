#ifndef UTILS_H
#define UTILS_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

std::wstring AnsiToWide(const char* cstr);
std::wstring FormatString(const wchar_t* format, ...);
std::wstring LoadString(UINT id, ...);

#endif