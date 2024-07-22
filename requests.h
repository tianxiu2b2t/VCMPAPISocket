#ifndef ASYNCHTTPCONNECTION_HPP
#define ASYNCHTTPCONNECTION_HPP

#include <iostream>
#include <curl/curl.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>
#include <sstream>
#include <functional>
#include <map>

class AsyncHttpConnection {
public:
    std::map<std::string, std::string> defaultHeaders = std::map<std::string, std::string>{
        {"Connection", "keep-alive"},
        {"Content-Type", "application/json"}
    };
    AsyncHttpConnection();
    ~AsyncHttpConnection();

    // 异步 GET 请求，并带有回调函数和等待
    void asyncGet(const std::string& url, std::function<void(std::string)> callback);
    std::string asyncGet(const std::string& url);

    // 等待异步 GET 请求完成并获取响应
    std::string waitForGetResponse();

    // 异步 POST 请求，并带有回调函数和等待
    void asyncPost(const std::string& url, const std::string& data, std::function<void(std::string)> callback);
    std::string asyncPost(const std::string& url, const std::string& data);

    // 等待异步 POST 请求完成并获取响应
    std::string waitForPostResponse();

private:
    CURL* curl_;

    std::string getResponse_;
    bool isGetResponseReady_;
    std::mutex getMutex_;
    std::condition_variable getCv_;

    std::string postResponse_;
    bool isPostResponseReady_;
    std::mutex postMutex_;
    std::condition_variable postCv_;

    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
};

#endif 