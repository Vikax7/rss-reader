#pragma once
#include "feed_parser.hpp"
#include <pugixml.hpp>
#include <stdexcept>
#include <string>

namespace rss_reader::core {

    class RssParser : public FeedParser { //наследование
        public:
        Feed parse(const std::string& raw_data) override {
            pugi::xml_document doc;
            //документ пытается распарсить переданную строку
            pugi::xml_parse_result result = doc.load_string(raw_data.c_str());

            if (!result) throw std::runtime_error("Failed to parse RSS XML: " + std::string(result.description()));

            pugi::xml_node channel = doc.child("rss").child("channel");
            if (!channel) throw std::runtime_error("Invalid RSS format: missing 'channel' element");
            
            std::string title = channel.child_value("title");
            std::string link = channel.child_value("link");

            Feed feed(link, title);

            //Парсим статьи
            for (pugi::xml_node item_node : channel.children("item")) {
                Article article;
                article.title = item_node.child_value("title");
                article.link = item_node.child_value("link");
                // Полный текст статьи имеет приоритет над кратким описанием.
                article.content = item_node.child_value("content:encoded");
                if (article.content.empty()) article.content = item_node.child_value("description");

                feed.add_article(article);
            }

            return feed;
        }
        
    };

}
