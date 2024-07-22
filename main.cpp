#include "main.h"
#include "VCMP.h"
#include "logger.h"
#include "config.h"
#include "curl/curl.h"
#include "nlohmann/json.hpp"
#include "requests.h"
#include "hmac_sha256.h"
#include "dataParser.h"
#include "easywsclient.hpp"
#include "SQImports.h"
#include "commandID.h"

#include <time.h>
#include <sstream>
#include <string.h>
#include <string>
#include <stdio.h>
#include <ctime>
#include <iostream>
#include <random>

#define AUTHOR "TTB-Network"
#define VERSION "1.0.0"
#define PLUGIN_NAME "VCMPAPISocket"

PluginFuncs* Server;
PluginCallbacks* Callbacks;
HSQAPI sq = NULL;
HSQUIRRELVM v = NULL;
HSQOBJECT container;

std::string base_url = "ws://localhost/";
std::string vcmp_id = "";
std::string vcmp_secret = "";
std::string token = "";

std::map<std::string, std::function<void(WebSocketData)>> callbacks;
std::map<std::string, WebSocketResult> results;
unsigned long long tokenExpires = 0;

easywsclient::WebSocket::pointer websocket = NULL;
int reconnect_timestamp = 0;
int reconnect_state = 0;
int ws_keepalive = 0;

using json = nlohmann::json;
unsigned long long GetFixedTickCount() {
#ifndef WIN32
#include <time.h>
	struct timespec ts;
	unsigned long long theTick = 0U;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	theTick = ts.tv_nsec / 1000000;
	theTick += ts.tv_sec * 1000;
	return theTick;
#else
	return static_cast<unsigned long long>(GetTickCount64());
#endif
}

std::string random_UUID4() {
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, 15);

	std::string uuid;
	uuid.reserve(36);

	auto generateHexPart = [&](int count) {
		std::string part;
		std::generate_n(std::back_inserter(part), count, [&]() {
			int randomValue = dis(gen);
			return "0123456789abcdef"[randomValue];
			});
		return part;
		};

	uuid += generateHexPart(8);
	uuid += '-';
	uuid += generateHexPart(4);
	uuid += '-';
	uuid += "4";
	uuid += generateHexPart(3);
	uuid += '-';
	uuid += "89ab";
	uuid += generateHexPart(12);

	return uuid;
}

std::string str_replace_all(std::string origin, std::string old, std::string news) {
	if (origin.length() < old.length()) return origin;
	std::string result = "";
	std::string copy_origin = std::string(origin.c_str());
	int p = 0;
	while ((p = copy_origin.find(old)) != -1) {
		result += copy_origin.substr(0, p);
		result += news;
		copy_origin = copy_origin.substr(p + news.length());
	}
	result += copy_origin;
	return result;
}

std::string hmac_sha256_hexdigest(const std::string& key, const std::string& message) {
	std::stringstream ss_result;
	std::vector<uint8_t> out(SHA256_HASH_SIZE);

	hmac_sha256(key.data(), key.size(), message.data(), message.size(),
		out.data(), out.size());
	for (uint8_t x : out) {
		ss_result << std::hex << std::setfill('0') << std::setw(2) << (int)x;
	}
	return ss_result.str();
}

