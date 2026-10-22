#pragma once
#include "feed_parser.hpp"
#include <pugixml.hpp>
#include <stdexcept>
#include <string>
#include <sstream>

namespace rss_reader::core {

    class AtomParser : public FeedParser {
        public:
        Feed parse(const std::string& raw_data) override {
            
            pugi::xml_document doc;
            pugi::xml_parse_result result = doc.load_string(raw_data.c_str());

            if (!result) {
            throw std::runtime_error("Failed to parse Atom XML: " + std::string(result.description()));
            }

            //в Atom <feed> - корневой элемент
            pugi::xml_node feed_node = doc.child("feed");
            if (!feed_node) {
            throw std::runtime_error("Invalid Atom format: missing 'feed' element");
            }

            std::string title = feed_node.child_value("title");

            std::string link;
            pugi::xml_node link_node = feed_node.child("link");
            if (link_node) {
                link = link_node.attribute("href").value();
            }

            Feed feed(link, title);

            for (pugi::xml_node entry_node : feed_node.children("entry")) {
                Article article;
                article.title = entry_node.child_value("title");

                pugi::xml_node entry_link = entry_node.child("link");
                if (entry_link) {
                    article.link = entry_link.attribute("href").value();
                }

                //Содержание может быть в <content> или <summary>
                for (const char* name : {"content", "summary"}) {
                    auto content = entry_node.child(name);
                    if (std::string(content.attribute("type").value()) == "xhtml") {
                        // Встроенную XHTML-разметку передаём рендереру целиком.
                        std::ostringstream html;
                        for (auto child : content.children()) child.print(html, "", pugi::format_raw);
                        article.content = html.str();
                    } else {
                        article.content = content.child_value();
                    }
                    if (!article.content.empty()) break;
                }

                feed.add_article(article);
            }

            return feed;
        }
    };

}
