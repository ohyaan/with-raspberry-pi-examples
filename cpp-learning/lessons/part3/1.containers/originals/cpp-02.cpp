#include <array>

std::array<int, 3> coords{10, 20, 30};
int val = coords.at(2); // Bound-checked access
