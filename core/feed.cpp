#include "feed.hpp"

namespace rss_reader::core {

    Feed::Feed(std::string url, std::string title)
        : url_(std::move(url)), title_(std::move(title)) {}

    void Feed::add_article(const Article& article) {
        articles_.push_back(article);
    }

    const std::string& Feed::get_url() const noexcept {
        return url_;
    }

    const std::string& Feed::get_title() const noexcept {
        return title_;
    }

    // константная версия
    const std::vector<Article>& Feed::get_articles() const noexcept {
        return articles_;
    }

    // неконстантная версия
    std::vector<Article>& Feed::get_articles() noexcept {
        return articles_;
    }

    void Feed::set_url(const std::string& url) {
        url_ = url;
    }

    void Feed::set_etag(const std::string& etag) {
        etag_ = etag;
    }

    void Feed::set_last_modified(const std::string& last_modified) {
        last_modified_ = last_modified;
    }

    const std::string& Feed::get_etag() const noexcept {
        return etag_;
    }

    const std::string& Feed::get_last_modified() const noexcept {
        return last_modified_;
    }
}