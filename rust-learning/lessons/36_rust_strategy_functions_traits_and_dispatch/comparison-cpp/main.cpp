#include <array>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <limits>
#include <span>

struct Policy {
    virtual bool accept(int reading) const = 0;
    virtual ~Policy() = default;
};

struct NonNegative : Policy {
    bool accept(int reading) const override { return reading >= 0; }
};

struct AtLeast : Policy {
    int minimum;
    explicit AtLeast(int value) : minimum(value) {}
    bool accept(int reading) const override { return reading >= minimum; }
};

bool nonnegative(int reading) { return reading >= 0; }

template<class Predicate>
std::size_t count_with(std::span<const int> readings, Predicate predicate) {
    std::size_t accepted = 0;
    for (int reading : readings) {
        if (predicate(reading)) ++accepted;
    }
    return accepted;
}

std::size_t count_policy(std::span<const int> readings, const Policy& policy) {
    return count_with(readings, [&policy](int reading) { return policy.accept(reading); });
}

int main() {
    const std::array readings{-500, 0, 46700};
    const NonNegative ordinary;
    const AtLeast warm(40000);
    assert(count_with(readings, nonnegative) == 2);
    std::cout << "nonnegative=" << count_with(readings, nonnegative) << '\n';
    assert(count_policy(readings, warm) == 1);
    std::cout << "warm=" << count_policy(readings, warm) << '\n';
    int threshold = 0;
    auto captured = [threshold](int reading) { return reading >= threshold; };
    assert(count_with(readings, captured) == 2);
    std::cout << "captured=" << count_with(readings, captured) << '\n';

    // All pointers are non-null and their stack owners outlive this selection.
    const std::array<const Policy*, 2> candidates{&ordinary, &warm};
    bool choose_warm = true;
    const Policy& selected = *candidates[choose_warm ? 1 : 0];
    std::cout << "dynamic=" << count_policy(readings, selected) << '\n';
    assert(count_policy(readings, *candidates[0]) == 2);
    assert(count_policy(readings, *candidates[1]) == 1);
    const AtLeast changed(-1000);
    std::cout << "changed=" << count_policy(readings, changed) << '\n';
    assert(count_policy(readings, changed) == 3);

    std::size_t calls = 0;
    auto counted = [&calls](int reading) { ++calls; return reading >= 0; };
    auto accepted = count_with(readings, counted);
    assert(accepted == 2 && calls == readings.size());
    std::cout << "stateful=" << accepted << ", calls=" << calls << '\n';
    assert((readings == std::array{-500, 0, 46700}));
    std::cout << "retained=" << readings.size() << '\n';

    const std::array<int, 0> empty{};
    assert(count_policy(empty, ordinary) == 0);
    assert(count_with(empty, counted) == 0 && calls == 3);
    const std::array limits{std::numeric_limits<int>::min(), 0, std::numeric_limits<int>::max()};
    assert(count_policy(limits, ordinary) == 2);
    assert(count_policy(std::array{40000}, warm) == 1);
}
