#pragma once

#include <string>
#include <vector>
#include <pugixml.hpp>
#include <sstream>
#include <functional>
#include <algorithm>
#include <cctype>

namespace rss_reader::core {

    struct RenderedArticle {
        std::string text;
        std::vector<std::string> footnotes; // Сохраняем все уникальные ссылки
    };

    class HtmlRenderer {
    public:
        // Функция переноса текста по словам для соблюдения max_width
        static std::string wrap_paragraph(const std::string& text, size_t max_width) {
            if (max_width == 0 || text.empty()) return text;

            std::istringstream lines(text);
            std::string line;
            std::string wrapped_result;
            bool first_line = true;
            // В UTF-8 русская буква занимает несколько байтов, но одну позицию.
            auto text_width = [](const std::string& value) {
                return static_cast<size_t>(std::count_if(value.begin(), value.end(), [](unsigned char c) {
                    return (c & 0xC0) != 0x80;
                }));
            };

            while (std::getline(lines, line)) {
                if (!first_line) wrapped_result += "\n";
                first_line = false;
                std::istringstream stream(line);
                std::string word;
                std::string current_line;
                while (stream >> word) {
                    if (current_line.empty()) {
                        current_line = word;
                    } else if (text_width(current_line) + 1 + text_width(word) <= max_width) {
                        current_line += " " + word;
                    } else {
                        wrapped_result += current_line + "\n";
                        current_line = word;
                    }
                }
                wrapped_result += current_line;
            }
            if (text.back() == '\n') wrapped_result += "\n";
            return wrapped_result;
        }

        static RenderedArticle render(const std::string& html_content, size_t max_width = 80) {
            RenderedArticle result;
            
            // Если контент пустой, возвращаем пустышку
            if (html_content.empty()) {
                return result;
            }

            // Обернем в корневой тег на случай, если это просто текст или набор тегов без одного общего корня
            std::string wrapped_html = "<root>" + html_content + "</root>";

            pugi::xml_document doc;
            // Сохраняем пробелы между соседними инлайн-тегами.
            pugi::xml_parse_result parse_res = doc.load_string(wrapped_html.c_str(), pugi::parse_default | pugi::parse_ws_pcdata);
            
            std::vector<std::string> paragraphs;
            std::string current_paragraph_text;

            auto flush_paragraph = [&](const std::string& prefix = "") {
                if (!current_paragraph_text.empty()) {
                    // Очищаем лишние пробелы в начале/конце
                    size_t start = current_paragraph_text.find_first_not_of(" \t\n\r");
                    size_t end = current_paragraph_text.find_last_not_of(" \t\n\r");
                    if (start != std::string::npos && end != std::string::npos) {
                        std::string clean = prefix + current_paragraph_text.substr(start, end - start + 1);
                        paragraphs.push_back(wrap_paragraph(clean, max_width));
                    }
                    current_paragraph_text.clear();
                }
            };

            std::vector<std::string> footnotes;

            // XML-сущности уже декодированы парсером; неразрывные пробелы заменяем обычными.
            auto clean_html_entities = [](std::string text) {
                size_t pos = 0;
                while ((pos = text.find("&nbsp;", pos)) != std::string::npos) {
                    text.replace(pos, 6, " ");
                    pos += 1;
                }
                pos = 0;
                while ((pos = text.find("\xC2\xA0", pos)) != std::string::npos) {
                    text.replace(pos, 2, " ");
                    pos += 1;
                }
                return text;
            };

            // Рекурсивная функция обхода узлов дерева
            std::function<void(pugi::xml_node)> traverse = [&](pugi::xml_node node) {
                for (pugi::xml_node child : node.children()) {
                    std::string name = child.name();

                    if (name.empty()) {
                        // Это текстовый узел
                        std::string val = clean_html_entities(child.value());
                        // Нормализуем пробелы и переводы строк
                        for (char& c : val) {
                            if (c == '\n' || c == '\r' || c == '\t') c = ' ';
                        }
                        
                        current_paragraph_text += val;
                    } 
                    else if (name == "p" || name == "div" || name == "article" || name == "section") {
                        flush_paragraph();
                        traverse(child);
                        flush_paragraph();
                    } 
                    else if (name == "br") {
                        current_paragraph_text += "\n";
                    }
                    else if (name == "script" || name == "style") {
                        continue;
                    } 
                    else if (name == "h1" || name == "h2" || name == "h3" || name == "h4" || name == "h5" || name == "h6") {
                        flush_paragraph();
                        std::string saved_text = current_paragraph_text;
                        current_paragraph_text.clear();
                        traverse(child);
                        std::string h_text = current_paragraph_text;
                        current_paragraph_text = saved_text;
                        
                        paragraphs.push_back(wrap_paragraph("### " + h_text, max_width));
                    } 
                    else if (name == "b" || name == "strong" || name == "i" || name == "em" || name == "span") {
                        // Обходим содержимое инлайн-тегов, сохраняя исходные пробелы.
                        traverse(child);
                    } 
                    else if (name == "a") {
                        std::string href = child.attribute("href").value();
                        
                        std::string saved_text = current_paragraph_text;
                        current_paragraph_text.clear();
                        traverse(child);
                        std::string link_text = current_paragraph_text;
                        current_paragraph_text = saved_text;

                        if (link_text.empty()) {
                            link_text = href;
                        }

                        if (!href.empty()) {
                            auto found = std::find(footnotes.begin(), footnotes.end(), href);
                            size_t fn_num = found - footnotes.begin() + 1;
                            if (found == footnotes.end()) {
                                footnotes.push_back(href);
                            }
                            current_paragraph_text += link_text + " [" + std::to_string(fn_num) + "]";
                        } else {
                            current_paragraph_text += link_text;
                        }
                    } 
                    else if (name == "li") {
                        flush_paragraph();
                        std::string saved_text = current_paragraph_text;
                        current_paragraph_text.clear();
                        traverse(child);
                        std::string li_text = current_paragraph_text;
                        current_paragraph_text = saved_text;

                        paragraphs.push_back("  " + wrap_paragraph("- " + li_text, max_width > 2 ? max_width - 2 : max_width));
                    } 
                    else {
                        // Для любых других тегов просто спускаемся внутрь
                        traverse(child);
                    }
                }
            };

            if (parse_res) {
                traverse(doc.child("root"));
                flush_paragraph();
            } else {
                // Если парсинг не удался, убираем теги вручную
                std::string raw = html_content;
                std::string cleaned = "";
                bool in_tag = false;
                for (char c : raw) {
                    if (c == '<') in_tag = true;
                    else if (c == '>' && in_tag) { in_tag = false; cleaned += " "; }
                    else if (!in_tag) cleaned += c;
                }
                // В запасном варианте XML-сущности тоже нужно декодировать один раз.
                pugi::xml_document text_doc;
                if (text_doc.load_string(("<root>" + cleaned + "</root>").c_str())) {
                    cleaned = text_doc.child("root").child_value();
                }
                paragraphs.push_back(wrap_paragraph(clean_html_entities(cleaned), max_width));
            }

            // Формируем итоговый текст со списком сносок в конце
            std::ostringstream final_output;
            for (size_t i = 0; i < paragraphs.size(); ++i) {
                final_output << paragraphs[i];
                if (i + 1 < paragraphs.size()) {
                    final_output << "\n\n";
                }
            }

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
