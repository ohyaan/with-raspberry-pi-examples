#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class Sample {
    int reading_;
public:
    explicit Sample(int value) : reading_(value) {}
    Sample(const Sample&) = delete;
    Sample& operator=(const Sample&) = delete;
    Sample(Sample&&) = delete;
    Sample& operator=(Sample&&) = delete;
    int reading() const { return reading_; }
    void set_reading(int value) { reading_ = value; }
};

int main() {
    std::cout << std::boolalpha;
    auto owner = std::make_unique<Sample>(46700);
    const Sample* address = owner.get();
    auto transferred = std::move(owner);
    assert(!owner && transferred.get() == address);
    std::cout << "owner transfer=" << (transferred.get() == address) << '\n';

    std::vector<std::unique_ptr<Sample>> records;
    records.push_back(std::move(transferred));
    const auto old_capacity = records.capacity();
    records.reserve(old_capacity + 1);
    assert(records.capacity() > old_capacity);
    assert(records.front().get() == address);
    std::cout << "collection growth=" << (records.front().get() == address) << '\n';
    assert(records.front()->reading() == 46700);
    std::cout << "reading=" << records.front()->reading() << '\n';
    records.front()->set_reading(0);
    assert(records.front()->reading() == 0);
    assert(records.front().get() == address);
    std::cout << "updated=" << records.front()->reading() << '\n';

    auto ordinary = std::make_unique<std::string>("Pi 4B");
    std::string extracted = std::move(*ordinary);
    assert(extracted == "Pi 4B");
    std::cout << "ordinary bytes=" << extracted.size() << '\n';
    // address is not used after records destroys its owned Sample.
}
