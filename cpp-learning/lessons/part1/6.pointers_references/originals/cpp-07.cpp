int original = 100;
int& ref = original; // 'ref' is now a synonym for 'original'

ref = 200; // Modifies 'original' to 200
std::cout << original << " " << ref << "\n"; // Prints: 200 200
