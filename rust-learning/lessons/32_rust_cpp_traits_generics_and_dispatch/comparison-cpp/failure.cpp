struct Fixed {};
struct Missing {};
auto choose(bool present) {
    if (present) {
        return Fixed{};
    }
    return Missing{};
}
int main() {
    auto value = choose(true);
    (void)value;
}
