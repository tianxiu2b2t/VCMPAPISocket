#include "requests.h"

#include <iostream>
#include <curl/curl.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>
#include <sstream>
#include <string.h>
#include <stdio.h>
#include <functional>

AsyncHttpConnection::AsyncHttpConnection() {
    curl_global_init(CURL_GLOBAL_ALL);
    curl_ = curl_easy_init();
    isGetResponseReady_ = false;
    isPostResponseReady_ = false;
    struct curl_slist* header_list = NULL;
    if (!defaultHeaders.empty()) {
        for (const auto& key : defaultHeaders) {
            header_list = curl_slist_append(header_list, (std::string(key.first) + ": " + std::string(key.second)).c_str());
        }
        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, header_list);
    }
}

AsyncHttpConnection::~AsyncHttpConnection() {
    curl_easy_cleanup(curl_);
}

// 异步 GET 请求，并带有回调函数和等待
void AsyncHttpConnection::asyncGet(const std::string& url, std::function<void(std::string)> callback) {
    std::thread t([this, url, callback]() {
        std::string response;
        curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, &writeCallback);
        curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response);

        CURLcode res = curl_easy_perform(curl_);

        {
            std::lock_guard<std::mutex> lock(getMutex_);
            getResponse_ = response;
            isGetResponseReady_ = true;
            getCv_.notify_one();
        }

        callback(response);
        });
    t.detach();
}

std::string AsyncHttpConnection::asyncGet(const std::string& url) {
    this->asyncGet(url, [](std::string response) {});
    return this->waitForGetResponse();
}

// 等待异步 GET 请求完成并获取响应
std::string AsyncHttpConnection::waitForGetResponse() {
    std::unique_lock<std::mutex> lock(getMutex_);
    getCv_.wait(lock, [this]() { return isGetResponseReady_; });
    return getResponse_;
}

// 异步 POST 请求，并带有回调函数和等待
void AsyncHttpConnection::asyncPost(const std::string& url, const std::string& data, std::function<void(std::string)> callback) {
    std::thread t([this, url, data, callback]() {
        std::string response;
        curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl_, CURLOPT_POST, 1L);
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, data.c_str());
        curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, &writeCallback);
        curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response);

        CURLcode res = curl_easy_perform(curl_);

        {
            std::lock_guard<std::mutex> lock(postMutex_);
            postResponse_ = response;
            isPostResponseReady_ = true;
            postCv_.notify_one();
        }

        callback(response);
        });
    t.detach();
}

std::string AsyncHttpConnection::asyncPost(const std::string& url, const std::string& data) {
    this->asyncPost(url, data, [](std::string response) {});
    return this->waitForPostResponse();
}

// 等待异步 POST 请求完成并获取响应
std::string AsyncHttpConnection::waitForPostResponse() {
    std::unique_lock<std::mutex> lock(postMutex_);
    postCv_.wait(lock, [this]() { return isPostResponseReady_; });
    return postResponse_;
}

// 写入回调函数
size_t AsyncHttpConnection::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realSize = size * nmemb;
    std::string* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), realSize);
    return realSize;
}