int* ptr1;           // Dangerous! Contains a random memory location.
int* ptr2 = nullptr; // Safe. Points to a defined null address.

if (ptr2 != nullptr) {
    std::cout << *ptr2; // Only dereference if pointer is valid
}
