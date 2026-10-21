struct Point {
    double x; // public by default
    double y;
};

class BankAccount {
    double balance; // private by default
public:
    void deposit(double amount) { balance += amount; }
};
