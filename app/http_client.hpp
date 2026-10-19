#pragma once
#include "http_response.hpp"
#include <curl/curl.h>
#include <stdexcept>
#include <vector>

namespace rss_reader::app {

class HttpClient {
public:
    HttpClient() {
        // Инициализация libcurl
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }

    ~HttpClient() {
        curl_global_cleanup();
    }

    // Запрещаем копирование во избежание двойного освобождения ресурсов curl
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    //конструктор перемещения
    HttpClient(HttpClient&&) noexcept = default;
    HttpClient& operator=(HttpClient&&) noexcept = default;

    // Выполнение GET-запроса с поддержкой условного кэширования
    HttpResponse get(const std::string& url, const std::string& etag = "", const std::string& last_modified = "") {
        CURL* curl = curl_easy_init();
        if (!curl) {
            throw std::runtime_error("Failed to initialize cURL handle");
        }

        HttpResponse response;

        // Установка URL
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        // Настройка записи тела ответа
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);

        // Настройка чтения заголовков ответа (для извлечения ETag и Last-Modified)
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response);

        // Настройка условных заголовков для кеширования
        struct curl_slist* headers = nullptr;
        if (!etag.empty()) {
            std::string header = "If-None-Match: " + etag;
            headers = curl_slist_append(headers, header.c_str());
        }
        if (!last_modified.empty()) {
            std::string header = "If-Modified-Since: " + last_modified;
            headers = curl_slist_append(headers, header.c_str());
        }
        if (headers) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        }

        // Дополнительные параметры безопасности и удобства
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Следовать редиректам
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);       // Таймаут 15 секунд

        // Выполнение запроса
        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            if (headers) curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            throw std::runtime_error(std::string("cURL request failed: ") + curl_easy_strerror(res));
        }

        // Получаем HTTP статус-код (например, 200 OK, 304 Not Modified, 404 Not Found)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);

        // Очистка ресурсов запроса
        if (headers) curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        return response;
    }

private:
    // Callback для записи данных тела ответа
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        size_t total_size = size * nmemb;
        auto* str = static_cast<std::string*>(userp);
        str->append(static_cast<char*>(contents), total_size);
        return total_size;
    }

    // Callback для разбора заголовков ответа сервера
    static size_t HeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
        size_t total_size = size * nitems;
        std::string header_line(buffer, total_size);
        auto* response = static_cast<HttpResponse*>(userdata);

        // Поиск ETag
        if (header_line.rfind("ETag:", 0) == 0 || header_line.rfind("etag:", 0) == 0) {
            size_t start = header_line.find_first_not_of(" \t", 5);
            size_t end = header_line.find_last_not_of("\r\n");
            if (start != std::string::npos && end != std::string::npos) {
                response->etag = header_line.substr(start, end - start + 1);
            }
        }
        // Поиск Last-Modified
        else if (header_line.rfind("Last-Modified:", 0) == 0 || header_line.rfind("last-modified:", 0) == 0) {
            size_t start = header_line.find_first_not_of(" \t", 14);
            size_t end = header_line.find_last_not_of("\r\n");
            if (start != std::string::npos && end != std::string::npos) {
                response->last_modified = header_line.substr(start, end - start + 1);
            }
        }

        return total_size;
    }
};

} 