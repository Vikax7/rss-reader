#pragma once
#include <string>

namespace rss_reader::core {


struct Article {
    std::string title;
    std::string link;
    std::string content;
    bool is_read{false};
};

}