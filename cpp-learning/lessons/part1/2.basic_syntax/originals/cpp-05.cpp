const int active_pin = 22;
auto pin_copy = active_pin; // Type is "int" (const is dropped!)

const auto& pin_ref = active_pin; // Type is "const int&" (explicitly preserved)
