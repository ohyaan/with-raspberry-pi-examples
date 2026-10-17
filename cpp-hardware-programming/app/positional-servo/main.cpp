#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char* argv[])
{
    try {
        if (argc != 2) throw std::runtime_error("Usage: servo_plan PULSE_US");
        const std::string text(argv[1]);
        std::size_t used = 0;
        const int pulse_us = std::stoi(text, &used);
        if (used != text.size() || pulse_us < 1000 || pulse_us > 2000)
            throw std::runtime_error("This exercise permits 1000..2000 microseconds.");
        constexpr int period_ns = 20'000'000;
        const int duty_ns = pulse_us * 1000;
        std::cout << "period_ns=" << period_ns << '\n'
                  << "duty_ns=" << duty_ns << '\n'
                  << "duty_percent=" << 100.0 * duty_ns / period_ns << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
