#pragma once

#include <string>
#include <vector>
#include "article.hpp"

namespace rss_reader::core {
    
    class Feed {
        public:
        Feed() = default;
        Feed(std::string url, std::string title);

        void add_article(const Article& article);

        [[nodiscard]] const std::string& get_url() const noexcept;
        [[nodiscard]] const std::string& get_title() const noexcept;

        //Константная версия — для обычного чтения статей (используется в тестах и при отрисовке)
        [[nodiscard]] const std::vector<Article>& get_articles() const noexcept;

        //Неконстантная версия — для изменения статуса статей (is_read = true)
        [[nodiscard]] std::vector<Article>& get_articles() noexcept;

        void set_url(const std::string& url);

        void set_etag(const std::string& etag);
        void set_last_modified(const std::string& last_modified);

        [[nodiscard]] const std::string& get_etag() const noexcept;
        [[nodiscard]] const std::string& get_last_modified() const noexcept;

        private:
        std::string url_;
        std::string title_;
        std::vector<Article> articles_;
        std::string etag_;
        std::string last_modified_;
    };
}