struct Value {};
struct Missing {};
struct Visitor {
    virtual ~Visitor() = default;
    virtual void visit(const Value&) = 0;
    virtual void visit(const Missing&) = 0;
};
struct Incomplete : Visitor {
    void visit(const Value&) override {}
};
int main() {
    Incomplete operation;
}
