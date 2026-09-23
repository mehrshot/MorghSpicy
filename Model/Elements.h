#ifndef MORGHSPICY_ELEMENTS_H
#define MORGHSPICY_ELEMENTS_H

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <cmath>
#include <numbers>
#include <Eigen/Dense>
#include "ElementTypes.h"

// everything an element needs to stamp itself into the MNA system
struct StampInfo {
    const std::map<int, int>& nodeIdx;   // node id -> row in the matrix (ground is not in here)
    int extraStart;                      // first row of the extra (current) variables
    const Eigen::VectorXd& prev;         // solution at the previous time point
    const Eigen::VectorXd& guess;        // current newton-raphson guess (only diode uses it)
    double h;                            // time step
};

// Base class for all circuit elements
class Element {
public:
    std::string name;
    int node1, node2;
    double value;
    ElementType type;

    // Flag for elements that add a new unknown (current) to the MNA system
    bool introducesExtraVariable = false;
    // The index of this element's extra variable in the MNA matrix
    int extraVariableIndex = -1;

    Element(std::string n, int n1, int n2, double v, ElementType t)
            : name(n), node1(n1), node2(n2), value(v), type(t) {}
    virtual ~Element() = default;

    void setValue(double newValue) { value = newValue; }
    double getValue() const { return value; }

    virtual void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const StampInfo& info) = 0;
    virtual void display() = 0;

    virtual bool isNonlinear() const { return false; }
};

class Resistor : public Element {
public:
    Resistor(std::string n, int n1, int n2, double v) : Element(n, n1, n2, v, RESISTOR) {}
    void display() override;
    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const StampInfo& info) override;
};

class Capacitor : public Element {
public:
    Capacitor(std::string n, int n1, int n2, double v) : Element(n, n1, n2, v, CAPACITOR) {}
    void display() override;
    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const StampInfo& info) override;
};

class Inductor : public Element {
public:
    Inductor(std::string n, int n1, int n2, double v) : Element(n, n1, n2, v, INDUCTOR) {
        introducesExtraVariable = true;
    }
    void display() override;
    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const StampInfo& info) override;
};

class VoltageSource : public Element {
public:
    VoltageSource(std::string n, int n1, int n2, double v) : Element(n, n1, n2, v, VOLTAGE_SOURCE) {
        introducesExtraVariable = true;
    }
    void display() override;
    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const StampInfo& info) override;
};

class CurrentSource : public Element {
public:
    CurrentSource(std::string n, int n1, int n2, double v) : Element(n, n1, n2, v, CURRENT_SOURCE) {}
    void display() override;
    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const StampInfo& info) override;
};

class Diode : public Element {
public:
    std::string model;   // "D" = normal diode, "Z" = zener

    // model parameters
    double Is = 1e-14;    // saturation current
    double n = 1.0;       // ideality factor
    double Vt = 0.02585;  // thermal voltage at room temp

    // zener
    double Vz = 5.1;      // breakdown voltage
    double Gz = 1.0;      // conductance in breakdown

    Diode(std::string name, int n1, int n2, std::string m)
            : Element(name, n1, n2, 0.0, DIODE), model(m) {}

    double currentAt(double vd) const;

    void display() override;
    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const StampInfo& info) override;
    bool isNonlinear() const override { return true; }
};

// dependent sources
class vccs : public Element {
    int ctrl_node1, ctrl_node2;
public:
    vccs(std::string n, int n1, int n2, int c1, int c2, double gain)
        : Element(n, n1, n2, gain, ElementType::VCCS), ctrl_node1(c1), ctrl_node2(c2) {}

    void display() override {
        std::cout << "VCCS " << name << ": Gain = " << value
                  << ", Output: " << node1 << "-" << node2
                  << ", Control: " << ctrl_node1 << "-" << ctrl_node2 << std::endl;
    }

    int controlPos() const { return ctrl_node1; }
    int controlNeg() const { return ctrl_node2; }
    void setControlNodes(int c1, int c2) { ctrl_node1 = c1; ctrl_node2 = c2; }

    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const StampInfo& info) override;
};

