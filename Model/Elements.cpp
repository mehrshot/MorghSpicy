//
// Created by Mehrshad on 5/29/2025.
//

#include "Elements.h"
#include <cmath> // Required for std::exp used in Diode model
#include <iostream>

// Helper function to get the matrix index for a given node ID.
// Returns -1 for ground node.
int get_matrix_idx(int node_id, const std::map<int, int>& node_id_to_matrix_idx) {
    if (node_id == 0) return -1;
    if (node_id_to_matrix_idx.count(node_id)) {
        return node_id_to_matrix_idx.at(node_id);
    }
    std::cerr << "Warning: Node ID " << node_id << " not found in matrix indexing map." << std::endl;
    return -1;
}

void Resistor::stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                        const std::map<int, int>& node_id_to_matrix_idx,
                        int extra_var_start_idx,
                        const Eigen::VectorXd& prev_solution,
                        double h) {
    if (value <= 0) {
        std::cerr << "Error: Resistor '" << name << "' has a non-positive value (" << value << "). Skipping stamp." << std::endl;
        return;
    }
    double conductance = 1.0 / value;

    int n1_idx = get_matrix_idx(node1, node_id_to_matrix_idx);
    int n2_idx = get_matrix_idx(node2, node_id_to_matrix_idx);

    if (n1_idx != -1) A(n1_idx, n1_idx) += conductance;
    if (n2_idx != -1) A(n2_idx, n2_idx) += conductance;

    if (n1_idx != -1 && n2_idx != -1) {
        A(n1_idx, n2_idx) -= conductance;
        A(n2_idx, n1_idx) -= conductance;
    }
}

void Capacitor::stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                         const std::map<int, int>& node_id_to_matrix_idx,
                         int extra_var_start_idx,
                         const Eigen::VectorXd& prev_solution,
                         double h) {
    if (value <= 0) {
        std::cerr << "Error: Capacitor '" << name << "' has a non-positive value (" << value << "). Skipping stamp." << std::endl;
        return;
    }
    if (h <= 0) {
        std::cerr << "Error: Invalid timestep h (" << h << ") for Capacitor '" << name << "'. Skipping stamp." << std::endl;
        return;
    }
    double conductance_eq = value / h;

    int n1_idx = get_matrix_idx(node1, node_id_to_matrix_idx);
    int n2_idx = get_matrix_idx(node2, node_id_to_matrix_idx);

    double prev_V_n1 = (n1_idx != -1 && n1_idx < prev_solution.size()) ? prev_solution(n1_idx) : 0.0;
    double prev_V_n2 = (n2_idx != -1 && n2_idx < prev_solution.size()) ? prev_solution(n2_idx) : 0.0;

    if (n1_idx != -1) A(n1_idx, n1_idx) += conductance_eq;
    if (n2_idx != -1) A(n2_idx, n2_idx) += conductance_eq;
    if (n1_idx != -1 && n2_idx != -1) {
        A(n1_idx, n2_idx) -= conductance_eq;
        A(n2_idx, n1_idx) -= conductance_eq;
    }

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
    if (value <= 0) {
        std::cerr << "Error: Inductor '" << name << "' has a non-positive value (" << value << "). Skipping stamp." << std::endl;
        return;
    }
    if (h <= 0) {
        std::cerr << "Error: Invalid timestep h (" << h << ") for Inductor '" << name << "'. Skipping stamp." << std::endl;
        return;
    }

    int n1_idx = get_matrix_idx(node1, node_id_to_matrix_idx);
    int n2_idx = get_matrix_idx(node2, node_id_to_matrix_idx);

    int current_var_idx = extra_var_start_idx + extraVariableIndex;

    if (n1_idx != -1) A(n1_idx, current_var_idx) += 1.0;
    if (n2_idx != -1) A(n2_idx, current_var_idx) -= 1.0;

    // Add checks to prevent accessing matrix with index -1
    if (n1_idx != -1) A(current_var_idx, n1_idx) += 1.0;
    if (n2_idx != -1) A(current_var_idx, n2_idx) -= 1.0;

    A(current_var_idx, current_var_idx) -= (value / h);

    double prev_I_L = (current_var_idx < prev_solution.size()) ? prev_solution(current_var_idx) : 0.0;
    b(current_var_idx) -= (value / h) * prev_I_L;
}

void VoltageSource::stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                             const std::map<int, int>& node_id_to_matrix_idx,
                             int extra_var_start_idx,
                             const Eigen::VectorXd& prev_solution,
                             double h) {
    int n1_idx = get_matrix_idx(node1, node_id_to_matrix_idx);
    int n2_idx = get_matrix_idx(node2, node_id_to_matrix_idx);

    int current_var_idx = extra_var_start_idx + extraVariableIndex;

    if (n1_idx != -1) A(n1_idx, current_var_idx) += 1.0;
    if (n2_idx != -1) A(n2_idx, current_var_idx) -= 1.0;

    // Add checks to prevent accessing matrix with index -1
    if (n1_idx != -1) A(current_var_idx, n1_idx) += 1.0;
    if (n2_idx != -1) A(current_var_idx, n2_idx) -= 1.0;

    b(current_var_idx) += value;
}

