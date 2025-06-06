//
// Created by Mehrshad on 5/29/2025.
//

#ifndef MORGHSPICY_ELEMENTS_H
#define MORGHSPICY_ELEMENTS_H

#include <bits/stdc++.h>
#include <eigen3/Eigen/Dense>

#include "ElementTypes.h"

class Element {
public:
    std::string name;
    int node1, node2;
    double value;
    ElementType type;

    bool introducesExtraVariable = false;
    // The index of this extra variable in the extra variables section of the MNA matrix
    int extraVariableIndex = -1;

    // Constructor
    Element(std::string n, int n1, int n2, double v, ElementType t)
            : name(n), node1(n1), node2(n2), value(v), type(t) {}
    virtual ~Element() = default;

    void setValue(double newValue) {
        value = newValue;
    }
    double getValue() const {
        return value;
    }

    virtual void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                          const std::map<int, int>& node_id_to_matrix_idx,
                          int extra_var_start_idx,
                          const Eigen::VectorXd& prev_solution,
                          double h) = 0;

    virtual void display() = 0;
};

// Derived class for Resistor
class Resistor : public Element {
public:
    Resistor(std::string n, int n1, int n2, double v) : Element(n, n1, n2, v, RESISTOR) {}
    void display() override {
        std::cout << "Resistor " << name << ": " << value << " Ohms, Nodes: " << node1 << " - " << node2 << std::endl;
    }
    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                  const std::map<int, int>& node_id_to_matrix_idx,
                  int extra_var_start_idx,
                  const Eigen::VectorXd& prev_solution,
                  double h) override; // Declaration of stampMNA
};

// Derived class for Capacitor
class Capacitor : public Element {
public:
    Capacitor(std::string n, int n1, int n2, double v) : Element(n, n1, n2, v, CAPACITOR) {}
    void display() override {
        std::cout << "Capacitor " << name << ": " << value << " F, Nodes: " << node1 << " - " << node2 << std::endl;
    }
    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                  const std::map<int, int>& node_id_to_matrix_idx,
                  int extra_var_start_idx,
                  const Eigen::VectorXd& prev_solution,
                  double h) override; // Declaration of stampMNA
};

// Derived class for Inductor
class Inductor : public Element {
public:
    Inductor(std::string n, int n1, int n2, double v) : Element(n, n1, n2, v, INDUCTOR) {
        introducesExtraVariable = true; // Inductor current is an extra unknown in MNA
    }
    void display() override {
        std::cout << "Inductor " << name << ": " << value << " H, Nodes: " << node1 << " - " << node2 << std::endl;
    }
    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                  const std::map<int, int>& node_id_to_matrix_idx,
                  int extra_var_start_idx,
                  const Eigen::VectorXd& prev_solution,
                  double h) override;
};

// Derived class for VoltageSource
class VoltageSource : public Element {
public:
    VoltageSource(std::string n, int n1, int n2, double v) : Element(n, n1, n2, v, VOLTAGE_SOURCE) {
        introducesExtraVariable = true; // Voltage source current is an extra unknown in MNA
    }
    void display() override {
        std::cout << "Voltage Source " << name << ": " << value << " V, Nodes: " << node1 << " - " << node2 << std::endl;
    }
    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                  const std::map<int, int>& node_id_to_matrix_idx,
                  int extra_var_start_idx,
                  const Eigen::VectorXd& prev_solution,
                  double h) override;
};

// Derived class for CurrentSource
class CurrentSource : public Element {
public:
    CurrentSource(std::string n, int n1, int n2, double v) : Element(n, n1, n2, v, CURRENT_SOURCE) {}
    void display() override {
        std::cout << "Current Source " << name << ": " << value << " A, Nodes: " << node1 << " - " << node2 << std::endl;
    }
    void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                  const std::map<int, int>& node_id_to_matrix_idx,
                  int extra_var_start_idx,
                  const Eigen::VectorXd& prev_solution,
                  double h) override;
};

#endif // MORGHSPICY_ELEMENTS_H