std::string getToken() {
	if (tokenExpires <= GetFixedTickCount()) {
		token = "";
	}
	if (token.empty()) {
		AsyncHttpConnection connection;
		DataContent data = parseResponseData(connection.asyncGet(base_url + "/vcmp-agent/challenge?id=" + vcmp_id));
		if (!data.status) {
			Logger::error(data.msg);
			return token;
		}
		std::string challenge = data.data.at("challenge");
		std::string signature = hmac_sha256_hexdigest(vcmp_secret, challenge);

		Logger::debug("The challenge has been fetched.");

		data = parseResponseData(connection.asyncPost(base_url + "/vcmp-agent/token", "{\"id\":\"" + vcmp_id + "\",\"challenge\":\"" + challenge + "\",\"signature\":\"" + signature + "\"}"));

		const int ttl = data.data.at("ttl");

		Logger::debug("The token has been obtained. Its validity period is: " + std::to_string(ttl) + " seconds.");
		tokenExpires = GetFixedTickCount() + data.data.at("ttl") * 1000;
		token = data.data.at("token");
	}
	return token;
}
int raw_send(const char* content) {
	if (websocket->getReadyState() == easywsclient::WebSocket::OPEN) {
		websocket->send(content);
		websocket->poll();
		return strlen(content);
	}
	return -1;
}
WebSocketResult raw_send(std::string type, json data) {
	std::string echo = random_UUID4();
	WebSocketResult result;
	result.length = raw_send((std::string("{\"type\":\"") + type + std::string("\",\"data\":") + std::string(data.dump(0)) + std::string(",\"echo\":\"" + echo + "\"}")).c_str());
	result.echo = echo;
	return result;
}
WebSocketResult send_with_callback(std::string type, json data, std::function<void(WebSocketData)> callback) {
	WebSocketResult result = raw_send(type, data);
	callbacks[result.echo] = callback;
	return result;
}
WebSocketResult send(std::string type, json data, std::function<void(WebSocketData)> callback) {
	return send_with_callback(type, data, callback);
}
WebSocketResult send(std::string type, std::string data, std::function<void(WebSocketData)> callback) {
	json jsonData;
	try {
		jsonData = json::parse(data.c_str());
	}
	catch (const json::parse_error& e) {
		jsonData = json::parse("\"" + str_replace_all(data, "\"", "'") + "\"");
	}
	return send_with_callback(type, jsonData, callback);
}
WebSocketResult send(std::string type, json data) {
	return raw_send(type, data);
}
WebSocketResult send(std::string type, std::string data) {
	json jsonData;
	try {
		jsonData = json::parse(data.c_str());
	}
	catch (const json::parse_error& e) {
		jsonData = json::parse("\"" + str_replace_all(data, "\"", "'") + "\"");
	}
	return send(type, jsonData);
}
void StartWebSocket() {

	websocket = easywsclient::WebSocket::from_url("ws" + base_url.substr(base_url.find(":")) + "/vcmp/socket");
	if (!websocket) {
		Logger::error("Websocket not connect.");
		return;
	}
	Server->SendPluginCommand(VCMPAPISocket_CONNECT, "");
	send(std::string("auth"), getToken(), [](WebSocketData data) {
		if (data.status) {
			Logger::success("Verification successful. Welcome back: " + std::string(data.data) + "!");
			Server->SendPluginCommand(VCMPAPISocket_AUTHORIZATION, std::string(data.data).c_str());
		}
		else {
			Logger::error("Verification error. The token has expired.");
			token = "";
			websocket->close();
			websocket = NULL;
		}
	});
}

uint8_t onScriptLoad() {
	Logger::info("Reading configuration from server.cfg...");

	Configuration config;

	config.read("server.cfg");
	base_url = config.dictionary["vcmp_url"];
	vcmp_id = config.dictionary["vcmp_id"];
	vcmp_secret = config.dictionary["vcmp_secret"];
	if (config.dictionary.find("vcmp_debug") != config.dictionary.end()) {
		std::string d = config.dictionary["vcmp_debug"];
		Logger::DEBUG = d == "true" || d == "1" || d == "y";
	}

	if (base_url.empty() || vcmp_id.empty() || vcmp_secret.empty()) {
		Logger::error("The configuration cannot be read. Are the following contents missing: vcmp_url, vcmp_id, vcmp_secret");
		return 1;
	}

	Logger::success("Configuration file read successfully.");

	INT rc;
	WSADATA wsaData;

	rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (rc) {
		Logger::error("WSAStartup Failed.");
		return 1;
	}

	StartWebSocket();
	Logger::info("Connected.");

	Logger::info("Initializing service.");
}
void onScriptProcess(float et) {
	if (tokenExpires - 300000 <= GetFixedTickCount() && !token.empty()) {
		getToken(); // fetch token
	}
	if (websocket && websocket->getReadyState() != easywsclient::WebSocket::CLOSED) {
		easywsclient::WebSocket::pointer wsp = &*websocket; // <-- because a unique_ptr cannot be copied into a lambda
		if (ws_keepalive + 10000 <= GetFixedTickCount()) {
			websocket->sendPing();
			ws_keepalive = GetFixedTickCount();
		}
		websocket->poll();
		websocket->dispatch([wsp](const std::string& message) {
			try {
				WebSocketData content = parseWebSocketData(message);
				if (callbacks.find(content.echo) != callbacks.end())
				{
					callbacks[content.echo](content);
				}
				else {
					Server->SendPluginCommand(VCMPAPISocket_RECVALL, message.c_str());
				} // callback global
			}
			catch (const json::parse_error& e) {
				Logger::debug(message);
			}
		});
	}
	else if (reconnect_state == 0){
		reconnect_timestamp = GetFixedTickCount() + 5000;
		reconnect_state = 1;
		Logger::info("Disconnected.");
		Server->SendPluginCommand(VCMPAPISocket_DISCONNECT, "");
	}
	else if (reconnect_state == 1 && reconnect_timestamp <= GetFixedTickCount()) {
		Logger::info("Reconnecting...");
		Server->SendPluginCommand(VCMPAPISocket_RECONNECT, "");
		token = "";
		StartWebSocket();
		reconnect_state = 0;
	}
}

