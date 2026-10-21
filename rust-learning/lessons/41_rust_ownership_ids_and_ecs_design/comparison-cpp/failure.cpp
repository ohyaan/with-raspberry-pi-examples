class World {
public:
    World() = default;
    World(const World&) = delete;
};
int main() {
    World first;
    World duplicate = first;
}
