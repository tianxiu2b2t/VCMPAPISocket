#pragma once
#include <string>
#include <string.h>
#if defined(_WIN32) || defined(_MSC_VER) || defined(WIN64) 
#include <Windows.h>
#elif defined(__linux__) || defined(__GNUC__)
#include <iconv.h>
#endif

#if defined(__linux__) || defined(__GNUC__)
int EncodingConvert(const char* charsetSrc, const char* charsetDest, char* inbuf,
	size_t inSz, char* outbuf, size_t outSz)
{
	iconv_t cd;
	char** pin = &inbuf;
	char** pout = &outbuf;
	cd = iconv_open(charsetDest, charsetSrc);
	if (0 == cd)
	{
		std::cerr << charsetSrc << " to " << charsetDest
			<< " conversion not available" << std::endl;
		return -1;
	}

	if (-1 == static_cast<int>(iconv(cd, pin, &inSz, pout, &outSz)))
	{
		std::cerr << "conversion failure" << std::endl;
		return -1;
	}

	iconv_close(cd);
	**pout = '\0';
	return 0;
}
#endif

std::string GbkToUtf8(const std::string& str)
{
#if defined(_WIN32) || defined(_MSC_VER) || defined(WIN64)
	int len = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);
	wchar_t* wstr = new wchar_t[len + 1ull];
	memset(wstr, 0, len + 1ull);
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, wstr, len);
	len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
	char* cstr = new char[len + 1ull];
	memset(cstr, 0, len + 1ull);
	WideCharToMultiByte(CP_UTF8, 0, wstr, -1, cstr, len, NULL, NULL);
	std::string res(cstr);

	if (wstr) delete[] wstr;
	if (cstr) delete[] cstr;

	return res;
#elif defined(__linux__) || defined(__GNUC__)
	size_t len = str.size() * 2 + 1;
	char* temp = new char[len];
	if (EncodingConvert("gb2312", "utf-8", const_cast<char*>(str.c_str()), str.size(), temp, len)
		> = 0)
	{
		std::string res;
		res.append(temp);
		delete[] temp;
		return res;
	}
	else
	{
		delete[]temp;
		return str;
	}
#else
	std::cerr << "Unhandled operating system." << std::endl;
	return str;
#endif
}
std::string Utf8ToGbk(const std::string& str)
{
#if defined(_WIN32) || defined(_MSC_VER) || defined(WIN64) 
	// calculate length
	int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
	wchar_t* wsGbk = new wchar_t[len + 1ull];
	// set to '\0'
	memset(wsGbk, 0, len + 1ull);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wsGbk, len);
	len = WideCharToMultiByte(CP_ACP, 0, wsGbk, -1, NULL, 0, NULL, NULL);
	char* csGbk = new char[len + 1ull];
	memset(csGbk, 0, len + 1ull);
	WideCharToMultiByte(CP_ACP, 0, wsGbk, -1, csGbk, len, NULL, NULL);
	std::string res(csGbk);

	if (wsGbk)
	{
		delete[] wsGbk;
	}

	if (csGbk)
	{
		delete[] csGbk;
	}

	return res;
#elif defined(__linux__) || defined(__GNUC__)
	size_t len = str.size() * 2 + 1;
	char* temp = new char[len];
	if (EncodingConvert("utf-8", "gb2312", const_cast<char*>(str.c_str()),
		str.size(), temp, len) >= 0)
	{
		std::string res;
		res.append(temp);
		delete[] temp;
		return res;
	}
	else
	{
		delete[] temp;
		return str;
	}

#else
	std::cerr << "Unhandled operating system." << std::endl;
	return str;
#endif
}