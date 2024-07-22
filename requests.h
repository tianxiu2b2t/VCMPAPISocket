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

    // �첽 GET ���󣬲����лص������͵ȴ�
    void asyncGet(const std::string& url, std::function<void(std::string)> callback);
    std::string asyncGet(const std::string& url);

    // �ȴ��첽 GET ������ɲ���ȡ��Ӧ
    std::string waitForGetResponse();

    // �첽 POST ���󣬲����лص������͵ȴ�
    void asyncPost(const std::string& url, const std::string& data, std::function<void(std::string)> callback);
    std::string asyncPost(const std::string& url, const std::string& data);

    // �ȴ��첽 POST ������ɲ���ȡ��Ӧ
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