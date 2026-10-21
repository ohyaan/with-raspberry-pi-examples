#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <utility>

struct Fixed {
    int value;
    std::optional<int> read() const { return value; }
};

struct Missing {
    std::optional<int> read() const { return std::nullopt; }
};

template<class T>
concept Readable = requires(const T& value) {
    { value.read() } -> std::same_as<std::optional<int>>;
};

template<Readable T>
std::optional<int> generic_read(const T& source) {
    return source.read();
}

struct Source {
    virtual std::optional<int> read() const = 0;
    virtual ~Source() = default;
};

template<Readable T>
class Adapter final : public Source {
    T value;

public:
    explicit Adapter(T source) : value(std::move(source)) {}
    std::optional<int> read() const override { return value.read(); }
};

struct Report {
    std::size_t present = 0;
    std::size_t missing = 0;
    std::int64_t sum = 0;
};

template<std::size_t N>
Report collect(const std::array<const Source*, N>& sources) {
    Report report;
    for (const auto* source : sources) {
        const auto value = source->read();
        if (value) {
            ++report.present;
            report.sum += *value;
        } else {
            ++report.missing;
        }
    }
    return report;
}

auto make_fixed(int value) { return Fixed{value}; }

int main() {
    Fixed concrete{46700};
    Adapter<Fixed> fixed(concrete);
    Adapter<Missing> missing(Missing{});
    Adapter<Fixed> negative(Fixed{-500});
    std::array<const Source*, 3> sources{&fixed, &missing, &negative};
    const auto report = collect(sources);
    assert(generic_read(concrete) == 46700);
    assert(fixed.read() == generic_read(concrete));
    assert(report.present == 2 && report.missing == 1 && report.sum == 46200);
    assert(!generic_read(Missing{}));
    assert(generic_read(make_fixed(0)) == 0);
    const auto empty = collect(std::array<const Source*, 0>{});
    assert(empty.present == 0 && empty.missing == 0 && empty.sum == 0);
    std::cout << "generic=" << *generic_read(concrete) << '\n';
    std::cout << "dynamic=" << *sources[0]->read() << '\n';
    std::cout << "present=" << report.present << ", missing=" << report.missing
              << ", sum=" << report.sum << '\n';
    std::cout << "factory=" << *generic_read(make_fixed(0)) << '\n';
}