class vcvs : public Element {
    int ctrl_node1, ctrl_node2;
public:
    vcvs(std::string n, int n1, int n2, int c1, int c2, double gain)
        : Element(n, n1, n2, gain, ElementType::VCVS), ctrl_node1(c1), ctrl_node2(c2) {
        introducesExtraVariable = true;
    }

    int controlPos() const { return ctrl_node1; }
    int controlNeg() const { return ctrl_node2; }
    void setControlNodes(int c1, int c2) { ctrl_node1 = c1; ctrl_node2 = c2; }

    void display() override {
        std::cout << "VCVS " << name << ": Gain = " << value
                  << ", Output: " << node1 << "-" << node2
                  << ", Control: " << ctrl_node1 << "-" << ctrl_node2 << std::endl;
    }

    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const StampInfo& info) override;
};

class cccs : public Element {
    std::string controlling_name;
    Element* controlling_elem = nullptr;
public:
    cccs(std::string n, int n1, int n2, std::string cname, double gain)
        : Element(n, n1, n2, gain, ElementType::CCCS), controlling_name(cname) {}

    void display() override {
        std::cout << "CCCS " << name << ": Gain = " << value
                  << ", Output: " << node1 << "-" << node2
                  << ", Control source: " << controlling_name << std::endl;
    }

    const std::string& controlName() const { return controlling_name; }
    Element* controlElement() const { return controlling_elem; }

    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const StampInfo& info) override;
    bool linkControlSource(const std::vector<Element*>& all_elements);
};

class ccvs : public Element {
    std::string controlling_name;
    Element* controlling_elem = nullptr;
public:
    ccvs(std::string n, int n1, int n2, std::string cname, double gain)
        : Element(n, n1, n2, gain, ElementType::CCVS), controlling_name(cname) {
        introducesExtraVariable = true;
    }

    void display() override {
        std::cout << "CCVS " << name << ": Gain = " << value
                  << ", Output: " << node1 << "-" << node2
                  << ", Control source: " << controlling_name << std::endl;
    }

    const std::string& controlName() const { return controlling_name; }
    Element* controlElement() const { return controlling_elem; }

    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const StampInfo& info) override;
    bool linkControlSource(const std::vector<Element*>& all_elements);
};

class SinusoidalSource : public Element {
private:
    double Voffset;
    double Vamplitude;
    double frequency;   // Hz
    double phase;       // degrees
    double time;

public:
    SinusoidalSource(std::string n, int n1, int n2, double voffset, double vamplitude, double freq, double ph = 0.0)
        : Element(n, n1, n2, 0.0, SINUSOIDAL_SOURCE),
          Voffset(voffset), Vamplitude(vamplitude), frequency(freq), phase(ph), time(0.0) {
        introducesExtraVariable = true;
    }

    void updateTime(double newTime) { time = newTime; }

    double getInstantaneousValue() const {
        double ph = phase * std::numbers::pi / 180.0;
        return Voffset + Vamplitude * std::sin(2 * std::numbers::pi * frequency * time + ph);
    }

    double offset() const { return Voffset; }
    double amplitude() const { return Vamplitude; }
    double freq() const { return frequency; }
    double phaseDeg() const { return phase; }

    void display() override {
        std::cout << "Sinusoidal Source " << name << ": "
                  << "Voffset=" << Voffset << "V, "
                  << "Amplitude=" << Vamplitude << "V, "
                  << "Frequency=" << frequency << "Hz, "
                  << "Phase=" << phase << "deg, "
                  << "Nodes: " << node1 << "-" << node2 << std::endl;
    }

    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const StampInfo& info) override;
};

class PulseSource : public Element {
public:
    double v1, v2, td, tr, tf, pw, per;
private:
    double time;

public:
    PulseSource(std::string n, int n1, int n2, double v_initial, double v_pulsed,
                double t_delay, double t_rise, double t_fall, double p_width, double period)
            : Element(n, n1, n2, v_initial, PULSE_SOURCE),
              v1(v_initial), v2(v_pulsed), td(t_delay), tr(t_rise), tf(t_fall), pw(p_width), per(period), time(0.0) {
        introducesExtraVariable = true;
    }

    void updateTime(double newTime) { time = newTime; }
    double getInstantaneousValue() const;

    void display() override;
    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const StampInfo& info) override;
};

#endif //MORGHSPICY_ELEMENTS_H
