#include <iostream>
#include <string>

#include <objbase.h>

std::string random_UUID4()
{
    std::string strUUID;
    GUID guid;
    if (!CoCreateGuid(&guid)) {
        char buffer[64] = { 0 };
        _snprintf_s(buffer, sizeof(buffer),
            "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            guid.Data1, guid.Data2, guid.Data3,
            guid.Data4[0], guid.Data4[1], guid.Data4[2],
            guid.Data4[3], guid.Data4[4], guid.Data4[5],
            guid.Data4[6], guid.Data4[7]);
        strUUID = buffer;
    }
    return strUUID;
}