SQInteger fn_getWebSocketResultLength(HSQUIRRELVM v)
{
	const SQChar* key;
	sq->getstring(v, 2, &key);
	std::string k = std::string((char*)key);
	if (key && results.find(k) != results.end()) {
		WebSocketResult result = results[k];
		sq->pushinteger(v, result.length);
		return 1;
	}
	sq->pushnull(v);
	return 1;
}

SQInteger fn_getWebSocketResultEcho(HSQUIRRELVM v)
{
	const SQChar* key;
	sq->getstring(v, 2, &key);
	std::string k = std::string((char*)key);
	if (key && results.find(k) != results.end()) {
		WebSocketResult result = results[k];
		sq->pushstring(v, ((SQChar*)result.echo.c_str()), -1);
		return 1;
	}
	sq->pushnull(v);
	return 1;
}

SQInteger fn_freeWebSocketResult(HSQUIRRELVM v)
{
	const SQChar* key;
	sq->getstring(v, 2, &key);
	std::string k = std::string((char*)key);
	if (key && results.find(k) != results.end()) {
		delete &results[k];
		sq->pushbool(v, SQTrue);
		return 1;
	}
	sq->pushbool(v, SQFalse);
	return 1;
}

SQInteger fn_SendMessage(HSQUIRRELVM v)
{
	if (websocket && websocket->getReadyState() == easywsclient::WebSocket::OPEN)
	{
		const SQChar* type;
		const SQChar* message;
		sq->getstring(v, 2, &type);
		sq->getstring(v, 3, &message);
		WebSocketResult result = send(std::string((char*)type), std::string((char*)message));
		sq->pushstring(v, (SQChar*)result.echo.c_str(), -1);
		return 1;
	} 
	sq->pushinteger(v, -1);
	return 1;
}
std::string wrapperFunctionName(const char* funcname) {
	std::string func = std::string(funcname);
	return std::string("onVCMPAPISocket") + (char)std::toupper(func.at(0)) + func.substr(1);
}
void callSquirrelFunction(const char* functional_name, std::string data) {
	if (!sq || !v) return;
	sq->pushroottable(v);
	sq->pushstring(v, ((SQChar*)wrapperFunctionName(functional_name).c_str()), -1);
	if (SQ_SUCCEEDED(sq->get(v, -2)))
	{
		sq->pushroottable(v);
		sq->pushstring(v, (SQChar*)data.c_str(), -1);
		sq->call(v, 2, 0, 1);
	}
	sq->pop(v, 1);
}
void callSquirrelFunction(const char* functional_name) {
	if (!sq || !v) return;
	sq->pushroottable(v);
	sq->pushstring(v, ((SQChar*)wrapperFunctionName(functional_name).c_str()), -1);
	if (SQ_SUCCEEDED(sq->get(v, -2)))
	{
		sq->pushroottable(v);
		sq->call(v, 1, 0, 1);
	}
	sq->pop(v, 1);
}

