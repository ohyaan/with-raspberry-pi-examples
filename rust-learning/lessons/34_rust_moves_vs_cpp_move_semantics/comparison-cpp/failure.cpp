#include <memory>
#include <utility>
int main() {
    const auto owner = std::make_unique<int>(46700);
    auto destination = std::move(owner);
    (void)destination;
}
