#include <cassert>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

struct State {
    virtual bool can_send() const = 0;
    virtual std::unique_ptr<State> open() const = 0;
    virtual std::unique_ptr<State> close() const = 0;
    virtual ~State() = default;
};

struct IdleState : State {
    bool can_send() const override { return false; }
    std::unique_ptr<State> open() const override;
    std::unique_ptr<State> close() const override { return nullptr; }
};

struct ReadyState : State {
    bool can_send() const override { return true; }
    std::unique_ptr<State> open() const override { return nullptr; }
    std::unique_ptr<State> close() const override;
};

struct ClosedState : State {
    bool can_send() const override { return false; }
    std::unique_ptr<State> open() const override { return nullptr; }
    std::unique_ptr<State> close() const override { return nullptr; }
};

std::unique_ptr<State> IdleState::open() const { return std::make_unique<ReadyState>(); }
std::unique_ptr<State> ReadyState::close() const { return std::make_unique<ClosedState>(); }

class Runtime {
    std::unique_ptr<State> state_ = std::make_unique<IdleState>();
    std::vector<int> readings_;
public:
    Runtime() = default;
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    Runtime(Runtime&&) = delete;
    Runtime& operator=(Runtime&&) = delete;
    bool open() {
        auto next = state_->open();
        if (!next) return false;
        state_ = std::move(next);
        return true;
    }
    bool send(int reading) {
        if (!state_->can_send()) return false;
        readings_.push_back(reading);
        return true;
    }
    bool close() {
        auto next = state_->close();
        if (!next) return false;
        state_ = std::move(next);
        return true;
    }
    const std::vector<int>& values() const { return readings_; }
};

class Closed {
    friend class Ready;
    std::vector<int> readings_;
    explicit Closed(std::vector<int> values) : readings_(std::move(values)) {}
public:
    Closed(const Closed&) = delete;
    Closed(Closed&&) = default;
    const std::vector<int>& values() const { return readings_; }
};

class Ready {
    friend class Idle;
    std::vector<int> readings_;
    Ready() = default;
public:
    Ready(const Ready&) = delete;
    Ready(Ready&&) = default;
    void send(int reading) { readings_.push_back(reading); }
    Closed close() && { return Closed(std::move(readings_)); }
};

class Idle {
public:
    Ready open() && { return Ready{}; }
};

int main() {
    std::cout << std::boolalpha;
    Runtime runtime;
    auto before = !runtime.send(46700);
    assert(before && runtime.values().empty());
    assert(!runtime.close());
    std::cout << "before ready rejected=" << before << '\n';
    assert(runtime.open());
    assert(!runtime.open());
    assert(runtime.send(0) && runtime.send(-500));
    assert(runtime.close());
    assert((runtime.values() == std::vector<int>{0, -500}));
    std::cout << "runtime closed count=" << runtime.values().size() << '\n';
    auto after = !runtime.send(46700);
    assert(after && !runtime.close() && !runtime.open());
    assert((runtime.values() == std::vector<int>{0, -500}));
    std::cout << "after close rejected=" << after << '\n';

    Idle idle;
    auto ready = std::move(idle).open();
    ready.send(0);
    ready.send(-500);
    auto closed = std::move(ready).close();
    assert((closed.values() == std::vector<int>{0, -500}));
    std::cout << "typed closed count=" << closed.values().size() << '\n';
    std::cout << "zero retained=" << (closed.values()[0] == 0) << '\n';
    std::cout << "negative retained=" << (closed.values()[1] == -500) << '\n';
}
