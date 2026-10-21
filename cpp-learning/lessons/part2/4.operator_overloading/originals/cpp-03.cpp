#include <iostream>
#include <string>

class User {
    std::string name;
    int age;
public:
    User(const std::string& n, int a) : name(n), age(a) {}

    friend std::ostream& operator<<(std::ostream& os, const User& user);
    friend std::istream& operator>>(std::istream& is, User& user);
};

std::ostream& operator<<(std::ostream& os, const User& user) {
    os << "User: " << user.name << " (Age: " << user.age << ")";
    return os; // Returns stream reference for chaining
}

std::istream& operator>>(std::istream& is, User& user) {
    is >> user.name >> user.age;
    return is;
}
