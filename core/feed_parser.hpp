#pragma once
#include "core/feed.hpp"
#include <string>
#include <memory>

namespace rss_reader::core {

    class FeedParser {
        public:
        virtual ~FeedParser() = default;
        virtual Feed parse(const std::string& raw_data) = 0;
    };

}