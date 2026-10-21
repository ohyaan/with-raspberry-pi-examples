void updateValue(int val) {
    val = 100; // Changes local copy only
}

int main() {
    int num = 5;
    updateValue(num); // num remains 5
}
