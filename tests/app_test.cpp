#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "app/feed_manager.hpp"


const std::string valid_rss = R"(<?xml version="1.0" encoding="UTF-8" ?>
<rss version="2.0">
    <channel>
        <title>Test RSS Feed</title>
        <link>https://example.com</link>
        <description>A test feed for FeedManager</description>
        <item>
            <title>First Test Article</title>
            <link>https://example.com/article1</link>
            <description>Content of the first article</description>
        </item>
    </channel>
</rss>)";

TEST_CASE("FeedManager - Subscription Management") {
    rss_reader::app::FeedManager manager;

    SUBCASE("Initially feed list is empty") {
        CHECK(manager.get_feeds_copy().empty());
    }

    SUBCASE("Add feed from raw data successfully") {
        REQUIRE_NOTHROW(manager.add_feed_from_raw("https://example.com/rss", valid_rss));
        
        const auto& feeds = manager.get_feeds_copy();
        REQUIRE(feeds.size() == 1);
        CHECK(feeds[0].get_title() == "Test RSS Feed");
        CHECK(feeds[0].get_articles().size() == 1);
        CHECK(feeds[0].get_articles()[0].title == "First Test Article");
    }

    SUBCASE("Remove feed by index") {
        manager.add_feed_from_raw("https://example.com/rss", valid_rss);
        REQUIRE(manager.get_feeds_copy().size() == 1);

        // Удаляем существующий индекс
        REQUIRE_NOTHROW(manager.remove_feed(0));
        CHECK(manager.get_feeds_copy().empty());
    }

    SUBCASE("Throw exception on out of range removal") {
        // Попытка удалить из пустого менеджера должна вызвать исключение
        CHECK_THROWS_AS(manager.remove_feed(0), std::out_of_range);

        manager.add_feed_from_raw("https://example.com/rss", valid_rss);
        // Попытка удалить несуществующий индекс
        CHECK_THROWS_AS(manager.remove_feed(5), std::out_of_range);
    }

    SUBCASE("Clear all feeds") {
        manager.add_feed_from_raw("https://example.com/rss", valid_rss);
        manager.add_feed_from_raw("https://example.com/rss2", valid_rss);
        CHECK(manager.get_feeds_copy().size() == 2);

        manager.clear();
        CHECK(manager.get_feeds_copy().empty());
    }
}