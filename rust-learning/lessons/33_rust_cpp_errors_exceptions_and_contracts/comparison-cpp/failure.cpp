#include <variant>
enum class Error { Format, Range };
std::variant<int, Error> parse() { return Error::Format; }
int main() {
    int value = parse();
    (void)value;
}
