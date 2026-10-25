// Software-only link-time I2C substitute. Never opens an actual device.
#include <cerrno>
#include <csignal>
#include <cstdarg>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <linux/i2c-dev.h>
#include <string>
#include <unistd.h>

namespace {
constexpr int fake_fd = 777;
unsigned writes = 0;
std::ofstream log_file() {
    const char* name = std::getenv("OLED_TEST_LOG");
    if (!name) {
        std::abort();
    }
    return std::ofstream(name, std::ios::app);
}
} // namespace

extern "C" int __wrap_open(const char* path, int, ...) {
    if (std::string(path) != "/unverified-device") {
        errno = ENOENT;
        return -1;
    }
    log_file() << "OPEN\n";
    return fake_fd;
}
extern "C" int __wrap_ioctl(int fd, unsigned long request, ...) {
    if (fd != fake_fd || request != I2C_SLAVE) {
        errno = ENOTTY;
        return -1;
    }
    va_list arguments;
    va_start(arguments, request);
    const unsigned address = va_arg(arguments, unsigned);
    va_end(arguments);
    log_file() << "ADDRESS " << address << '\n';
    if (std::getenv("OLED_TEST_ADDRESS_FAIL")) {
        errno = EIO;
        return -1;
    }
    return 0;
}
extern "C" ssize_t __real_write(int, const void*, size_t);
extern "C" ssize_t __wrap_write(int fd, const void* data, size_t count) {
    if (fd != fake_fd) {
        return __real_write(fd, data, count);
    }
    auto log = log_file();
    log << "WRITE ";
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < count; ++i) {
        log << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<unsigned>(bytes[i]);
    }
    log << '\n';
    ++writes;
    if (writes == 40 && std::getenv("OLED_TEST_FAIL")) {
        errno = EIO;
        return -1;
    }
    if (writes == 40 && std::getenv("OLED_TEST_SHORT")) {
        return static_cast<ssize_t>(count - 1);
    }
    if (writes == 40 && std::getenv("OLED_TEST_SIGNAL")) {
        std::raise(SIGINT);
    }
    return static_cast<ssize_t>(count);
}
extern "C" int __real_close(int);
extern "C" int __wrap_close(int fd) {
    if (fd != fake_fd) {
        return __real_close(fd);
    }
    log_file() << "CLOSE\n";
    return 0;
}
