#include <cassert>
#include <iostream>
#include <optional>

struct Trace {
    int opened = 0;
    int closed = 0;
};

class Lease {
    Trace& trace;

public:
    explicit Lease(Trace& value) : trace(value) { ++trace.opened; }
    ~Lease() noexcept { ++trace.closed; }
    Lease(const Lease&) = delete;
    Lease& operator=(const Lease&) = delete;
    Lease(Lease&&) = delete;
    Lease& operator=(Lease&&) = delete;
};

std::optional<int> run(Trace& trace, std::optional<int> reading) {
    Lease lease(trace);
    if (!reading) {
        return std::nullopt;
    }
    return *reading;
}

int main() {
    Trace trace;
    const auto normal = run(trace, 46700);
    const auto missing = run(trace, std::nullopt);
    assert(normal == 46700);
    assert(!missing);
    assert(trace.opened == 2 && trace.closed == 2);
    std::cout << "normal=" << *normal << '\n';
    std::cout << "missing=" << (!missing ? "true" : "false") << '\n';
    std::cout << "opened=" << trace.opened << ", closed=" << trace.closed << '\n';

    Trace boundaries;
    assert(run(boundaries, 0) == 0);
    assert(run(boundaries, -500) == -500);
    assert(boundaries.opened == 2 && boundaries.closed == 2);
    Trace scope;
    {
        Lease guard(scope);
        assert(scope.closed == 0);
    }
    assert(scope.closed == 1);
}
