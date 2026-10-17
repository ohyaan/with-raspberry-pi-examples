#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char* argv[])
{
    try {
        if (argc != 3)
            throw std::runtime_error("Usage: rotation_plan NEUTRAL_US OFFSET_US");
        std::size_t used_neutral = 0, used_offset = 0;
        const std::string neutral_text(argv[1]), offset_text(argv[2]);
        const int neutral = std::stoi(neutral_text, &used_neutral);
        const int offset = std::stoi(offset_text, &used_offset);
        if (used_neutral != neutral_text.size() || used_offset != offset_text.size() ||
            neutral < 1400 || neutral > 1600 || offset < -100 || offset > 100)
            throw std::runtime_error("Example limits: neutral 1400..1600, offset -100..100.");
        const int pulse = neutral + offset;
        std::cout << "pulse_us=" << pulse << '\n'
                  << "period_ns=20000000\n"
                  << "duty_ns=" << pulse * 1000 << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
