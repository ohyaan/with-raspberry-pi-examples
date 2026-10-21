// Adapter activating the article's commented consteval failure.
consteval int force_compile_time_math(int x) { return x * x; }
int main(int argc, char**) { return force_compile_time_math(argc); }
