#include <string_view>
#include <iostream>
#include <string>

// String parameter: no copy is triggered, regardless of string size
void log_message(std::string_view message) {
    std::cout << "Log: " << message << "\n";
}

int main() {
    std::string dynamic_str = "Heap allocated string";
    log_message(dynamic_str);        // View of std::string
    log_message("Literal string");    // View of raw literal (zero heap allocation)
}
