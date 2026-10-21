int main() {
    int minimum = 0;
    bool (*predicate)(int) = [minimum](int reading) { return reading >= minimum; };
    (void)predicate;
}
