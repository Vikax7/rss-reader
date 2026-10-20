#include "app_tui.hpp" 

int main() {
    try {
        rss_reader::tui::RssReaderApp app;
        app.run();
    } catch (const std::exception& e) {
        // Защита от непредвиденных исключений на верхнем уровне
        return 1;
    }
    return 0;
}