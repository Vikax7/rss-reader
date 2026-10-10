#pragma once
#include <string>

namespace rss::core {


struct Article {
    std::string title;
    std::string link;
    std::string content;
    bool is_read{false};
};

}