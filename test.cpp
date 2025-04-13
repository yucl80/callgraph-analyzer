#include <iostream>
#include <functional>
#include <future>

#define LOG(msg) std::cout << "LOG: " << msg << std::endl
#define MAX(a,b) ((a) > (b) ? (a) : (b))

class GrandBase {
public:
    virtual void virtualFunc() {
        std::cout << "GrandBase::virtualFunc" << std::endl;
    }
};

class Base : public GrandBase {
public:
    void virtualFunc() override {
        std::cout << "Base::virtualFunc" << std::endl;
    }
};

class Vector {
    double x, y;
    std::string name;
public:
    Vector(double x, double y) : x(x), y(y), name("Vector") {
        std::cout << "Constructing Vector" << std::endl;
    }
    ~Vector() {
        std::cout << "Destroying Vector" << std::endl;
    }
    
    Vector operator+(const Vector& other) const {
        return Vector(x + other.x, y + other.y);
    }
    
    friend std::ostream& operator<<(std::ostream& os, const Vector& v) {
        return os << "(" << v.x << "," << v.y << ")";
    }
};

class Derived : public Base {
public:
    void virtualFunc() override {
        std::cout << "Derived::virtualFunc" << std::endl;
    }
    
    void newFunc() {
        std::cout << "Derived::newFunc" << std::endl;
    }
};

void normalFunction() {
    std::cout << "normalFunction" << std::endl;
}

void functionPointerTest(void (*func)()) {
    func();
}

int main() {
    // Virtual function call
    Base* obj = new Derived();
    obj->virtualFunc();

    // Function pointer call
    void (*funcPtr)() = &normalFunction;
    functionPointerTest(funcPtr);

    // Lambda expression call
    auto lambda = [](){
        std::cout << "lambda function" << std::endl;
    };
    lambda();

    // STL function call
    std::function<void()> stlFunc = [](){
        std::cout << "std::function" << std::endl;
    };
    stlFunc();

    // Template function test
    auto maxVal = std::max<int>(10, 20);
    std::cout << "Max value: " << maxVal << std::endl;

    // Class template test
    std::vector<std::string> vec;
    vec.push_back("template");
    vec.push_back("test");
    
    for (const auto& s : vec) {
        std::cout << s << " ";
    }
    std::cout << std::endl;

    // Macro tests
    LOG("Testing macro expansion");
    int x = MAX(10, 20);
    std::cout << "MAX result: " << x << std::endl;

    // Exception test
    try {
        Vector v1(1, 2);
        Vector v2(3, 4);
        Vector v3 = v1 + v2;
        throw std::runtime_error("Test exception");
    } catch (const std::exception& e) {
        std::cout << "Caught exception: " << e.what() << std::endl;
    }

    // Async test
    auto future = std::async(std::launch::async, [](){
        std::cout << "Async operation" << std::endl;
    });
    future.get();

    // Reflection test
    Base* basePtr = new Derived();
    Derived* derivedPtr = dynamic_cast<Derived*>(basePtr);
    if (derivedPtr) {
        std::cout << "Dynamic cast successful" << std::endl;
    }
    std::cout << "Type: " << typeid(*basePtr).name() << std::endl;

    delete obj;
    delete basePtr;
    return 0;
}
