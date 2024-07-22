#pragma once // Only load this file once (#include)
#ifndef __MAIN_H // Include guard; makes sure that this file is only included once and subsequent #includes are ignored
#define __MAIN_H // Include guard

#include "VCMP.h"
#include <cstdio>

#ifdef _WIN32 // If we're on windows
#include "Windows.h"

#define DLL_EXPORT __declspec( dllexport ) // This is required in windows so that other programs can access the function
#else // We're not on windows
#define DLL_EXPORT // Nothing like that is required in linux so we leave it blank
#define MAX_PATH 250
#endif

#ifdef __cplusplus // We don't need to (and can not) extern "C" if we're already in C. so we make sure that we are in C++
extern "C" { // We need to extern "C" the function because windows expects a C-style function
#endif
    DLL_EXPORT    unsigned int            VcmpPluginInit(PluginFuncs* pluginFuncs, PluginCallbacks* pluginCalls, PluginInfo* pluginInfo); // This is a blank function and it's exported (check the #define tags above). VCMP requires it
#ifdef __cplusplus // We opened a brace in C++, so we need to close it in C++ only as well
}
#endif

#endif // Include guard