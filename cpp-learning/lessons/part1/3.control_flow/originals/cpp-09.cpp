std::vector<int> readings = {23, 25, 22, 28};

// 'index' is created locally, incremented in the loop, and goes out of scope after the loop
for (int index = 0; int temp : readings) {
    std::cout << "Sensor " << index++ << " read: " << temp << "C\n";
}
