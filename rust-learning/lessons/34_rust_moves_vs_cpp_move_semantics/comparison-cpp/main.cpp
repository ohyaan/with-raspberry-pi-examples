#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

struct Probe {
    inline static int copies = 0;
    inline static int moves = 0;
    int value;
    explicit Probe(int number) : value(number) {}
    Probe(const Probe& other) : value(other.value) { ++copies; }
    Probe(Probe&& other) noexcept : value(std::exchange(other.value, 0)) { ++moves; }
};

int main() {
    std::cout << std::boolalpha;
    auto source = std::make_unique<int>(46700);
    auto destination = std::move(source);
    assert(!source);
    assert(destination && *destination == 46700);
    std::cout << "owned=" << *destination << '\n';
    source = std::make_unique<int>(0);
    std::cout << "reused=" << *source << '\n';

    auto pending = std::make_unique<int>(-500);
    auto extracted = std::exchange(pending, nullptr);
    assert(!pending && extracted && *extracted == -500);
    std::cout << "slot empty=" << !pending << ", value=" << *extracted << '\n';

    auto repeated = [label = std::string("Pi 4B")] { return label.size(); };
    auto first = repeated();
    auto second = repeated();
    assert(first == 5 && second == 5);
    std::cout << "repeated=" << first << ',' << second << '\n';

    auto consume = [label = std::string("Pi 4B")]() mutable {
        return std::move(label);
    };
    auto consumed = consume();
    assert(consumed == "Pi 4B");
    std::cout << "consumed=" << consumed.size() << '\n';

    int original = 7;
    int copied = std::move(original);
    assert(original == 7 && copied == 7);
    std::cout << "copied=" << original << ',' << copied << '\n';

    Probe item(9);
    (void)std::move(item); // Casting alone does not call a move constructor.
    assert(item.value == 9 && Probe::copies == 0 && Probe::moves == 0);
    Probe moved(std::move(item));
    assert(moved.value == 9 && item.value == 0 && Probe::moves == 1);
    const Probe fixed(11);
    Probe from_const(std::move(fixed)); // const Probe&& cannot bind to Probe&&.
    assert(from_const.value == 11 && fixed.value == 11);
    assert(Probe::copies == 1 && Probe::moves == 1);
}
