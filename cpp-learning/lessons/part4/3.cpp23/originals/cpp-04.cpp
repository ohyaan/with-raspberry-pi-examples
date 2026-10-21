#include <utility>

struct SensorReader {
    int value = 42;
    
    // 'self' explicitly represents the instance, deducing type and constness
    template <typename Self>
    int get_value(this Self&& self) {
        // Keeps track of whether 'self' was passed as const or rvalue
        return std::forward<Self>(self).value;
    }
};

int main() {
    SensorReader reader;
    const SensorReader const_reader;
    
    int val1 = reader.get_value();        // self is SensorReader&
    int val2 = const_reader.get_value();  // self is const SensorReader&
}
