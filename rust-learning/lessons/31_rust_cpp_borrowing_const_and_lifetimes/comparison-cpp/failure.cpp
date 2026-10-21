int main() {
    int reading = 46700;
    const int& observed = reading;
    observed = -500;
}
