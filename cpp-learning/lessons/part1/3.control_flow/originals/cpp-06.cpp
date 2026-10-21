int retries = 3;
while (retries > 0) {
    if (connect_to_server()) break;
    retries--;
}

char input;
do {
    std::cout << "Confirm exit (y/n): ";
    std::cin >> input;
} while (input != 'y' && input != 'n');
