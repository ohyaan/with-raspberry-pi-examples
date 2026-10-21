#include <iostream>
#include <cstdint>

int main() {
    uint8_t gpio_register = 0b0000'0000; // All pins set to LOW (0)
    const uint8_t PIN_3_MASK = (1 << 3); // Bitmask representing pin 3 (0b0000'1000)
    
    // 1. Set Pin 3 to HIGH (Using OR)
    gpio_register |= PIN_3_MASK; 
    // gpio_register becomes 0b0000'1000
    
    // 2. Check if Pin 3 is HIGH (Using AND)
    bool is_pin3_high = (gpio_register & PIN_3_MASK) != 0; 
    // Evaluates to true
    
    // 3. Toggle Pin 3 (Using XOR)
    gpio_register ^= PIN_3_MASK; 
    // gpio_register goes back to 0b0000'0000
    
    // 4. Clear Pin 3 to LOW (Using AND with NOT mask)
    gpio_register &= ~PIN_3_MASK; 
    // Guarantees pin 3 is set to 0
    
    return 0;
}
