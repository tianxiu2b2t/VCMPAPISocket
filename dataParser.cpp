#include "dataParser.h"

using namespace std;

DataContent parseResponseData(std::string response) {
    json jsonData = json::parse(response);
    DataContent content;
    content.status = jsonData.at("status");
    content.data = jsonData.at("data");
    if (jsonData.find("msg") != jsonData.end()) {
        content.msg = jsonData.at("msg");
    }
    return content;
}

WebSocketData parseWebSocketData(std::string response) {
    json jsonData = json::parse(response);
    WebSocketData content;
    content.status = jsonData.at("status");
    if (jsonData.find("data") != jsonData.end()) {
        content.data = jsonData.at("data");
    }
    else {
        content.data = json::parse("{}");
    }
    content.type = jsonData.at("type");
    if (jsonData.find("echo") != jsonData.end()) {
        content.echo = jsonData.at("echo");
    }
    else content.echo = "";
    if (jsonData.find("msg") != jsonData.end()) {
        content.msg = jsonData.at("msg");
    }
    else {
        content.msg = "";
    }
    return content;
}