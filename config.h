#ifndef READCFG_H
#define READCFG_H

#include <string>
#include <map>

class Configuration
{
public:
    std::map<std::string, std::string> dictionary;
    void freememory();
    void read(const char* filename);
    Configuration()
    {
        dictionary = std::map<std::string, std::string>();
    }
};
#endif