#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "core/article.hpp"
#include "core/feed.hpp"
#include "core/feed_parser_factory.hpp"
#include "core/rss_parser.hpp"
#include "core/atom_parser.hpp"
#include "core/html_renderer.hpp"

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

TEST_CASE("FeedParserFactory creates correct parser based on feed content") {
    SUBCASE("Should create RssParser for RSS 2.0 feed") {
        std::string rss_data = "<?xml version=\"1.0\"?><rss version=\"2.0\"><channel><title>Test RSS</title></channel></rss>";

        // Проверяем, что фабрика отрабатывает без исключений
        REQUIRE_NOTHROW(rss_reader::core::FeedParserFactory::create_parser(rss_data));
        
        auto parser = rss_reader::core::FeedParserFactory::create_parser(rss_data);
        // Убеждаемся, что указатель не нулевой
        CHECK(parser != nullptr);
    }
 
}

TEST_CASE("RssParser successfully parses valid RSS 2.0 feed") {
    std::string valid_rss = 
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<rss version=\"2.0\">"
        "  <channel>"
        "    <title>C++ News</title>"
        "    <link>https://example.com/rss</link>"
        "    <description>Latest news about C++</description>"
        "    <item>"
        "      <title>C++20 Modules</title>"
        "      <link>https://example.com/modules</link>"
        "      <description>All about modules in C++20.</description>"
        "     </item>"
        "  </channel>"
        "</rss>";

    auto parser = rss_reader::core::FeedParserFactory::create_parser(valid_rss);
    REQUIRE(parser != nullptr);

    rss_reader::core::Feed feed = parser->parse(valid_rss);

    CHECK(feed.get_title() == "C++ News");
    CHECK(feed.get_url() == "https://example.com/rss");
    REQUIRE(feed.get_articles().size() == 1);
    CHECK(feed.get_articles()[0].title == "C++20 Modules");
    CHECK(feed.get_articles()[0].link == "https://example.com/modules");
    CHECK(feed.get_articles()[0].content == "All about modules in C++20.");
}

TEST_CASE("AtomParser successfully parses valid Atom 1.0 feed") {
    std::string valid_atom = 
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<feed xmlns=\"http://www.w3.org/2005/Atom\">"
        "  <title>My Atom Blog</title>"
        "  <link href=\"https://example.com/atom.xml\" rel=\"self\"/>"
        "  <entry>"
        "    <title>First Atom Post</title>"
        "    <link href=\"https://example.com/posts/first-post\"/>"
        "    <content>This is the content of the first Atom post.</content>"
        "  </entry>"
        "</feed>";

    auto parser = rss_reader::core::FeedParserFactory::create_parser(valid_atom);
    REQUIRE(parser != nullptr);

    rss_reader::core::Feed feed = parser->parse(valid_atom);

    CHECK(feed.get_title() == "My Atom Blog");
    CHECK(feed.get_url() == "https://example.com/atom.xml");
    REQUIRE(feed.get_articles().size() == 1);
    CHECK(feed.get_articles()[0].title == "First Atom Post");
    CHECK(feed.get_articles()[0].link == "https://example.com/posts/first-post");
    CHECK(feed.get_articles()[0].content == "This is the content of the first Atom post.");
}

TEST_CASE("HtmlRenderer basic text and paragraphs") {
    std::string html = "<p>Hello, World!</p><div>Second block</div><br>New line test";
    auto result = rss_reader::core::HtmlRenderer::render(html);

    // Ожидаем, что теги заменятся на переносы строк, а текст сохранится
    CHECK(result.text.find("Hello, World!") != std::string::npos);
    CHECK(result.text.find("Second block") != std::string::npos);
    CHECK(result.text.find("New line test") != std::string::npos);
}

TEST_CASE("HtmlRenderer links and footnotes generation") {
    std::string html = "<p>Check <a href=\"https://example.com\">this link</a> out.</p>";
    auto result = rss_reader::core::HtmlRenderer::render(html);

    // Проверяем, что ссылка превратилась в текст со сноской [1]
    CHECK(result.text.find("this link [1]") != std::string::npos);
    
    // Проверяем, что в структуре сохранился список сносок с правильным URL
    REQUIRE(result.footnotes.size() == 1);
    CHECK(result.footnotes[0] == "https://example.com");

    // Проверяем, что в самом конце отрендеренного текста есть секция со списком ссылок
    CHECK(result.text.find("[1] https://example.com") != std::string::npos);
}

TEST_CASE("HtmlRenderer list items and headers") {
    std::string html = "<h1>Title</h1><ul><li>Item 1</li><li>Item 2</li></ul>";
    auto result = rss_reader::core::HtmlRenderer::render(html);

    CHECK(result.text.find("Title") != std::string::npos);
    CHECK(result.text.find("  - Item 1") != std::string::npos);
    CHECK(result.text.find("  - Item 2") != std::string::npos);
}

TEST_CASE("HtmlRenderer empty and invalid html resilience") {
    // Пустой контент
    auto res_empty = rss_reader::core::HtmlRenderer::render("");
    CHECK(res_empty.text.empty());
    CHECK(res_empty.footnotes.empty());

    // Битый или неструктурированный текст
    std::string raw = "Just plain text without tags";
    auto res_plain = rss_reader::core::HtmlRenderer::render(raw);
    CHECK(res_plain.text.find("Just plain text without tags") != std::string::npos);
}
