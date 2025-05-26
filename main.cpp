#include <iostream>
#include <string>
#include <vector>
using namespace std;

class Element {
public:
    string name;
    int node1, node2;

    Element(string n, int n1, int n2) : name(n), node1(n1), node2(n2) {}

    virtual void display() = 0;
};

class Resistor : public Element {
public:
    double value;

    Resistor(string n, int n1, int n2, double v) : Element(n, n1, n2), value(v) {}

    void display() override {
        cout << "Resistor " << name << ": " << value << " Ohms, Nodes: " << node1 << " - " << node2 << endl;
    }
};

class Capacitor : public Element {
public:
    double value;

    Capacitor(string n, int n1, int n2, double v) : Element(n, n1, n2), value(v) {}

    void display() override {
        cout << "Capacitor " << name << ": " << value << " F, Nodes: " << node1 << " - " << node2 << endl;
    }
};

class Inductor : public Element {
public:
    double value;

    Inductor(string n, int n1, int n2, double v) : Element(n, n1, n2), value(v) {}

    void display() override {
        cout << "Inductor " << name << ": " << value << " H, Nodes: " << node1 << " - " << node2 << endl;
    }
};

class VoltageSource : public Element {
public:
    double value;

    VoltageSource(string n, int n1, int n2, double v) : Element(n, n1, n2), value(v) {}

    void display() override {
        cout << "Voltage Source " << name << ": " << value << " V, Nodes: " << node1 << " - " << node2 << endl;
    }
};

class Circuit {
private:
    vector<Element*> elements;

public:
    void addElement(Element* e) {
        elements.push_back(e);
    }

    void displayElements() {
        for (auto e : elements) {
            e->display();
        }
    }
};

int main() {
    Circuit circuit;

    circuit.addElement(new Resistor("R1", 1, 2, 1000));
    circuit.addElement(new Capacitor("C1", 2, 0, 1e-6));
    circuit.addElement(new Inductor("L1", 1, 0, 1e-3));
    circuit.addElement(new VoltageSource("V1", 1, 0, 5));

    circuit.displayElements();

    return 0;
}
