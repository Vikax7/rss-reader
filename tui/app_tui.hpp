#pragma once

#include "app/feed_manager.hpp"
#include "core/html_renderer.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <cstdlib>
#include <sstream>
//для работы с файлом
#include <fstream>
#include <filesystem>
#include <unordered_set>

namespace rss_reader::tui {

    enum class ScreenState {
        FeedsList, // состояние со списком лент
        ArticlesList, // состояние со список внутри конкретной ленты
        ArticleView //состояние просмотра конкретной стати
    };

    class RssReaderApp {
        public:
            RssReaderApp() : manager_() {
                std::string filename = "feeds.txt";

                // Загружаем список прочитанных ссылок из файла
                std::unordered_set<std::string> read_links;
                if (std::filesystem::exists("read_articles.txt")) {
                    std::ifstream read_file("read_articles.txt");
                    std::string link;
                    while (std::getline(read_file, link)) {
                        if (!link.empty() && link.back() == '\r') link.pop_back();
                        if (!link.empty()) {
                            read_links.insert(link);
                        }
                    }
                }

                // Пытаемся прочитать сохраненные ленты из файла
                if (std::filesystem::exists(filename)) {
                    std::ifstream file(filename);
                    std::string url;
                    while (std::getline(file, url)) {
                        // Убираем возможные пробелы/переводы строк
                        if (!url.empty() && url.back() == '\r') url.pop_back();
                        if (!url.empty()) {
                            try {
                                manager_.fetch_and_add_feed(url);
                            } catch (...) {
                                // Игнорируем битые URL при автозагрузке
                            }
                        }
                    }
                }

                // Если ленты загрузились, проставляем статусы прочитанных статей
                if (!read_links.empty()) {
                    auto feeds = manager_.get_feeds_copy();
                    for (size_t f_idx = 0; f_idx < feeds.size(); ++f_idx) {
                        // Обновляем статьи внутри менеджера
                        // Т.к. get_feeds_copy дает копию, нам нужно пройтись по реальным объектам через модификацию в manager_ 
                        // Но проще пройтись после добавления. Сделаем метод отметки или обновим прямо здесь:
                    }
                }
                
                // Применим прочитанные ссылки ко всем загруженным лентам в менеджере
                apply_read_status(read_links);
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

                // Состояние добавления новой ленты
                bool is_adding_feed = false;
                std::string new_feed_url = "";
                std::string add_feed_status = "";

                //локальный кэш объекта Feed который пользователь выбрал на первом экране
                core::Feed current_feed_cache;
                //кэш статьи
                core::RenderedArticle current_article_cache;
                Element article_content;
                int article_scroll = 0;

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
                            auto row = text(prefix + feeds[i].get_title());
                            if (i == static_cast<size_t>(selected_feed)) row = row | focus;
                            feed_elements.push_back(row);
                        }
                    }

                    Elements main_vbox_items;
                    main_vbox_items.push_back(text("Subscriptions:") | bold);
                    main_vbox_items.push_back(separator());

                    // Если активен режим добавления ленты, показываем поле ввода
                    if (is_adding_feed) {
                        main_vbox_items.push_back(text("Add Feed URL: " + new_feed_url + "_") | color(Color::Yellow));
                        if (!add_feed_status.empty()) {
                            main_vbox_items.push_back(text(add_feed_status) | color(Color::Red));
                        }
                        main_vbox_items.push_back(separator());
                    }

                    main_vbox_items.push_back(vbox(feed_elements) | vscroll_indicator | yframe | flex);
                    main_vbox_items.push_back(separator());
                    main_vbox_items.push_back(text("Controls: [a] Add Feed | [d] Delete Feed | [Up/Down] Navigate | [Enter] Select | [q] Quit") | dim);

