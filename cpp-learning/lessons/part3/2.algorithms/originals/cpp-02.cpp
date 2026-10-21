#include <algorithm>
#include <vector>

void verify() {
    std::vector<int> nums = {1, 2, 3};
    bool all_positive = std::all_of(nums.begin(), nums.end(), [](int n) { return n > 0; });
}
