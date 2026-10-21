#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <utility>

struct Value;
struct Missing;
struct Visitor {
    virtual ~Visitor() = default;
    virtual void visit(const Value&) = 0;
    virtual void visit(const Missing&) = 0;
};
struct Item {
    virtual ~Item() = default;
    virtual void accept(Visitor&) const = 0;
};
struct Value final : Item {
    explicit Value(int reading) : reading(reading) {}
    int reading;
    void accept(Visitor& visitor) const override { visitor.visit(*this); }
};
struct Missing final : Item {
    explicit Missing(std::string reason) : reason(std::move(reason)) {}
    std::string reason;
    void accept(Visitor& visitor) const override { visitor.visit(*this); }
};
struct Report final : Visitor {
    std::int64_t sum = 0;
    std::size_t missing = 0;
    std::string labels;
    void append(const std::string& text) {
        if (!labels.empty()) labels += '|';
        labels += text;
    }
    void visit(const Value& value) override {
        sum += value.reading;
        append("value=" + std::to_string(value.reading));
    }
    void visit(const Missing& value) override {
        ++missing;
        append("missing=" + value.reason);
    }
};
struct ZeroCount final : Visitor {
    std::size_t count = 0;
    void visit(const Value& value) override { count += value.reading == 0; }
    void visit(const Missing&) override {}
};
// A separate display interface accepts new types, not arbitrary Visitor variants.
struct Describe {
    virtual ~Describe() = default;
    virtual std::string describe() const = 0;
};
struct Custom final : Describe {
    explicit Custom(std::string label) : label(std::move(label)) {}
    std::string label;
    std::string describe() const override { return "custom=" + label; }
};

int main() {
    Value zero(0), negative(-500), positive(46700);
    Missing missing("offline");
    std::array<const Item*, 4> items{&zero, &negative, &missing, &positive};
    Report report;
    ZeroCount zeros;
    for (const Item* item : items) {
        item->accept(report);
        item->accept(zeros);
    }
    assert(report.sum == 46200 && report.missing == 1 && zeros.count == 1);
    assert(report.labels == "value=0|value=-500|missing=offline|value=46700");
    std::cout << "sum=" << report.sum << ", missing=" << report.missing << '\n';
    std::cout << "labels=" << report.labels << '\n';
    std::cout << "zero=" << zeros.count << '\n';
    Custom custom("Pi 4B");
    const Describe& display = custom;
    assert(display.describe() == "custom=Pi 4B");
    std::cout << "open=" << display.describe() << '\n';
    Report empty;
    assert(empty.sum == 0 && empty.missing == 0 && empty.labels.empty());
    Value smallest(std::numeric_limits<int>::min());
    Value largest(std::numeric_limits<int>::max());
    Report extremes;
    smallest.accept(extremes);
    largest.accept(extremes);
    assert(extremes.sum == -1);
    Missing blank("");
    Report blank_reason;
    blank.accept(blank_reason);
    assert(blank_reason.missing == 1 && blank_reason.labels == "missing=");
}
