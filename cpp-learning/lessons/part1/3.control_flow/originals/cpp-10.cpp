for (int i = 1; i <= 10; ++i) {
    if (i == 4) {
        continue; // Skip printing 4
    }
    if (i == 8) {
        break; // Exit the loop entirely when reaching 8
    }
    std::cout << i << " ";
}
// Output: 1 2 3 5 6 7
