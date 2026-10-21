#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <variant>

enum class Error { MissingLower, MissingUpper, Bounds, Interval };

const char* name(Error error) {
    switch (error) {
    case Error::MissingLower: return "MissingLower";
    case Error::MissingUpper: return "MissingUpper";
    case Error::Bounds: return "Bounds";
    case Error::Interval: return "Interval";
    }
    return "Unknown";
}

class Config {
    int lower_;
    int upper_;
    std::uint32_t period_;
    Config(int lower, int upper, std::uint32_t period)
        : lower_(lower), upper_(upper), period_(period) {}
public:
    static std::variant<Config, Error> make(int lower, int upper, std::uint32_t period = 1000);
    int lower() const { return lower_; }
    int upper() const { return upper_; }
    std::uint32_t period() const { return period_; }
};

std::variant<Config, Error> Config::make(int lower, int upper, std::uint32_t period) {
    if (lower > upper) return Error::Bounds;
    if (period == 0 || period > 60000) return Error::Interval;
    return Config(lower, upper, period);
}

class Builder {
    std::optional<int> lower_;
    std::optional<int> upper_;
    std::optional<std::uint32_t> period_;
public:
    Builder& lower(int value) { lower_ = value; return *this; }
    Builder& upper(int value) { upper_ = value; return *this; }
    Builder& period(std::uint32_t value) { period_ = value; return *this; }
    std::variant<Config, Error> build() const {
        if (!lower_) return Error::MissingLower;
        if (!upper_) return Error::MissingUpper;
        return Config::make(*lower_, *upper_, period_.value_or(1000));
    }
};

int main() {
    auto direct_result = Config::make(0, 46700);
    auto direct = std::get<Config>(direct_result);
    assert(direct.lower() == 0 && direct.upper() == 46700 && direct.period() == 1000);
    std::cout << "direct=" << direct.lower() << ".." << direct.upper()
              << ", period=" << direct.period() << '\n';
    auto built_result = Builder{}.lower(-500).upper(0).period(250).build();
    auto built = std::get<Config>(built_result);
    assert(built.lower() == -500 && built.upper() == 0 && built.period() == 250);
    std::cout << "built=" << built.lower() << ".." << built.upper()
              << ", period=" << built.period() << '\n';
    auto missing = Builder{}.build();
    assert(std::get<Error>(missing) == Error::MissingLower);
    std::cout << "missing=" << name(std::get<Error>(missing)) << '\n';
    auto zero = Builder{}.lower(0).upper(46700).period(0).build();
    assert(std::get<Error>(zero) == Error::Interval);
    std::cout << "zero period=" << name(std::get<Error>(zero)) << '\n';
    auto bounds = Config::make(1, 0);
    assert(std::get<Error>(bounds) == Error::Bounds);
    std::cout << "invalid bounds=" << name(std::get<Error>(bounds)) << '\n';
    auto repeated = Builder{}.lower(0).upper(0).period(250).period(500).build();
    assert(std::get<Config>(repeated).period() == 500);
    std::cout << "last period=" << std::get<Config>(repeated).period() << '\n';

    assert(std::get<Error>(Builder{}.lower(0).build()) == Error::MissingUpper);
    assert(std::get<Error>(Builder{}.period(0).build()) == Error::MissingLower);
    assert(std::get<Error>(Config::make(1, 0, 0)) == Error::Bounds);
    assert(std::get<Error>(Config::make(0, 0, 60001)) == Error::Interval);
    for (auto period : {1u, 60000u}) {
        auto value = Config::make(0, 0, period);
        assert(std::get<Config>(value).period() == period);
    }
    auto extremes = Config::make(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
    assert(std::holds_alternative<Config>(extremes));
}
