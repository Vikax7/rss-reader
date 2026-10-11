#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "core/article.hpp"
#include "core/feed.hpp"

TEST_CASE("Article structure cat be initialized") {
    rss_reader::core::Article article{
        "Test title",
        "https://exemple.com/article1",
        "Test content.",
        false
    };

    CHECK(article.title == "Test title");
    CHECK(article.link == "https://exemple.com/article1");
    CHECK(article.content == "Test content.");
    CHECK_FALSE(article.is_read);
}

TEST_CASE("Feed manages articles and cache metadata correctly") {
    rss_reader::core::Feed feed("https://example.com/rss.xml", "Example News");
   
    CHECK(feed.get_url() == "https://example.com/rss.xml");
    CHECK(feed.get_title() == "Example News");
    CHECK(feed.get_articles().empty());

    //Проверяем добавление статьи
    rss_reader::core::Article article{
        "Article 1",
        "https://example.com/1",
        "Content 1",
        false
    };
    feed.add_article(article);

    CHECK(feed.get_articles().size() == 1);
    CHECK(feed.get_articles()[0].title == "Article 1");
    CHECK(feed.get_articles()[0].link == "https://example.com/1");
    CHECK(feed.get_articles()[0].content == "Content 1");

    //Проверяем работу ETag и Last-Mofied
    feed.set_etag("\"12345-abc\"");
    feed.set_last_modified("Sun, 11 Oct 2026 14:07:00 GMT");

    CHECK(feed.get_etag() == "\"12345-abc\"");
    CHECK(feed.get_last_modified() == "Sun, 11 Oct 2026 14:07:00 GMT");

}
