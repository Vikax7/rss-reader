#include <iostream>
#include "../core/article.hpp"

int main() {
    rss::core::Article test_article{"Заголовок статьи", "https://example.com", "Текст статьи"};
    
    std::cout << "RSS Reader запущен!" << std::endl;
    std::cout << "Тестовая статья: " << test_article.title << std::endl;
    
    return 0;
}