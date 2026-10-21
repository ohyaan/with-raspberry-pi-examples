#include <iostream>

class Animal { public: virtual ~Animal() = default; };
class Dog : public Animal { public: void bark() {} };
class Cat : public Animal { public: void meow() {} };

void play_with_animal(Animal* animal) {
    // Safely verify if the Animal is actually a Dog
    if (Dog* dog = dynamic_cast<Dog*>(animal)) {
        dog->bark();
    } else {
        std::cout << "Not a dog!\n";
    }
}