                    return window(text(" RSS Reader (Variant A) "), vbox(main_vbox_items));
                });

                // обработка статей выбранной ленты
                auto articles_renderer = Renderer([&] {
                    // Синхронизируем current_feed_cache с актуальными данными из manager_ (чтобы статус is_read отображался вовремя)
                    auto feeds = manager_.get_feeds_copy();
                    for (const auto& f : feeds) {
                        if (f.get_url() == current_feed_cache.get_url()) {
                            current_feed_cache = f;
                            break;
                        }
                    }

                    auto articles = get_filtered_articles();
                    
                    Elements article_elements;
                    if (articles.empty()) {
                        article_elements.push_back(text("No articles in this feed."));
                    } else {
                        for (size_t i = 0; i < articles.size(); ++i) {
                            std::string prefix = (i == (size_t)selected_article) ? "> " : "  ";
                            auto row = text(prefix + articles[i].title);
                            
                            // Если статья прочитана, делаем её тусклым светом (dim) без значков и галочек
                            if (articles[i].is_read) {
                                row = row | dim;
                            }

                            if (i == static_cast<size_t>(selected_article)) row = row | focus;
                            article_elements.push_back(row);
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

                    layout_elements.push_back(vbox(article_elements) | vscroll_indicator | yframe | flex);
                    layout_elements.push_back(separator());
                    layout_elements.push_back(text("Controls: [/] Search | [Up/Down] Navigate | [Enter] Read | [Esc] Back | [q] Quit") | dim);

                    return window(text(" RSS Reader (" + current_feed_cache.get_title() + ") "), vbox(layout_elements));
                });

                // Рисуем каждую строку отдельно: paragraph не сохраняет переводы строк.
                auto article_view_renderer = Renderer([&] {
                    Elements view_elements;
                    Elements lines;
                    std::istringstream content_stream(current_article_cache.text);
                    std::string line;
                    while (std::getline(content_stream, line)) {
                        lines.push_back(line.empty() ? text("") : paragraph(line));
                    }
                    article_content = vbox(lines);
                    view_elements.push_back(article_content | focusPosition(0, article_scroll) | vscroll_indicator | yframe | flex);
                    view_elements.push_back(separator());
                    view_elements.push_back(text("Controls: [1-9] Open Footnote in Browser | [Up/Down] Scroll | [Esc] Back | [q] Quit") | dim);

                    return window(text(" Article Viewer "), vbox(view_elements));
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
                    // Если мы в режиме ввода URL новой ленты
                    if (is_adding_feed) {
                        if (event == Event::Return) {
                            if (!new_feed_url.empty()) {
                                try {
                                    manager_.fetch_and_add_feed(new_feed_url);
                                    //сохраняем в файл
                                    save_feeds_to_file();
                                    is_adding_feed = false;
                                    new_feed_url.clear();
                                    add_feed_status.clear();
                                    return true;
                                } catch (const std::exception& e) {
                                    add_feed_status = "Error: " + std::string(e.what());
                                    return true;
                                }
                            }
                        }
                        if (event == Event::Escape) {
                            is_adding_feed = false;
                            new_feed_url.clear();
                            add_feed_status.clear();
                            return true;
                        }
                        if (event == Event::Backspace) {
                            if (!new_feed_url.empty()) {
                                new_feed_url.pop_back();
                            }
                            return true;
                        }
                        if (event.is_character()) {
                            new_feed_url += event.character();
                            return true;
                        }
                        return false;
                    }

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
                                // Удаляем целый символ UTF-8, включая все его байты.
                                size_t start = search_query.size() - 1;
                                while (start > 0 && (static_cast<unsigned char>(search_query[start]) & 0xC0) == 0x80) --start;
                                search_query.erase(start);
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

                    // Включение режима добавления ленты по нажатию 'a' на экране списка лент
                    if (event == Event::Character('a') && current_screen == ScreenState::FeedsList) {
                        is_adding_feed = true;
                        new_feed_url.clear();
                        add_feed_status.clear();
                        return true;
                    }

                    // Удаление выбранной ленты по нажатию 'd' на экране списка лент
                    if (event == Event::Character('d') && current_screen == ScreenState::FeedsList) {
                        auto feeds = manager_.get_feeds_copy();
                        if (!feeds.empty() && selected_feed < static_cast<int>(feeds.size())) {
                            manager_.remove_feed(selected_feed);
                            save_feeds_to_file();
                            if (selected_feed > 0 && selected_feed >= static_cast<int>(manager_.get_feeds_copy().size())) {
                                selected_feed--;
                            }
                            return true;
                        }
                    }

                    // Включение режима поиска по нажатию '/'
                    if (event == Event::Character('/') && current_screen == ScreenState::ArticlesList) {
                        is_searching = true;
                        return true;
                    }

                    // Обработка открытия сносок по цифровым клавишам на экране просмотра статьи
                    if (current_screen == ScreenState::ArticleView && event.is_character()) {
                        char c = event.character()[0];
                        if (c >= '1' && c <= '9') {
                            size_t index = c - '1'; // Преобразуем символ '1' в индекс 0, '2' в 1 и т.д.
                            if (index < current_article_cache.footnotes.size()) {
                                std::string url = current_article_cache.footnotes[index];
                                if (url.rfind("https://", 0) != 0 && url.rfind("http://", 0) != 0) return true;
                                // Экранируем URL, чтобы содержимое ссылки не выполнялось оболочкой.
                                std::string quoted_url = "'";
                                for (char ch : url) quoted_url += ch == '\'' ? "'\\''" : std::string(1, ch);
                                quoted_url += "'";
#ifdef __APPLE__
                                std::string command = "open " + quoted_url + " > /dev/null 2>&1 &";
#else
                                std::string command = "xdg-open " + quoted_url + " > /dev/null 2>&1 &";
#endif
                                std::system(command.c_str());
                                return true;
                            }
                        }
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

                    if (current_screen == ScreenState::ArticleView) {
                        if (event == Event::ArrowUp && article_scroll > 0) {
                            --article_scroll;
                            return true;
                        }
                        if (event == Event::ArrowDown && article_content && article_scroll + 1 < article_content->requirement().min_y) {
                            ++article_scroll;
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
                                // Помечаем выбранную статью как прочитанную в менеджере
                                auto filtered_articles = get_filtered_articles();
                                const auto& selected_art = filtered_articles[selected_article];
                                
                                manager_.mark_article_read(current_feed_cache.get_url(), selected_art.link);
                                save_read_articles(); // Сохраняем в файл прочитанных

                                // Обновляем локальный кеш
                                feeds = manager_.get_feeds_copy();
                                for (const auto& f : feeds) {
                                    if (f.get_url() == current_feed_cache.get_url()) {
                                        current_feed_cache = f;
                                        break;
                                    }
                                }

                                // Переходим к чтению статьи, обрабатываем HTML через HtmlRenderer
                                const auto& article = selected_art;
                                current_article_cache = core::HtmlRenderer::render(article.content);
                                if (current_article_cache.text.empty()) {
                                    current_article_cache.text = "RAW CONTENT / NO HTML TEXT FOUND:\n\n" + article.content;
                                }
                                article_scroll = 0;
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

            void save_feeds_to_file() {
                std::ofstream file("feeds.txt");
                auto feeds = manager_.get_feeds_copy();
                for (const auto& feed : feeds) {
                    if (!feed.get_url().empty() && feed.get_url().rfind("https://example.com", 0) != 0) {
                        file << feed.get_url() << "\n";
                    }
                }
            }

            void save_read_articles() {
                std::ofstream file("read_articles.txt");
                auto feeds = manager_.get_feeds_copy();
                for (auto& feed : feeds) {
                    for (const auto& article : feed.get_articles()) {
                        if (article.is_read && !article.link.empty()) {
                            file << article.link << "\n";
                        }
                    }
                }
            }

            void apply_read_status(const std::unordered_set<std::string>& read_links) {
                if (read_links.empty()) return;
                manager_.mark_articles_read_batch(read_links);
            }
    };
    

}