#include <string>

// 1. Efficient: Direct initialization of member variable
class User {
    std::string name;
public:
    User(const std::string& username) : name(username) {}
};

// 2. Inefficient: Constructs empty 'name' string first, then overwrites it via assignment
class User2 {
    std::string name;
public:
    User2(const std::string& username) {
        name = username; 
    }
};
