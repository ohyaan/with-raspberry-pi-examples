class Config {
    explicit Config(unsigned period) : period_(period) {}
    unsigned period_;
};
int main() {
    Config invalid(0);
}
