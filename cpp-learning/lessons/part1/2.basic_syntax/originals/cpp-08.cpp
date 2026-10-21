// C-style alias (not recommended in modern C++)
typedef unsigned char byte;

// Modern C++ alias (preferred)
using byte = unsigned char;
using GpioConfigMap = std::unordered_map<int, std::string>;
