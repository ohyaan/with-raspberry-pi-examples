int battery_level = 15;
if (battery_level < 20) {
    std::cout << "Warning: Battery low!\n";
} else if (battery_level == 100) {
    std::cout << "Battery fully charged.\n";
} else {
    std::cout << "Battery status normal.\n";
}
