#include <functional>
#include <memory>
#include <utility>
int main() {
    auto owner = std::make_unique<int>(0);
    std::function<void()> callback = [owner = std::move(owner)] { (void)*owner; };
}
