#include "nlohmann/json.hpp"
#include <string>
#include <string.h>
#ifndef DATAPARSER_H
#define DATAPARSER_H
using json = nlohmann::json;

class DataContent
{
public:
	bool status;
	json data;
	std::string msg;
};


DataContent parseResponseData(std::string response);

class WebSocketResult
{
public:
	int length;
	std::string echo;
};

class WebSocketData
{
public:
	bool status;
	json data;
	std::string type;
	std::string echo;
	std::string msg;
};
struct WSCallback_Imp { virtual void operator()(const WebSocketData& message) = 0; };

template<class Callable>
WSCallback_Imp dispatch(Callable callable_)
// For callbacks that accept a string argument.
{ // N.B. this is compatible with both C++11 lambdas, functors and C function pointers
    struct _Callback : public WSCallback_Imp {
        Callable& callable;
        _Callback(Callable& callable__) : callable(callable__) { }
        void operator()(const std::string& message) { callable(message); }
    };
    _Callback callback(callable_);
    return callback;
}
WebSocketData parseWebSocketData(std::string response);

#endif