#pragma once

#include <string>
#include <vector>
#include <pugixml.hpp>
#include <sstream>
#include <regex>

namespace rss_reader::core {

    struct RenderedArticle {
        std::string text;
        std::vector<std::string> footnotes; // сохраняем все уникальные ссылки
    };

    class HtmlRenderer {
        public:
            static RenderedArticle render(const std::string& html_content, [[maybe_unused]] size_t max_width = 80) {
                RenderedArticle result;
                
                // Если контент пустой, возвращаем пустышку
                if (html_content.empty()) {
                    return result;
                }
                // Обернем в корневой тег на случай, если это просто текст или набор тегов без одного общего корня
                std::string wrapped_html = "<root>" + html_content + "</root>";

                pugi::xml_document doc;
                // Используем pugi::parse_default для мягкого парсинга HTML-фрагментов
                pugi::xml_parse_result parse_res = doc.load_string(wrapped_html.c_str(), pugi::parse_default);
                
                //строковый поток, куда мы будем записывать готовый текст
                std::ostringstream text_builder;
                std::vector<std::string> footnotes;

                //Рекурсивная функция обхода узлов дерева
                std::function<void(pugi::xml_node)> traverse = [&](pugi::xml_node node) {
                    for (pugi::xml_node child : node.children()) {
                        std::string name = child.name();

                        if (name.empty()) {
                            // Это текстовый узел
                            std::string val = child.value();
                            // Нормализуем пробелы и переводы строк
                            text_builder << val;
                        } else if (name == "p" || name == "div" || name == "br") {
                            traverse(child);
                            text_builder << "\n\n";
                        } else if (name == "h1" || name == "h2" || name == "h3" || name == "h4" || name == "h5" || name == "h6") {
                            text_builder << "\n";
                            traverse(child);
                            text_builder << "\n\n";
                        } else if (name == "b" || name == "strong" || name == "i" || name == "em") {
                            // Пока просто обходим текст внутри форматирования
                            traverse(child);
                        } else if (name == "a") {
                            std::string href = child.attribute("href").value();
                            // Собираем текст ссылки
                            std::ostringstream link_text_stream;
                            // Временный обход для текста внутри тега <a>
                            for (pugi::xml_node sub_child : child.children()) {
                                std::string sub_name = sub_child.name();
                                if (sub_name.empty()) {
                                    link_text_stream << sub_child.value();
                                } else {
                                    // Рекурсим глубже, если там разметка
                                    // Для простоты пока вытащим текст
                                    link_text_stream << sub_child.child_value();
                                }
                            }
                            std::string link_text = link_text_stream.str();
                            if (link_text.empty()) {
                                link_text = href;
                            }

                            if (!href.empty()) {
                                footnotes.push_back(href);
                                size_t fn_num = footnotes.size();
                                text_builder << link_text << " [" << fn_num << "]";
                            } else {
                                text_builder << link_text;
                            }

                        } else if (name == "li") {
                            text_builder << "  - ";
                            traverse(child);
                            text_builder << "\n";
                        } else {
                            // Неизвестные теги — просто пропускаем тег, но рекурсивно вытаскиваем текст из них
                            traverse(child);
                        }
                    }

                };

                if (parse_res) {
                    traverse(doc.child("root"));
                } else {
                    // Если парсинг не удался (например, чистый текст), отдаем как есть
                    text_builder << html_content;
                }

                std::string raw_text = text_builder.str();

                // Формируем итоговый текст со списком сносок в конце
                std::ostringstream final_output;
                final_output << raw_text;

                if (!footnotes.empty()) {
                    final_output << "\n\nLinks:\n";
                    for (size_t i = 0; i < footnotes.size(); ++i) {
                        final_output << "[" << (i + 1) << "] " << footnotes[i] << "\n";
                    }
                }

                result.text = final_output.str();
                result.footnotes = footnotes;
                return result;
            }
    };

}