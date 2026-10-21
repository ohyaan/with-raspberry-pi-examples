#include <mdspan>
#include <vector>

int main() {
    // 1D flat layout of a 3x3 grid
    std::vector<int> grid_data = {
        1, 2, 3,
        4, 5, 6,
        7, 8, 9
    };
    
    // Create a 2D view (3 rows, 3 columns) pointing to the vector
    auto grid = std::mdspan(grid_data.data(), 3, 3);
    
    // Access elements using modern multidimensional subscript operator
    int val = grid[1, 1]; // Value is 5
    
    grid[0, 2] = 99; // Modifies the underlying vector element
    
    return 0;
}
