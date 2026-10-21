#include <array>
#include <cassert>
#include <charconv>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <string_view>
#include <system_error>

enum class Kind { Format, Range };

class ParseError : public std::exception {
public:
    Kind kind;
    explicit ParseError(Kind value) : kind(value) {}
    const char* what() const noexcept override {
        return kind == Kind::Format ? "Format" : "Range";
    }
};

std::int32_t parse(std::string_view text) {
    auto digits = text;
    if (digits.starts_with('-')) digits.remove_prefix(1);
    if (digits.empty()) throw ParseError(Kind::Format);
    for (char digit : digits) {
        if (digit < '0' || digit > '9') throw ParseError(Kind::Format);
    }
    std::int32_t value = 0;
    auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec == std::errc::result_out_of_range) throw ParseError(Kind::Range);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        throw ParseError(Kind::Format);
    }
    return value;
}

struct Guard {
    int& closes;
    explicit Guard(int& counter) : closes(counter) {}
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;
    ~Guard() noexcept { ++closes; }
};

std::int32_t run(std::string_view text, int& closes) {
    Guard guard(closes);
    return parse(text);
}

void expect_error(std::string_view text, Kind expected) {
    try {
        (void)parse(text);
        assert(false && "input should have failed");
    } catch (const ParseError& error) {
        assert(error.kind == expected);
    }
}

int main() {
    int closes = 0;
    for (std::string_view text : std::array{"0", "-500", "bad", "2147483648"}) {
        try {
            // Parse before emitting a prefix: a failed call prints no partial OK line.
            auto value = run(text, closes);
            std::cout << "OK=" << value << '\n';
        } catch (const ParseError& error) {
            std::cout << "ERR=" << error.what() << '\n';
        }
    }
    assert(closes == 4);
    std::cout << "closes=" << closes << '\n';
    assert(parse("2147483647") == std::numeric_limits<std::int32_t>::max());
    assert(parse("-2147483648") == std::numeric_limits<std::int32_t>::min());
    assert(parse("-0") == 0);
    for (std::string_view text : std::array{"", "-", "+1", " 1", "1 ", "1x", "１２", "999999999999x"}) {
        expect_error(text, Kind::Format);
    }
    expect_error("2147483648", Kind::Range);
    expect_error("-2147483649", Kind::Range);
}
