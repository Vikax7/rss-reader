#pragma once

#include "core/feed.hpp"
#include "core/feed_parser_factory.hpp"
#include "app/http_client.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <unordered_set>

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
            std::lock_guard<std::mutex> lock(mutex_);
            feeds_.push_back(std::make_unique<core::Feed>(std::move(feed)));
        }

        //Загрузка или обновление ленты по реальной сети с кэшированием
        void fetch_and_add_feed(const std::string& url) {
            app::HttpClient client; 

            //Проверяем, нет ли уже такой ленты, чтобы подтянуть старый ETag/Last-Modified
            std::string etag;
            std::string last_modified;

            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (const auto& existing_feed : feeds_) {
                    if (existing_feed->get_url() == url) {
                        etag = existing_feed->get_etag();
                        last_modified = existing_feed->get_last_modified();
                        break;
                    }
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
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& existing_feed : feeds_) {
                if (existing_feed->get_url() == url) {
                    existing_feed = std::make_unique<core::Feed>(std::move(feed));
                    return;
                }
            }

            feeds_.push_back(std::make_unique<core::Feed>(std::move(feed)));
        }

        //Фоновое асинхронное обновление всех лент
        void refresh_all_async() {
            // std::jthread автоматически завершится и сделает join при уничтожении (RAII)
            background_thread_ = std::jthread([this](std::stop_token stoken) {
                // Собираем копии URL подписок под мьютексом, чтобы не держать его во время сетевых запросов
                std::vector<std::string> urls;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    for (const auto& feed : feeds_) {
                        urls.push_back(feed->get_url());
                    }
                }

                // Последовательно обновляем каждую ленту в фоне
                for (const auto& url : urls) {
                    if (stoken.stop_requested()) break; // Поддержка мягкой отмены потока

                    try {
                        fetch_and_add_feed(url);
                    } catch (const std::exception& e) {
                        // Здесь можно сохранить ошибку в лог или статус ленты, 
                        // чтобы интерфейс мог показать её пользователю
                    }
                }
            });
        }

        //отметка конкретной статьи как прочитанной
        void mark_article_read(const std::string& feed_url, const std::string& article_link) {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& feed : feeds_) {
                if (feed->get_url() == feed_url) {
                    for (auto& art : feed->get_articles()) {
                        if (art.link == article_link) {
                            art.is_read = true;
                            return;
                        }
                    }
                }
            }
        }

        // Массовая отметка статей как прочитанных по списку ссылок
        void mark_articles_read_batch(const std::unordered_set<std::string>& read_links) {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& feed : feeds_) {
                for (auto& art : feed->get_articles()) {
                    if (read_links.count(art.link)) {
                        art.is_read = true;
                    }
                }
            }
        }

        // Потокобезопасное получение списка лент 
        [[nodiscard]] std::vector<core::Feed> get_feeds_copy() const {
            std::lock_guard<std::mutex> lock(mutex_);
            std::vector<core::Feed> copies;
            copies.reserve(feeds_.size());
            for (const auto& feed : feeds_) {
                if (feed) {
                    copies.push_back(*feed);
                }
            }
            return copies;
        }

        void remove_feed(size_t index) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (index >= feeds_.size()) throw std::out_of_range("Feed index uot of range");
            feeds_.erase(feeds_.begin() + index);
        }

        void clear() noexcept {
            std::lock_guard<std::mutex> lock(mutex_);
            feeds_.clear();
        }

        private:
        mutable std::mutex mutex_;
        std::vector<std::unique_ptr<core::Feed>> feeds_;
        std::jthread background_thread_; // Поток для фонового обновления
    };
}