#ifndef MORGHSPICY_ELEMENTS_H
#define MORGHSPICY_ELEMENTS_H

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <eigen3/Eigen/Dense>
#include "ElementTypes.h"
#include <bits/stdc++.h>
#include <cmath>
#include<bits/stdc++.h>

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

    // Pure virtual function for stamping the element's contribution to the MNA matrices
    virtual void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                          const std::map<int, int>& node_id_to_matrix_idx,
                          int extra_var_start_idx,
                          const Eigen::VectorXd& prev_solution,
                          double h) = 0;

    virtual void display() = 0;
};

class Resistor : public Element {
public:
    Resistor(std::string n, int n1, int n2, double v) : Element(n, n1, n2, v, RESISTOR) {}
    void display() override;
    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const std::map<int, int>& node_id_to_matrix_idx, int extra_var_start_idx, const Eigen::VectorXd& prev_solution, double h) override;
};

class Capacitor : public Element {
public:
    Capacitor(std::string n, int n1, int n2, double v) : Element(n, n1, n2, v, CAPACITOR) {}
    void display() override;
    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const std::map<int, int>& node_id_to_matrix_idx, int extra_var_start_idx, const Eigen::VectorXd& prev_solution, double h) override;
};

class Inductor : public Element {
public:
    Inductor(std::string n, int n1, int n2, double v) : Element(n, n1, n2, v, INDUCTOR) {
        introducesExtraVariable = true;
    }
    void display() override;
    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const std::map<int, int>& node_id_to_matrix_idx, int extra_var_start_idx, const Eigen::VectorXd& prev_solution, double h) override;
};

class VoltageSource : public Element {
public:
    VoltageSource(std::string n, int n1, int n2, double v) : Element(n, n1, n2, v, VOLTAGE_SOURCE) {
        introducesExtraVariable = true;
    }
    void display() override;
    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const std::map<int, int>& node_id_to_matrix_idx, int extra_var_start_idx, const Eigen::VectorXd& prev_solution, double h) override;
};

class CurrentSource : public Element {
public:
    CurrentSource(std::string n, int n1, int n2, double v) : Element(n, n1, n2, v, CURRENT_SOURCE) {}
    void display() override;
    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const std::map<int, int>& node_id_to_matrix_idx, int extra_var_start_idx, const Eigen::VectorXd& prev_solution, double h) override;
};

class Diode : public Element {
public:
    std::string model;

    // --- Model Parameters ---
    double Is; // Saturation Current
    double n;  // Ideality Factor
    double Vt; // Thermal Voltage

    // --- Zener Model Parameters ---
    double Vz; // Zener Breakdown Voltage
    double Iz; // Zener Current at Vz

    Diode(std::string name, int n1, int n2, std::string m)
            : Element(name, n1, n2, 0.0, DIODE), model(m) {
        // Initialize model parameters with common default values
        Is = 1e-14;
        n = 1.0;
        Vt = 0.02585; // at room temperature

        // Default Zener voltage
        Vz = 5.1;
        Iz = 0.0;
    }

    void display() override;
    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                  const std::map<int, int>& node_id_to_matrix_idx,
                  int extra_var_start_idx,
                  const Eigen::VectorXd& prev_solution,
                  double h) override;
};

// dependent sources // بخدا خودم کامنت گذاشتم
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

    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                  const std::map<int, int>& node_id_to_matrix_idx,
                  int extra_var_start_idx,
                  const Eigen::VectorXd& prev_solution,
                  double h) override;
};

class vcvs : public Element {
    int ctrl_node1, ctrl_node2;
public:
    vcvs(std::string n, int n1, int n2, int c1, int c2, double gain)
        : Element(n, n1, n2, gain,ElementType:: VCVS), ctrl_node1(c1), ctrl_node2(c2) {
        introducesExtraVariable = true;
    }

    void display() override {
        std::cout << "VCVS " << name << ": Gain = " << value
                  << ", Output: " << node1 << "-" << node2
                  << ", Control: " << ctrl_node1 << "-" << ctrl_node2 << std::endl;
    }

    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                  const std::map<int, int>& node_id_to_matrix_idx,
                  int extra_var_start_idx,
                  const Eigen::VectorXd& prev_solution,
                  double h) override;
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

    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                  const std::map<int, int>& node_id_to_matrix_idx,
                  int extra_var_start_idx,
                  const Eigen::VectorXd& prev_solution,
                  double h) override;

    void linkControlSource(const std::vector<Element*>& all_elements);
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

    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                  const std::map<int, int>& node_id_to_matrix_idx,
                  int extra_var_start_idx,
                  const Eigen::VectorXd& prev_solution,
                  double h) override;

    void linkControlSource(const std::vector<Element*>& all_elements);
};


class SinusoidalSource : public Element {
private:
    double Voffset;
    double Vamplitude;
    double frequency;
    double phase;
    double time;

public:
    SinusoidalSource(std::string n, int n1, int n2, double voffset, double vamplitude, double freq, double ph = 0.0)
        : Element(n, n1, n2, 0.0, SINUSOIDAL_SOURCE),
          Voffset(voffset), Vamplitude(vamplitude), frequency(freq), phase(ph), time(0.0) {
        introducesExtraVariable = true;
    }

    void updateTime(double newTime) { time = newTime; }

    double getInstantaneousValue() const {
        return Voffset + Vamplitude * sin(2 * std::numbers::pi * frequency * time + phase);
    }

    void display() override {
        std::cout << "Sinusoidal Source " << name << ": "
                  << "Voffset=" << Voffset << "V, "
                  << "Amplitude=" << Vamplitude << "V, "
                  << "Frequency=" << frequency << "Hz, "
                  << "Nodes: " << node1 << "-" << node2 << std::endl;
    }

    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                  const std::map<int, int>& node_id_to_matrix_idx,
                  int extra_var_start_idx,
                  const Eigen::VectorXd& prev_solution,
                  double h) override {
        int extra_index = extra_var_start_idx + extraVariableIndex;
        int row1 = node_id_to_matrix_idx.count(node1) ? node_id_to_matrix_idx.at(node1) : -1;
        int row2 = node_id_to_matrix_idx.count(node2) ? node_id_to_matrix_idx.at(node2) : -1;

        if (row1 != -1) {
            A(row1, extra_index) += 1;
            A(extra_index, row1) += 1;
        }
        if (row2 != -1) {
            A(row2, extra_index) -= 1;
            A(extra_index, row2) -= 1;
        }

        b(extra_index) += getInstantaneousValue();
    }
};


#endif //MORGHSPICY_ELEMENTS_H