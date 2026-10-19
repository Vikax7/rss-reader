#pragma once
#include <string>

namespace rss_reader::app {

    struct HttpResponse {
        long status_code{0}; //HTTP-статус ответа сервера (числовой код)
        std::string body; //Тело ответа от сервера (сырой текст XML-разметки RSS или Atom ленты)
        std::string etag;
        std::string last_modified;
    };

}