#include <algorithm>
#include <vector>

void modify() {
    std::vector<int> nums = {-1, 2, -3};
    // Replace all negative numbers with 0
    std::replace_if(nums.begin(), nums.end(), [](int n) { return n < 0; }, 0);
}
