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
    virtual ~Element() = default; // Virtual destructor for proper memory cleanup of derived classes

    // New: Pure virtual method for stamping the element's contribution into the MNA matrix
    // A: The MNA matrix where the element's contributions are added.
    // b: The right-hand side vector of equations.
    // node_id_to_matrix_idx: Map from actual Node ID to its 0-based matrix index.
    // extra_var_start_idx: The starting index for extra variables in the matrix (after node voltages).
    // prev_solution: The solution vector from the previous timestep (for transient elements like C, L).
    // h: The timestep (required for transient analysis, i.e., C and L).
    virtual void stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                          const std::map<int, int>& node_id_to_matrix_idx,
                          int extra_var_start_idx,
                          const Eigen::VectorXd& prev_solution,
                          double h) = 0;

    // Virtual display method
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
                  double h) override; // Declaration of stampMNA
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
                  double h) override; // Declaration of stampMNA
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
                  double h) override; // Declaration of stampMNA
};

#endif // MORGHSPICY_ELEMENTS_H