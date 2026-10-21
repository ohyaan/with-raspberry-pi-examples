class Counter {
    int count = 0;
public:
    // 1. Prefix: ++c (Returns reference to updated object)
    Counter& operator++() {
        count++;
        return *this;
    }

    // 2. Postfix: c++ (Returns copy of old object, takes dummy int parameter)
    Counter operator++(int) {
        Counter temp = *this; // Keep copy of old state
        count++;              // Perform increment
        return temp;          // Return old state
    }
};