void onPluginCommand0(uint32_t cid, std::string data) {
	if (cid == 0x7D6E22D8)
	{
		int32_t id = Server->FindPlugin("SQHost2");
		if (id == -1) return;
		size_t size;
		const void** exports = Server->GetPluginExports(id, &size);
		if (Server->GetLastError() != vcmpErrorNone || exports == NULL || size <= 0) return;
		SquirrelImports** s = (SquirrelImports**)exports;
		SquirrelImports* f = (SquirrelImports*)(*s);
		if (!f) return;
		sq = *(f->GetSquirrelAPI());
		v = *(f->GetSquirrelVM());
		if (!sq || !v) return;
		sq->pushroottable(v);
		sq->pushstring(v, ((SQChar*)"VCMPAPISocket_sendMessage"), -1);
		sq->newclosure(v, fn_SendMessage, 0);
		sq->setparamscheck(v, 3, ((SQChar*)"tss"));
		sq->newslot(v, -3, SQFalse);
		sq->pop(v, 1);

		sq->pushroottable(v);
		sq->pushstring(v, ((SQChar*)"VCMPAPISocket_getResultEcho"), -1);
		sq->newclosure(v, fn_getWebSocketResultEcho, 0);
		sq->setparamscheck(v, 2, ((SQChar*)"ts"));
		sq->newslot(v, -3, SQFalse);
		sq->pop(v, 1);

		sq->pushroottable(v);
		sq->pushstring(v, ((SQChar*)"VCMPAPISocket_getResultLength"), -1);
		sq->newclosure(v, fn_getWebSocketResultLength, 0);
		sq->setparamscheck(v, 2, ((SQChar*)"ts"));
		sq->newslot(v, -3, SQFalse);
		sq->pop(v, 1);

		sq->pushroottable(v);
		sq->pushstring(v, ((SQChar*)"VCMPAPISocket_freeResult"), -1);
		sq->newclosure(v, fn_freeWebSocketResult, 0);
		sq->setparamscheck(v, 2, ((SQChar*)"ts"));
		sq->newslot(v, -3, SQFalse);
		sq->pop(v, 1);
	}
	else {
		switch (cid) {
		case VCMPAPISocket_CONNECT:
			callSquirrelFunction("connect");
			break;
		case VCMPAPISocket_AUTHORIZATION:
			callSquirrelFunction("authorization", data);
			break;
		case VCMPAPISocket_DISCONNECT:
			callSquirrelFunction("disconnect");
			break;
		case VCMPAPISocket_RECONNECT:
			callSquirrelFunction("reconnect");
			break;
		case VCMPAPISocket_RECVALL:
			if (!sq || !v) return;
			sq->pushroottable(v);
			sq->pushstring(v, ((SQChar*)"onVCMPAPISocketMessage"), -1);
			if (SQ_SUCCEEDED(sq->get(v, -2)))
			{
				WebSocketData wsd = parseWebSocketData(data);
				sq->pushroottable(v);
				sq->pushbool(v, wsd.status);
				sq->pushstring(v, (SQChar*)wsd.type.c_str(), -1);
				sq->pushstring(v, (SQChar*)wsd.data.dump(0).c_str(), -1);
				sq->pushstring(v, (SQChar*)wsd.msg.c_str(), -1);
				sq->pushstring(v, (SQChar*)wsd.echo.c_str(), -1);
				sq->call(v, 6, 0, 1);
			}
			sq->pop(v, 1);
			break;
		case VCMPAPISocket_ECHO_CLIENT:
			// todo
			break;
		}
	}
}

uint8_t onPluginCommand(uint32_t cid, const char* message) {
	std::string data = std::string(message);
	onPluginCommand0(cid, data);
}

#ifdef WIN32
extern "C" __declspec(dllexport) unsigned int VcmpPluginInit(PluginFuncs * pluginFuncs, PluginCallbacks * pluginCallbacks, PluginInfo * pluginInfo) {
#else
extern "C"  unsigned int VcmpPluginInit(PluginFuncs * pluginFuncs, PluginCallbacks * pluginCallbacks, PluginInfo * pluginInfo) {
#endif
#ifdef WIN32 
	DWORD dwMode = 0;
	//Enable Virtual Terminal Sequences
	bool bAlternateBuffer = false;
	GetConsoleMode(
		GetStdHandle(STD_OUTPUT_HANDLE),
		&dwMode);
	SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), dwMode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
	Server = pluginFuncs;
	Callbacks = pluginCallbacks;
	// Plugin information
	pluginInfo->pluginVersion = 0x1;
	pluginInfo->apiMajorVersion = PLUGIN_API_MAJOR;
	pluginInfo->apiMinorVersion = PLUGIN_API_MINOR;
	memcpy(pluginInfo->name, PLUGIN_NAME, strlen(PLUGIN_NAME) + 1);

	pluginCallbacks->OnServerInitialise = onScriptLoad;
	pluginCallbacks->OnServerFrame = onScriptProcess;
	pluginCallbacks->OnPluginCommand = onPluginCommand;

#ifdef WIN32
	//Get console handl
	Logger::hstdout = GetStdHandle(STD_OUTPUT_HANDLE);
#endif
	return 1;
}