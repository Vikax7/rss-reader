#pragma once

#include "core/feed.hpp"
#include "core/feed_parser_factory.hpp"
#include <string>
#include <vector>

namespace rss_reader::app {

    class FeedManager {
        public:

        FeedManager() = default;

        void add_feed_from_raw(const std::string& url, const std::string& raw_data) {
            auto parser = core::FeedParserFactory::create_parser(raw_data);
            core::Feed feed = parser->parse(raw_data);
            feed.set_url(url);

            //сохр ленту
            feeds_.push_back(std::make_unique<core::Feed>(std::move(feed)));
        }

        [[nodiscard]] const std::vector<std::unique_ptr<core::Feed>>& get_feeds() const noexcept {
            return feeds_;
        }

        void remove_feed(size_t index) {
            if (index >= feeds_.size()) throw std::out_of_range("Feed index uot of range");
            feeds_.erase(feeds_.begin() + index);
        }

        void clear() noexcept {
            feeds_.clear();
        }

        private:
        std::vector<std::unique_ptr<core::Feed>> feeds_;
    };
}