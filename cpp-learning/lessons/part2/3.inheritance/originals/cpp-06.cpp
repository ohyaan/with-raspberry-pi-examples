class Controller final { /* Cannot be inherited */ };

class Base {
    virtual void setup();
};

class Intermediate : public Base {
    void setup() override final; // Child classes cannot override this
};
