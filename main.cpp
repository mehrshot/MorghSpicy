#include <iostream>
#include <string>
#include <vector>
#include "Model/ElementTypes.h"

using namespace std;
class Element {
public:
    string name;
    int node1, node2;
    double value;
    ElementType type;
    Element(string n, int n1, int n2,double v,ElementType t) : name(n), node1(n1), node2(n2), value(v) , type(t){}
    virtual void display() = 0;
};

class Resistor : public Element {
public:
    Resistor(string n, int n1, int n2, double v) : Element(n, n1, n2,v,RESISTOR){}

    void display() override {
        cout << "Resistor " << name << ": " << value << " Ohms, Nodes: " << node1 << " - " << node2 << endl;
        cout;
    }
};

class Capacitor : public Element {
public:
    Capacitor(string n, int n1, int n2, double v) : Element(n, n1, n2,v,CAPACITOR){}

    void display() override {
        cout << "Capacitor " << name << ": " << value << " F, Nodes: " << node1 << " - " << node2 << endl;
    }
};

class Inductor : public Element {
public:
    Inductor(string n, int n1, int n2, double v) : Element(n, n1, n2,v,INDUCTOR){}
    void display() override {
        cout << "Inductor " << name << ": " << value << " H, Nodes: " << node1 << " - " << node2 << endl;

    }
};

class VoltageSource : public Element {
public:
    VoltageSource(string n, int n1, int n2, double v) : Element(n, n1, n2,v,VOLTAGE_SOURCE){}


    void display() override {
        cout << "Voltage Source " << name << ": " << value << " V, Nodes: " << node1 << " - " << node2 << endl;
    }
};
class CurrentSource : public Element {
public:
    CurrentSource(string n, int n1, int n2, double v) : Element(n, n1, n2,v,CURRENT_SOURCE){}


    void display() override {
        cout << "Current Source " << name << ": " << value << " V, Nodes: " << node1 << " - " << node2 << endl;
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
