#pragma once
#include "feed_parser.hpp"
#include "atom_parser.hpp"
#include "rss_parser.hpp"
#include <memory>
#include <string>
#include <stdexcept>

namespace rss_reader::core {

class FeedParserFactory {
public:
    static std::unique_ptr<FeedParser> create_parser(const std::string& raw_data) {
        // Простейшая эвристика по определению формата
        if (raw_data.find("<rss") != std::string::npos) {
            return std::make_unique<RssParser>();
        } else if (raw_data.find("<feed") != std::string::npos) {
            return std::make_unique<AtomParser>();
        }
        throw std::runtime_error("Unknown feed format");
    }
};

} 