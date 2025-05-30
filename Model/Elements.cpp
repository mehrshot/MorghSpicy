//
// Created by Mehrshad on 5/29/2025.
//

// Model/Elements.cpp
#include "Elements.h" // Include its own header

#include <iostream> // For std::cerr in get_matrix_idx

// Helper function to get the 0-based matrix index from a Node ID.
// Ground node (ID=0) does not have a matrix index.
int get_matrix_idx(int node_id, const std::map<int, int>& node_id_to_matrix_idx) {
    if (node_id == 0) return -1; // Ground node (GND) does not have a matrix index.
    if (node_id_to_matrix_idx.count(node_id)) {
        return node_id_to_matrix_idx.at(node_id);
    }
    // If a node is not found in the map, it's a logical error in circuit definition or indexing.
    std::cerr << "Warning: Node ID " << node_id << " not found in matrix indexing map." << std::endl;
    return -1;
}

void Resistor::stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                        const std::map<int, int>& node_id_to_matrix_idx,
                        int extra_var_start_idx,
                        const Eigen::VectorXd& prev_solution,
                        double h) {
    double conductance = 1.0 / value; // 'value' is resistance here

    int n1_idx = get_matrix_idx(node1, node_id_to_matrix_idx);
    int n2_idx = get_matrix_idx(node2, node_id_to_matrix_idx);

    // Stamp self-terms (diagonal)
    if (n1_idx != -1) A(n1_idx, n1_idx) += conductance;
    if (n2_idx != -1) A(n2_idx, n2_idx) += conductance;

    // Stamp mutual-terms (off-diagonal)
    if (n1_idx != -1 && n2_idx != -1) {
        A(n1_idx, n2_idx) -= conductance;
        A(n2_idx, n1_idx) -= conductance;
    }
    // Resistors do not contribute to the 'b' vector directly in basic MNA.
}

void Capacitor::stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                         const std::map<int, int>& node_id_to_matrix_idx,
                         int extra_var_start_idx,
                         const Eigen::VectorXd& prev_solution,
                         double h) {
    // Backward Euler method for capacitor: i_C(n+1) = C/h * (V_C(n+1) - V_C(n))
    // Equivalent model: a resistor (h/C) in parallel with a current source (C/h * V_C(n)).
    double conductance_eq = value / h; // 'value' is capacitance here

    int n1_idx = get_matrix_idx(node1, node_id_to_matrix_idx);
    int n2_idx = get_matrix_idx(node2, node_id_to_matrix_idx);

    // Get previous node voltages (V_n1(n) and V_n2(n))
    // If a node is ground (ID=0) or its index is out of bounds for prev_solution, its voltage is 0.
    double prev_V_n1 = (n1_idx != -1 && n1_idx < prev_solution.size()) ? prev_solution(n1_idx) : 0.0;
    double prev_V_n2 = (n2_idx != -1 && n2_idx < prev_solution.size()) ? prev_solution(n2_idx) : 0.0;

    // Stamp A matrix (similar to a resistor with equivalent conductance)
    if (n1_idx != -1) A(n1_idx, n1_idx) += conductance_eq;
    if (n2_idx != -1) A(n2_idx, n2_idx) += conductance_eq;
    if (n1_idx != -1 && n2_idx != -1) {
        A(n1_idx, n2_idx) -= conductance_eq;
        A(n2_idx, n1_idx) -= conductance_eq;
    }

    // Stamp b vector (equivalent current source contribution)
    // Current flows from node1 to node2, so it contributes positively to node1 KCL
    // and negatively to node2 KCL.
    // The equivalent current source value is C/h * (V_n1(n) - V_n2(n)) flowing from n1 to n2.
    if (n1_idx != -1) {
        b(n1_idx) += conductance_eq * (prev_V_n1 - prev_V_n2);
    }
    if (n2_idx != -1) {
        b(n2_idx) += conductance_eq * (prev_V_n2 - prev_V_n1);
    }
}

