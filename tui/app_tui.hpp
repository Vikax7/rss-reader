#pragma once

#include "app/feed_manager.hpp"
#include "core/html_renderer.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

namespace rss_reader::tui {

    enum class ScreenState {
        FeedsList, // состояние со списком лент
        ArticlesList // состояние со список внутри конкретной ленты
    };

    class RssReaderApp {
        public:
            RssReaderApp() : manager_() {
                // Добавим для теста дефолтную ленту, чтобы интерфейс сразу не был пустым
                try {
                    manager_.fetch_and_add_feed("https://news.ycombinator.com/rss");
                } catch (...) {
                    // Если сети нет, создадим локальную тестовую ленту
                    std::string sample_rss = 
                        "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"
                        "<rss version=\"2.0\">"
                            "<channel>"
                                "<title>Local C++ Test Feed</title>"
                                "<link>https://example.com</link>"
                                "<description>Offline test feed</description>"
                                "<item>"
                                    "<title>Hello C++20 TUI Article</title>"
                                    "<link>https://example.com/test</link>"
                                    "<description>Testing the FTXUI article navigation</description>"
                                "</item>"
                            "</channel>"
                        "</rss>";
                    manager_.add_feed_from_raw("https://example.com/rss", sample_rss);
                }
            }
            

            void run() {
                using namespace ftxui;
                auto screen = ScreenInteractive::Fullscreen();

                ScreenState current_screen = ScreenState::FeedsList;

                // индекс выбранной пользователем ленты в списке
                int selected_feed = 0;
                // индекс выбранной пользователем статьи внутри конкретной ленты
                int selected_article = 0;

                //локальный кэш объекта Feed который пользователь выбрал на первом экране
                core::Feed current_feed_cache;

                //отрисовываем список лент
                auto feeds_renderer = Renderer([&] {
                    auto feeds = manager_.get_feeds_copy();
                    
                    Elements feed_elements;
                    if (feeds.empty()) {
                        feed_elements.push_back(text("No feeds added. Press 'a' to add one."));
                    } else {
                        for (size_t i = 0; i < feeds.size(); ++i) {
                            std::string prefix = (i == (size_t)selected_feed) ? "> " : "  ";
                            feed_elements.push_back(text(prefix + feeds[i].get_title()));
                        }
                    }

                    return window(text(" RSS Reader (Variant A) "),
                                //распологаем элементы друг под другом
                                vbox({
                                    text("Subscriptions:") | bold,
                                    separator(),
                                    vbox(feed_elements),
                                    separator(),
                                    text("Controls: [Up/Down] Navigate | [Enter] Select | [q] Quit") | dim,
                                }));
                });

                // обработка статей выбранной ленты
                auto articles_renderer = Renderer([&] {
                    const auto& articles = current_feed_cache.get_articles();
                    
                    Elements article_elements;
                    if (articles.empty()) {
                        article_elements.push_back(text("No articles in this feed."));
                    } else {
                        for (size_t i = 0; i < articles.size(); ++i) {
                            std::string prefix = (i == (size_t)selected_article) ? "> " : "  ";
                            article_elements.push_back(text(prefix + articles[i].title));
                        }
                    }

                    return window(text(" RSS Reader (" + current_feed_cache.get_title() + ") "),
                                vbox({
                                    text("Articles:") | bold,
                                    separator(),
                                    vbox(article_elements),
                                    separator(),
                                    text("Controls: [Up/Down] Navigate | [Esc] Back to Feeds | [q] Quit") | dim,
                                }));
                });

                auto renderer = Renderer([&] {
                    if (current_screen == ScreenState::FeedsList) {
                        return feeds_renderer->Render();
                    } else {
                        return articles_renderer->Render();
                    }
                });

                // Обработка нажатий клавиш
                auto component = CatchEvent(renderer, [&](Event event) {
                    if (event == Event::Character('q') || event == Event::Escape) {
                        if (current_screen == ScreenState::ArticlesList) {
                            current_screen = ScreenState::FeedsList;
                            return true;
                        } else {
                            screen.Exit();
                            return true;
                        }
                    }

                    auto feeds = manager_.get_feeds_copy();

                    if (current_screen == ScreenState::FeedsList) {
                        if (!feeds.empty()) {
                            if (event == Event::ArrowUp && selected_feed > 0) {
                                selected_feed--;
                                return true;
                            }
                            if (event == Event::ArrowDown && selected_feed < static_cast<int>(feeds.size()) - 1) {
                                selected_feed++;
                                return true;
                            }
                            if (event == Event::Return) {
                                current_feed_cache = feeds[selected_feed];
                                selected_article = 0;
                                current_screen = ScreenState::ArticlesList;
                                return true;
                            }
                        }
                    } else if (current_screen == ScreenState::ArticlesList) {
                        const auto& articles = current_feed_cache.get_articles();
                        if (!articles.empty()) {
                            if (event == Event::ArrowUp && selected_article > 0) {
                                selected_article--;
                                return true;
                            }
                            if (event == Event::ArrowDown && selected_article < static_cast<int>(articles.size()) - 1) {
                                selected_article++;
                                return true;
                            }
                        }
                    }

                    return false;
                });

                //отрисовываем интерфейс пока не q/ESC
                screen.Loop(component);
            }

        private:
            app::FeedManager manager_;
    };
    

}