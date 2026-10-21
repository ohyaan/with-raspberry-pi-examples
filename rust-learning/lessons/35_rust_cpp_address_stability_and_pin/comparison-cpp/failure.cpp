#include <utility>
struct Sample {
    Sample() = default;
    Sample(const Sample&) = delete;
    Sample(Sample&&) = delete;
};
int main() {
    Sample first;
    Sample second(std::move(first));
    (void)second;
}
