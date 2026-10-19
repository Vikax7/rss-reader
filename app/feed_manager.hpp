#pragma once

#include "core/feed.hpp"
#include "core/feed_parser_factory.hpp"
#include "app/http_client.hpp"
#include <string>
#include <vector>

namespace rss_reader::app {

    class FeedManager {
        public:

        FeedManager() = default;

        //Добавление ленты из сырых данных (для тестов)
        void add_feed_from_raw(const std::string& url, const std::string& raw_data) {
            auto parser = core::FeedParserFactory::create_parser(raw_data);
            core::Feed feed = parser->parse(raw_data);
            feed.set_url(url);

            //сохр ленту
            feeds_.push_back(std::make_unique<core::Feed>(std::move(feed)));
        }

        //Загрузка или обновление ленты по реальной сети с кэшированием
        void fetch_and_add_feed(const std::string& url) {
            app::HttpClient client; 

            //Проверяем, нет ли уже такой ленты, чтобы подтянуть старый ETag/Last-Modified
            std::string etag;
            std::string last_modified;
            for (const auto& existing_feed : feeds_) {
                if (existing_feed->get_url() == url) {
                    etag = existing_feed->get_etag();
                    last_modified = existing_feed->get_last_modified();
                    break;
                }
            }

            // Делаем запрос с условным кэшированием
            HttpResponse response = client.get(url, etag, last_modified);

            // Если сервер ответил 304 Not Modified, лента не изменилась — ничего перепаковывать не нужно
            if (response.status_code == 304) {
                return; 
            }

            if (response.status_code != 200) {
                throw std::runtime_error("Failed to fetch feed, HTTP status code: " + std::to_string(response.status_code));
            }

            // Парсим скачанный XML
            auto parser = core::FeedParserFactory::create_parser(response.body);
            core::Feed feed = parser->parse(response.body);
            
            feed.set_url(url);
            feed.set_etag(response.etag);
            feed.set_last_modified(response.last_modified);

            // Если лента уже была в списке — обновляем её, иначе добавляем новую
            for (auto& existing_feed : feeds_) {
                if (existing_feed->get_url() == url) {
                    existing_feed = std::make_unique<core::Feed>(std::move(feed));
                    return;
                }
            }

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