#ifndef MORGHSPICY_ELEMENTS_H
#define MORGHSPICY_ELEMENTS_H

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <eigen3/Eigen/Dense>
#include "ElementTypes.h"

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

#endif //MORGHSPICY_ELEMENTS_H