template <typename T>
class Wrapper {
    T data;
public:
    Wrapper(T val) : data(val) {}
    T get_value() const { return data; }
};
