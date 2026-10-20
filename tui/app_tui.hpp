#pragma once

#include "app/feed_manager.hpp"
#include "core/html_renderer.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

namespace rss_reader::tui {

    enum class ScreenState {
        FeedsList, // состояние со списком лент
        ArticlesList, // состояние со список внутри конкретной ленты
        ArticleView //состояние просмотра конкретной стати
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

                //состояние поиска
                bool is_searching = false;
                std::string search_query = "";  

                //локальный кэш объекта Feed который пользователь выбрал на первом экране
                core::Feed current_feed_cache;
                //кэш статьи
                core::RenderedArticle current_article_cache;

                // Вспомогательная функция для перевода строки в нижний регистр (для нечувствительного к регистру поиска)
                auto to_lower = [](std::string s) {
                    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
                    return s;
                };

                // Получение отфильтрованного списка статей
                auto get_filtered_articles = [&]() {
                    const auto& all_articles = current_feed_cache.get_articles();
                    if (search_query.empty()) {
                        return all_articles;
                    }
                    std::vector<core::Article> filtered;
                    std::string query_lower = to_lower(search_query);
                    for (const auto& article : all_articles) {
                        if (to_lower(article.title).find(query_lower) != std::string::npos) {
                            filtered.push_back(article);
                        }
                    }
                    return filtered;
                };

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
                    auto articles = get_filtered_articles();
                    
                    Elements article_elements;
                    if (articles.empty()) {
                        article_elements.push_back(text("No articles in this feed."));
                    } else {
                        for (size_t i = 0; i < articles.size(); ++i) {
                            std::string prefix = (i == (size_t)selected_article) ? "> " : "  ";
                            article_elements.push_back(text(prefix + articles[i].title));
                        }
                    }

                    Elements layout_elements;
                    layout_elements.push_back(text("Articles:") | bold);
                    layout_elements.push_back(separator());

                    // Если активен режим поиска, показываем поисковую строку
                    if (is_searching) {
                        layout_elements.push_back(text("Search: " + search_query + "_") | color(Color::Yellow));
                        layout_elements.push_back(separator());
                    } else if (!search_query.empty()) {
                        layout_elements.push_back(text("Filter: " + search_query + " (Press '/' to edit, Esc to clear)") | dim);
                        layout_elements.push_back(separator());
                    }

                    layout_elements.push_back(vbox(article_elements));
                    layout_elements.push_back(separator());
                    layout_elements.push_back(text("Controls: [/] Search | [Up/Down] Navigate | [Enter] Read | [Esc] Back | [q] Quit") | dim);

                    return window(text(" RSS Reader (" + current_feed_cache.get_title() + ") "), vbox(layout_elements));
                });

                //Рендерер экрана чтения статьи (с обработкой HtmlRenderer)
                auto article_view_renderer = Renderer([&] {
                    return window(text(" Article Viewer "),
                            vbox({
                                text(current_article_cache.text) | yframe,
                                separator(),
                                text("Controls: [Esc] Back to Articles | [q] Quit") | dim,
                            }));
                });

                auto renderer = Renderer([&] {
                    if (current_screen == ScreenState::FeedsList) {
                        return feeds_renderer->Render();
                    } else if (current_screen == ScreenState::ArticlesList) {
                        return articles_renderer->Render();
                    } else {
                        return article_view_renderer->Render();
                    }
                });

                // Обработка нажатий клавиш
                auto component = CatchEvent(renderer, [&](Event event) {
                    // Если мы в режиме ввода поискового запроса
                    if (is_searching) {
                        if (event == Event::Return) {
                            is_searching = false;
                            return true;
                        }
                        if (event == Event::Escape) {
                            is_searching = false;
                            search_query.clear();
                            selected_article = 0;
                            return true;
                        }
                        if (event == Event::Backspace) {
                            if (!search_query.empty()) {
                                search_query.pop_back();
                                selected_article = 0;
                            }
                            return true;
                        }
                        if (event.is_character()) {
                            search_query += event.character();
                            selected_article = 0;
                            return true;
                        }
                        return false;
                    }

                    if (event == Event::Character('q')) {
                        screen.Exit();
                        return true;
                    }

                    // Включение режима поиска по нажатию '/'
                    if (event == Event::Character('/') && current_screen == ScreenState::ArticlesList) {
                        is_searching = true;
                        return true;
                    }

                    // Обработка клавиши ESC в зависимости от экрана
                    if (event == Event::Escape) {
                        if (current_screen == ScreenState::ArticleView) {
                            current_screen = ScreenState::ArticlesList;
                            return true;
                        } else if (current_screen == ScreenState::ArticlesList) {
                            if (!search_query.empty()) {
                                search_query.clear();
                                selected_article = 0;
                                return true;
                            }
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
                                search_query.clear();
                                current_screen = ScreenState::ArticlesList;
                                return true;
                            }
                        }
                    } else if (current_screen == ScreenState::ArticlesList) {
                        auto articles = get_filtered_articles();
                        if (!articles.empty()) {
                            if (event == Event::ArrowUp && selected_article > 0) {
                                selected_article--;
                                return true;
                            }
                            if (event == Event::ArrowDown && selected_article < static_cast<int>(articles.size()) - 1) {
                                selected_article++;
                                return true;
                            }
                            if (event == Event::Return) {
                                // Переходим к чтению статьи, обрабатываем HTML через HtmlRenderer
                                const auto& article = articles[selected_article];
                                current_article_cache = core::HtmlRenderer::render(article.content);
                                current_screen = ScreenState::ArticleView;
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