#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

enum class Action { Keep, Remove };
enum class Error { Closed, Exhausted, Empty };
class Bus {
    struct Entry {
        std::uint64_t id;
        std::function<Action(int)> callback;
        bool remove = false;
    };
    std::vector<Entry> entries_;
    std::uint64_t next_ = 1;
    bool closed_ = false;
public:
    Bus() = default;
    Bus(const Bus&) = delete;
    Bus& operator=(const Bus&) = delete;
    Bus(Bus&&) = delete;
    Bus& operator=(Bus&&) = delete;
    std::variant<std::uint64_t, Error> subscribe(std::function<Action(int)> callback) {
        if (closed_) return Error::Closed;
        if (!callback) return Error::Empty;
        if (next_ == std::numeric_limits<std::uint64_t>::max()) return Error::Exhausted;
        auto id = next_;
        entries_.push_back(Entry{id, std::move(callback)});
        ++next_;
        return id;
    }
    bool unsubscribe(std::uint64_t id) {
        auto before = entries_.size();
        std::erase_if(entries_, [id](const Entry& entry) { return entry.id == id; });
        return before != entries_.size();
    }
    std::variant<std::size_t, Error> publish(int reading) {
        if (closed_) return Error::Closed;
        std::size_t delivered = 0;
        for (auto& entry : entries_) {
            ++delivered;
            entry.remove = entry.callback(reading) == Action::Remove;
        }
        std::erase_if(entries_, [](const Entry& entry) { return entry.remove; });
        return delivered;
    }
    bool close() {
        if (closed_) return false;
        closed_ = true;
        entries_.clear();
        return true;
    }
};

int main() {
    auto log = std::make_shared<std::vector<std::string>>();
    Bus bus;
    auto a = std::get<std::uint64_t>(bus.subscribe([log](int value) {
        log->push_back("A:" + std::to_string(value));
        return Action::Keep;
    }));
    auto b = std::get<std::uint64_t>(bus.subscribe([log](int value) {
        log->push_back("B:" + std::to_string(value));
        return Action::Remove;
    }));
    auto c = std::get<std::uint64_t>(bus.subscribe([log](int value) {
        log->push_back("C:" + std::to_string(value));
        return Action::Keep;
    }));
    auto first = std::get<std::size_t>(bus.publish(0));
    assert(first == 3 && !bus.unsubscribe(b));
    assert(bus.unsubscribe(a) && !bus.unsubscribe(a));
    auto second = std::get<std::size_t>(bus.publish(-500));
    assert(second == 1);
    assert((*log == std::vector<std::string>{"A:0", "B:0", "C:0", "C:-500"}));
    std::cout << "first=" << first << ", second=" << second << '\n';
    std::string joined;
    for (const auto& entry : *log) {
        if (!joined.empty()) joined += '|';
        joined += entry;
    }
    std::cout << "log=" << joined << '\n';
    bool closed = bus.close();
    bool repeated = bus.close();
    assert(closed && !repeated && !bus.unsubscribe(c));
    auto rejected = bus.publish(46700);
    assert(std::get<Error>(rejected) == Error::Closed);
    assert(std::get<Error>(bus.subscribe([](int) { return Action::Keep; })) == Error::Closed);
    assert(log->size() == 4);
    std::cout << "closed=" << closed << ", repeated=" << repeated << '\n';
    std::cout << "after close=Closed\n";
    Bus empty;
    assert(std::get<Error>(empty.subscribe({})) == Error::Empty);
    assert(std::get<std::size_t>(empty.publish(0)) == 0);
    auto held = std::make_shared<int>(7);
    std::weak_ptr<int> observed = held;
    Bus owner;
    auto id = std::get<std::uint64_t>(owner.subscribe([held](int) { return Action::Keep; }));
    held.reset();
    assert(!observed.expired());
    assert(owner.unsubscribe(id) && observed.expired());
    auto closing = std::make_shared<int>(0);
    std::weak_ptr<int> closing_view = closing;
    auto ignored = owner.subscribe([closing](int) { return Action::Keep; });
    assert(std::holds_alternative<std::uint64_t>(ignored));
    closing.reset();
    assert(!closing_view.expired() && owner.close() && closing_view.expired());
    auto dropping = std::make_shared<int>(0);
    std::weak_ptr<int> dropping_view = dropping;
    int calls = 0;
    {
        Bus scoped;
        assert(std::holds_alternative<std::uint64_t>(scoped.subscribe([dropping, &calls](int) {
            ++calls;
            return Action::Keep;
        }))); // The returned ID is not retained.
        dropping.reset();
        assert(!dropping_view.expired());
        assert(std::get<std::size_t>(scoped.publish(0)) == 1);
    }
    assert(dropping_view.expired() && calls == 1); // No final event on destruction.
}