void Inductor::stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                        const std::map<int, int>& node_id_to_matrix_idx,
                        int extra_var_start_idx,
                        const Eigen::VectorXd& prev_solution,
                        double h) {
    // Inductor: V_L = L * dI_L/dt
    // Backward Euler: V_L(n+1) = L/h * (I_L(n+1) - I_L(n))
    // So, V(n1) - V(n2) = L/h * I_L(n+1) - L/h * I_L(n)
    // Rearranging for MNA:
    // Equation for KCL at node n1: ... + I_L(n+1) = ...
    // Equation for KCL at node n2: ... - I_L(n+1) = ...
    // New equation for inductor: (V(n1) - V(n2)) - (L/h) * I_L(n+1) = - (L/h) * I_L(n)

    int n1_idx = get_matrix_idx(node1, node_id_to_matrix_idx);
    int n2_idx = get_matrix_idx(node2, node_id_to_matrix_idx);

    // Get the matrix index for the extra variable (inductor current)
    int current_var_idx = extra_var_start_idx + extraVariableIndex;

    // Stamp KCL for nodes connected to inductor (current I_L flows from n1 to n2)
    if (n1_idx != -1) A(n1_idx, current_var_idx) += 1.0;
    if (n2_idx != -1) A(n2_idx, current_var_idx) -= 1.0;

    // Stamp KVL equation for the inductor (new row in MNA matrix)
    if (n1_idx != -1) A(current_var_idx, n1_idx) += 1.0;
    if (n2_idx != -1) A(current_var_idx, n2_idx) -= 1.0;
    A(current_var_idx, current_var_idx) -= (value / h); // Coefficient for I_L(n+1) ('value' is inductance)

    // Right-hand side for inductor KVL (from previous timestep)
    // Ensure index is valid for prev_solution vector.
    double prev_I_L = (current_var_idx < prev_solution.size()) ? prev_solution(current_var_idx) : 0.0;
    b(current_var_idx) -= (value / h) * prev_I_L; // - (L/h) * I_L(n)
}

void VoltageSource::stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                             const std::map<int, int>& node_id_to_matrix_idx,
                             int extra_var_start_idx,
                             const Eigen::VectorXd& prev_solution,
                             double h) {
    // Voltage Source: V(n1) - V(n2) = Value (Voltage constraint equation)
    // Its current (I_VS) is added as an extra unknown variable.

    int n1_idx = get_matrix_idx(node1, node_id_to_matrix_idx);
    int n2_idx = get_matrix_idx(node2, node_id_to_matrix_idx);

    // Get the matrix index for the extra variable (voltage source current)
    int current_var_idx = extra_var_start_idx + extraVariableIndex;

    // Stamp KCL for nodes connected to the voltage source (current I_VS flows from n1 to n2)
    if (n1_idx != -1) A(n1_idx, current_var_idx) += 1.0;
    if (n2_idx != -1) A(n2_idx, current_var_idx) -= 1.0;

    // Stamp voltage constraint (new row in MNA matrix)
    if (n1_idx != -1) A(current_var_idx, n1_idx) += 1.0;
    if (n2_idx != -1) A(current_var_idx, n2_idx) -= 1.0;
    // The value of the voltage source goes on the RHS.
    b(current_var_idx) += value; // 'value' is voltage of the source
}

void CurrentSource::stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                             const std::map<int, int>& node_id_to_matrix_idx,
                             int extra_var_start_idx,
                             const Eigen::VectorXd& prev_solution,
                             double h) {
    // Current source contributes directly to the 'b' vector (RHS of KCL equations at nodes).
    // Current flows from node1 to node2.

    int n1_idx = get_matrix_idx(node1, node_id_to_matrix_idx);
    int n2_idx = get_matrix_idx(node2, node_id_to_matrix_idx);

    // Current leaves node1 (negative contribution to node1's KCL equation)
    if (n1_idx != -1) b(n1_idx) -= value; // 'value' is current of the source
    // Current enters node2 (positive contribution to node2's KCL equation)
    if (n2_idx != -1) b(n2_idx) += value;
}