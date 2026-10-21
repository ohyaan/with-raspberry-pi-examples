#include <memory>

int main() {
    auto first = std::make_unique<int>(46700);
    auto second = first;
}
