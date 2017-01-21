#include "Utils.h"
using namespace std;

wstring AnsiToWide(const char* cstr)
{
	int size = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, cstr, -1, NULL, 0);
	WCHAR* wstr = new WCHAR[size];
	try
	{
		MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, cstr, -1, wstr, size);
		wstring result(wstr);
		delete[] wstr;
		return result;
	}
	catch (...)
	{
		delete[] wstr;
		throw;
	}
}

static wstring FormatString(const wchar_t* format, va_list args)
{
    int      n   = _vscwprintf(format, args);
    wchar_t* buf = new wchar_t[n + 1];
    try
    {
        vswprintf(buf, n + 1, format, args);
        wstring str = buf;
        delete[] buf;
        return str;
    }
    catch (...)
    {
        delete[] buf;
        throw;
    }
}

wstring FormatString(const wchar_t* format, ...)
{
    va_list args;
    va_start(args, format);
    wstring str = FormatString(format, args);
    va_end(args);
    return str;
}

wstring LoadString(UINT id, ...)
{
    int len = 256;
    TCHAR* buf = new TCHAR[len];
    try
    {
        while (::LoadString(NULL, id, buf, len) >= len - 1)
        {
            delete[] buf;
            buf = NULL;
            buf = new TCHAR[len *= 2];
        }
        va_list args;
        va_start(args, id);
        wstring str = FormatString(buf, args);
        va_end(args);
        delete[] buf;
        return str;
    }
    catch (...)
    {
        delete[] buf;
        throw;
    }
}