void CurrentSource::stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                             const std::map<int, int>& node_id_to_matrix_idx,
                             int extra_var_start_idx,
                             const Eigen::VectorXd& prev_solution,
                             double h) {
    int n1_idx = get_matrix_idx(node1, node_id_to_matrix_idx);
    int n2_idx = get_matrix_idx(node2, node_id_to_matrix_idx);

    if (n1_idx != -1) b(n1_idx) -= value;
    if (n2_idx != -1) b(n2_idx) += value;
}
void Diode::stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                     const std::map<int, int>& node_id_to_matrix_idx,
                     int extra_var_start_idx,
                     const Eigen::VectorXd& current_guess,
                     double h) {

    int n1_idx = get_matrix_idx(node1, node_id_to_matrix_idx);
    int n2_idx = get_matrix_idx(node2, node_id_to_matrix_idx);

    double v1_guess = (n1_idx == -1) ? 0.0 : current_guess(n1_idx);
    double v2_guess = (n2_idx == -1) ? 0.0 : current_guess(n2_idx);
    double vd_guess = v1_guess - v2_guess;

    // --- Diode Limiting for Numerical Stability ---
    const double max_vd_forward = 0.85;
    if (vd_guess > max_vd_forward) {
        vd_guess = max_vd_forward;
    }
    // --- End of Limiting Logic ---

    double Geq, Ieq;

    if (model == "Z" && vd_guess < -Vz) {
        Geq = 1.0;
        Ieq = Geq * Vz;
    } else {
        double exp_val = std::exp(vd_guess / (n * Vt));
        double id_val = Is * (exp_val - 1.0);
        Geq = (Is / (n * Vt)) * exp_val;
        Ieq = id_val - Geq * vd_guess;
    }

    // Stamping the equivalent circuit
    if (n1_idx != -1) A(n1_idx, n1_idx) += Geq;
    if (n2_idx != -1) A(n2_idx, n2_idx) += Geq;
    if (n1_idx != -1 && n2_idx != -1) {
        A(n1_idx, n2_idx) -= Geq;
        A(n2_idx, n1_idx) -= Geq;
    }

    if (n1_idx != -1) b(n1_idx) -= Ieq;
    if (n2_idx != -1) b(n2_idx) += Ieq;
}
void vccs::stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                    const std::map<int, int>& node_id_to_matrix_idx,
                    int extra_var_start_idx,
                    const Eigen::VectorXd& prev_solution,
                    double h) {
    int n1 = get_matrix_idx(node1, node_id_to_matrix_idx);
    int n2 = get_matrix_idx(node2, node_id_to_matrix_idx);
    int c1 = get_matrix_idx(ctrl_node1, node_id_to_matrix_idx);
    int c2 = get_matrix_idx(ctrl_node2, node_id_to_matrix_idx);

    if (n1 != -1 && c1 != -1) A(n1, c1) += value;
    if (n1 != -1 && c2 != -1) A(n1, c2) -= value;
    if (n2 != -1 && c1 != -1) A(n2, c1) -= value;
    if (n2 != -1 && c2 != -1) A(n2, c2) += value;
}


void vcvs::stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                    const std::map<int, int>& node_id_to_matrix_idx,
                    int extra_var_start_idx,
                    const Eigen::VectorXd& prev_solution,
                    double h) {
    int n1 = get_matrix_idx(node1, node_id_to_matrix_idx);
    int n2 = get_matrix_idx(node2, node_id_to_matrix_idx);
    int c1 = get_matrix_idx(ctrl_node1, node_id_to_matrix_idx);
    int c2 = get_matrix_idx(ctrl_node2, node_id_to_matrix_idx);
    int idx = extra_var_start_idx + extraVariableIndex;

    if (n1 != -1) A(n1, idx) += 1;
    if (n2 != -1) A(n2, idx) -= 1;

    if (idx != -1 && n1 != -1) A(idx, n1) += 1;
    if (idx != -1 && n2 != -1) A(idx, n2) -= 1;
    if (idx != -1 && c1 != -1) A(idx, c1) -= value;
    if (idx != -1 && c2 != -1) A(idx, c2) += value;
}


