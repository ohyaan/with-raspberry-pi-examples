#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

struct Body {
    int x;
    std::optional<int> velocity;
    bool operator==(const Body&) const = default;
};
void step_records(std::array<Body, 2>& bodies) {
    for (auto& body : bodies) if (body.velocity) body.x += *body.velocity;
}
struct OwnerTag {};
class World;
class Handle {
    friend class World;
    std::shared_ptr<const OwnerTag> owner_;
    std::size_t slot_ = 0;
    std::uint8_t generation_ = 0;
    Handle(std::shared_ptr<const OwnerTag> owner, std::size_t slot, std::uint8_t generation)
        : owner_(std::move(owner)), slot_(slot), generation_(generation) {}
public:
    Handle() = default; // Explicit invalid handle; Rust below issues handles only.
    std::size_t slot() const { return slot_; }
    unsigned generation() const { return generation_; }
};
class World {
    struct Slot { std::uint8_t generation = 0; bool retired = false; };
    std::shared_ptr<const OwnerTag> owner_ = std::make_shared<OwnerTag>();
    std::array<Slot, 2> slots_{};
    std::array<std::optional<int>, 2> positions_{};
    std::array<std::optional<int>, 2> velocities_{};
    bool valid(const Handle& handle) const {
        return handle.owner_ == owner_ && handle.slot_ < slots_.size()
            && positions_[handle.slot_].has_value()
            && slots_[handle.slot_].generation == handle.generation_;
    }
public:
    World() = default;
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) = delete;
    World& operator=(World&&) = delete;
    std::optional<Handle> insert(Body body) {
        for (std::size_t index = 0; index < slots_.size(); ++index) {
            if (!positions_[index] && !slots_[index].retired) {
                positions_[index] = body.x;
                velocities_[index] = body.velocity;
                return Handle(owner_, index, slots_[index].generation);
            }
        }
        return std::nullopt;
    }
    std::optional<Body> inspect(const Handle& handle) const {
        if (!valid(handle)) return std::nullopt;
        return Body{*positions_[handle.slot_], velocities_[handle.slot_]};
    }
    bool set_velocity(const Handle& handle, std::optional<int> velocity) {
        if (!valid(handle)) return false;
        velocities_[handle.slot_] = velocity;
        return true;
    }
    bool remove(const Handle& handle) {
        if (!valid(handle)) return false;
        auto index = handle.slot_;
        positions_[index].reset();
        velocities_[index].reset(); // Clear every component before slot reuse.
        if (slots_[index].generation == std::numeric_limits<std::uint8_t>::max()) {
            slots_[index].retired = true;
        } else {
            ++slots_[index].generation;
        }
        return true;
    }
    void step() {
        for (std::size_t index = 0; index < slots_.size(); ++index) {
            if (positions_[index] && velocities_[index])
                *positions_[index] += *velocities_[index];
        }
    }
};

int main() {
    std::array<Body, 2> records{Body{0, 1}, Body{10, std::nullopt}};
    World world;
    auto a = world.insert(records[0]).value();
    auto b = world.insert(records[1]).value();
    step_records(records);
    world.step();
    assert(world.inspect(a) == records[0] && world.inspect(b) == records[1]);
    assert(world.set_velocity(b, -2));
    records[1].velocity = -2;
    step_records(records);
    world.step();
    assert(world.inspect(a) == records[0] && world.inspect(b) == records[1]);
    assert(records[0].x == 2 && records[1].x == 8);
    std::cout << "matched positions=" << records[0].x << ',' << records[1].x << '\n';
    auto snapshot = world.inspect(a).value();
    auto old = a;
    assert(world.remove(a) && !world.remove(old));
    auto replacement = world.insert(Body{99, std::nullopt}).value();
    bool reused = old.slot() == replacement.slot();
    bool stale = !world.inspect(old);
    assert(reused && stale && replacement.generation() == 1);
    assert(!world.set_velocity(old, 7));
    world.step();
    assert((world.inspect(replacement) == Body{99, std::nullopt}));
    assert(snapshot.x == 2); // An owned past snapshot, not a live view.
    std::cout << "reused=" << reused << ", stale=" << stale << '\n';
    World foreign;
    auto collision = foreign.insert(Body{7, 0}).value();
    assert(collision.slot() == old.slot() && collision.generation() == old.generation());
    assert(!world.inspect(collision) && !foreign.inspect(old) && !world.inspect(Handle{}));
    std::cout << "foreign rejected=1\n";
    World limited;
    Handle ancient;
    for (unsigned generation = 0; generation <= 255; ++generation) {
        auto current = limited.insert(Body{0, 0}).value();
        if (generation == 0) ancient = current;
        assert(current.slot() == 0 && current.generation() == generation);
        assert(limited.remove(current));
    }
    auto after_retirement = limited.insert(Body{5, std::nullopt}).value();
    assert(after_retirement.slot() == 1 && !limited.inspect(ancient));
    assert(!limited.insert(Body{0, 0}));
    std::cout << "retired slot=" << after_retirement.slot() << '\n';
    Handle surviving;
    {
        World temporary;
        surviving = temporary.insert(Body{0, 1}).value();
    }
    assert(!foreign.inspect(surviving));
    assert(foreign.inspect(collision)->velocity == 0);
    assert(foreign.set_velocity(collision, std::nullopt));
    foreign.step();
    assert((foreign.inspect(collision) == Body{7, std::nullopt}));
}