void cccs::stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                    const std::map<int, int>& node_id_to_matrix_idx,
                    int extra_var_start_idx,
                    const Eigen::VectorXd& prev_solution,
                    double h) {
    if (!controlling_elem) return;
    int ctrl_idx = extra_var_start_idx + controlling_elem->extraVariableIndex;

    int n1 = get_matrix_idx(node1, node_id_to_matrix_idx);
    int n2 = get_matrix_idx(node2, node_id_to_matrix_idx);

    if (n1 != -1) A(n1, ctrl_idx) += value;
    if (n2 != -1) A(n2, ctrl_idx) -= value;
}

void cccs::linkControlSource(const std::vector<Element*>& all) {
    for (Element* e : all) {
        if (e->name == controlling_name) {
            controlling_elem = e;
            std::cerr << "[DEBUG] CCCS " << name << " linked to control source: " << controlling_name << std::endl;
            return;
        }
    }
    std::cerr << "Warning: CCCS " << name << " could not find controlling source: " << controlling_name << std::endl;
}





void ccvs::stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                    const std::map<int, int>& node_id_to_matrix_idx,
                    int extra_var_start_idx,
                    const Eigen::VectorXd& prev_solution,
                    double h) {
    if (!controlling_elem) return;
    int idx = extra_var_start_idx + extraVariableIndex;
    int ctrl_idx = extra_var_start_idx + controlling_elem->extraVariableIndex;

    int n1 = get_matrix_idx(node1, node_id_to_matrix_idx);
    int n2 = get_matrix_idx(node2, node_id_to_matrix_idx);

    if (n1 != -1) A(n1, idx) += 1;
    if (n2 != -1) A(n2, idx) -= 1;

    if (idx != -1 && n1 != -1) A(idx, n1) += 1;
    if (idx != -1 && n2 != -1) A(idx, n2) -= 1;
    if (idx != -1 && ctrl_idx != -1) A(idx, ctrl_idx) -= value;
}

void ccvs::linkControlSource(const std::vector<Element*>& all) {
    for (Element* e : all) {
        if (e->name == controlling_name) {
            controlling_elem = e;
            return;
        }
    }
    std::cerr << "Warning: CCVS " << name << " could not find controlling source: " << controlling_name << std::endl;
}






void Resistor::display() {
    std::cout << "Resistor " << name << ": " << value << " Ohms, Nodes: " << node1 << " - " << node2 << std::endl;
}

void Capacitor::display() {
    std::cout << "Capacitor " << name << ": " << value << " F, Nodes: " << node1 << " - " << node2 << std::endl;
}

void Inductor::display() {
    std::cout << "Inductor " << name << ": " << value << " H, Nodes: " << node1 << " - " << node2 << std::endl;
}

void VoltageSource::display() {
    std::cout << "Voltage Source " << name << ": " << value << " V, Nodes: " << node1 << " - " << node2 << std::endl;
}

void CurrentSource::display() {
    std::cout << "Current Source " << name << ": " << value << " A, Nodes: " << node1 << " - " << node2 << std::endl;
}

void Diode::display() {
    std::cout << "Diode " << name << ": Model = " << model
              << ", Nodes: " << node1 << " - " << node2 << std::endl;
}
void PulseSource::display() {
    std::cout << "Pulse Source " << name << ": "
              << "V1=" << v1 << "V, V2=" << v2 << "V, "
              << "TD=" << td << "s, PW=" << pw << "s, PER=" << per << "s, "
              << "Nodes: " << node1 << "-" << node2 << std::endl;
}

void PulseSource::stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b,
                           const std::map<int, int>& node_id_to_matrix_idx,
                           int extra_var_start_idx,
                           const Eigen::VectorXd& prev_solution,
                           double h) {
    int extra_index = extra_var_start_idx + extraVariableIndex;
    int idx1 = get_matrix_idx(node1, node_id_to_matrix_idx);
    int idx2 = get_matrix_idx(node2, node_id_to_matrix_idx);

    if (idx1 != -1) A(idx1, extra_index) += 1.0;
    if (idx2 != -1) A(idx2, extra_index) -= 1.0;

    if (idx1 != -1) A(extra_index, idx1) += 1.0;
    if (idx2 != -1) A(extra_index, idx2) -= 1.0;

    b(extra_index) += getInstantaneousValue();
}

double PulseSource::getInstantaneousValue() const {
    // Before the delay time, the voltage is at its initial value
    if (time <= td) {
        return v1;
    }

    // After the delay, calculate time within the current period
    double time_in_period = fmod(time - td, per);

    // During the rise time
    if (time_in_period < tr) {
        return v1 + (v2 - v1) * (time_in_period / tr);
    }
        // During the pulse width (high plateau)
    else if (time_in_period < tr + pw) {
        return v2;
    }
        // During the fall time
    else if (time_in_period < tr + pw + tf) {
        return v2 - (v2 - v1) * ((time_in_period - (tr + pw)) / tf);
    }
        // After fall time until the next period (low plateau)
    else {
        return v1;
    }
}