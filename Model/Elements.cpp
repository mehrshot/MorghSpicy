//
// Created by Mehrshad on 5/29/2025.
//

#include "Elements.h"
#include <cmath>
#include <iostream>

// matrix row of a node, -1 for ground (or unknown node)
static int get_matrix_idx(int node_id, const std::map<int, int>& node_id_to_matrix_idx) {
    if (node_id == 0) return -1;
    auto it = node_id_to_matrix_idx.find(node_id);
    if (it != node_id_to_matrix_idx.end()) return it->second;
    std::cerr << "Warning: Node ID " << node_id << " not found in matrix indexing map." << std::endl;
    return -1;
}

// conductance g between n1 and n2
static void stampConductance(Eigen::MatrixXd& A, int n1, int n2, double g) {
    if (n1 != -1) A(n1, n1) += g;
    if (n2 != -1) A(n2, n2) += g;
    if (n1 != -1 && n2 != -1) {
        A(n1, n2) -= g;
        A(n2, n1) -= g;
    }
}

// the +1/-1 pattern every "voltage source like" element has (V, L, E, H, SIN, PULSE)
static void stampBranch(Eigen::MatrixXd& A, int n1, int n2, int k) {
    if (n1 != -1) { A(n1, k) += 1.0; A(k, n1) += 1.0; }
    if (n2 != -1) { A(n2, k) -= 1.0; A(k, n2) -= 1.0; }
}

void Resistor::stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const StampInfo& info) {
    if (value <= 0) {
        std::cerr << "Error: Resistor '" << name << "' has a non-positive value (" << value << "). Skipping stamp." << std::endl;
        return;
    }
    int n1_idx = get_matrix_idx(node1, info.nodeIdx);
    int n2_idx = get_matrix_idx(node2, info.nodeIdx);
    stampConductance(A, n1_idx, n2_idx, 1.0 / value);
}

// backward euler: i = C/h * (v - v_prev)
void Capacitor::stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const StampInfo& info) {
    if (value <= 0 || info.h <= 0) {
        std::cerr << "Error: bad value or time step for Capacitor '" << name << "'. Skipping stamp." << std::endl;
        return;
    }
    double geq = value / info.h;

    int n1_idx = get_matrix_idx(node1, info.nodeIdx);
    int n2_idx = get_matrix_idx(node2, info.nodeIdx);

    double prev_v1 = (n1_idx != -1 && n1_idx < info.prev.size()) ? info.prev(n1_idx) : 0.0;
    double prev_v2 = (n2_idx != -1 && n2_idx < info.prev.size()) ? info.prev(n2_idx) : 0.0;

    stampConductance(A, n1_idx, n2_idx, geq);
    if (n1_idx != -1) b(n1_idx) += geq * (prev_v1 - prev_v2);
    if (n2_idx != -1) b(n2_idx) -= geq * (prev_v1 - prev_v2);
}

// backward euler: v = L/h * (i - i_prev)
void Inductor::stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const StampInfo& info) {
    if (value <= 0 || info.h <= 0) {
        std::cerr << "Error: bad value or time step for Inductor '" << name << "'. Skipping stamp." << std::endl;
        return;
    }
    int n1_idx = get_matrix_idx(node1, info.nodeIdx);
    int n2_idx = get_matrix_idx(node2, info.nodeIdx);
    int k = info.extraStart + extraVariableIndex;

    stampBranch(A, n1_idx, n2_idx, k);
    A(k, k) -= value / info.h;

    double prev_i = (k < info.prev.size()) ? info.prev(k) : 0.0;
    b(k) -= (value / info.h) * prev_i;
}

void VoltageSource::stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const StampInfo& info) {
    int n1_idx = get_matrix_idx(node1, info.nodeIdx);
    int n2_idx = get_matrix_idx(node2, info.nodeIdx);
    int k = info.extraStart + extraVariableIndex;

    stampBranch(A, n1_idx, n2_idx, k);
    b(k) += value;
}

void CurrentSource::stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const StampInfo& info) {
    int n1_idx = get_matrix_idx(node1, info.nodeIdx);
    int n2_idx = get_matrix_idx(node2, info.nodeIdx);

    // current flows from n1 through the source into n2
    if (n1_idx != -1) b(n1_idx) -= value;
    if (n2_idx != -1) b(n2_idx) += value;
}

double Diode::currentAt(double vd) const {
    if (model == "Z" && vd < -Vz) {
        return Gz * (vd + Vz);
    }
    return Is * (std::exp(vd / (n * Vt)) - 1.0);
}

// linearized around the current guess: i = Geq*vd + Ieq
void Diode::stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const StampInfo& info) {
    int n1_idx = get_matrix_idx(node1, info.nodeIdx);
    int n2_idx = get_matrix_idx(node2, info.nodeIdx);

    double v1 = (n1_idx == -1 || n1_idx >= info.guess.size()) ? 0.0 : info.guess(n1_idx);
    double v2 = (n2_idx == -1 || n2_idx >= info.guess.size()) ? 0.0 : info.guess(n2_idx);
    double vd = v1 - v2;

    // limit the forward voltage, otherwise exp() blows up in the first iterations
    const double max_vd_forward = 0.9;
    if (vd > max_vd_forward) vd = max_vd_forward;

    double Geq, Ieq;
    if (model == "Z" && vd < -Vz) {
        Geq = Gz;
        Ieq = Gz * Vz;
    } else {
        double exp_val = std::exp(vd / (n * Vt));
        Geq = (Is / (n * Vt)) * exp_val;
        Ieq = Is * (exp_val - 1.0) - Geq * vd;
    }

    stampConductance(A, n1_idx, n2_idx, Geq);
    if (n1_idx != -1) b(n1_idx) -= Ieq;
    if (n2_idx != -1) b(n2_idx) += Ieq;
}

void vccs::stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const StampInfo& info) {
    int n1 = get_matrix_idx(node1, info.nodeIdx);
    int n2 = get_matrix_idx(node2, info.nodeIdx);
    int c1 = get_matrix_idx(ctrl_node1, info.nodeIdx);
    int c2 = get_matrix_idx(ctrl_node2, info.nodeIdx);

    if (n1 != -1 && c1 != -1) A(n1, c1) += value;
    if (n1 != -1 && c2 != -1) A(n1, c2) -= value;
    if (n2 != -1 && c1 != -1) A(n2, c1) -= value;
    if (n2 != -1 && c2 != -1) A(n2, c2) += value;
}

void vcvs::stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const StampInfo& info) {
    int n1 = get_matrix_idx(node1, info.nodeIdx);
    int n2 = get_matrix_idx(node2, info.nodeIdx);
    int c1 = get_matrix_idx(ctrl_node1, info.nodeIdx);
    int c2 = get_matrix_idx(ctrl_node2, info.nodeIdx);
    int k = info.extraStart + extraVariableIndex;

    // v1 - v2 - gain*(vc1 - vc2) = 0
    stampBranch(A, n1, n2, k);
    if (c1 != -1) A(k, c1) -= value;
    if (c2 != -1) A(k, c2) += value;
}

void cccs::stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const StampInfo& info) {
    if (!controlling_elem) return;
    int ctrl_idx = info.extraStart + controlling_elem->extraVariableIndex;

    int n1 = get_matrix_idx(node1, info.nodeIdx);
    int n2 = get_matrix_idx(node2, info.nodeIdx);

    if (n1 != -1) A(n1, ctrl_idx) += value;
    if (n2 != -1) A(n2, ctrl_idx) -= value;
}

// the controlling element has to be something with a current variable (V, L, E, H, ...)
static Element* findControl(const std::string& who, const std::string& ctrl, const std::vector<Element*>& all) {
    for (Element* e : all) {
        if (e->name != ctrl) continue;
        if (!e->introducesExtraVariable) {
            std::cerr << "Warning: " << who << " is controlled by " << ctrl
                      << " but its current is not a circuit variable (use a voltage source or inductor)." << std::endl;
            return nullptr;
        }
        return e;
    }
    std::cerr << "Warning: " << who << " could not find controlling source: " << ctrl << std::endl;
    return nullptr;
}

bool cccs::linkControlSource(const std::vector<Element*>& all) {
    controlling_elem = findControl(name, controlling_name, all);
    return controlling_elem != nullptr;
}

void ccvs::stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const StampInfo& info) {
    if (!controlling_elem) return;
    int k = info.extraStart + extraVariableIndex;
    int ctrl_idx = info.extraStart + controlling_elem->extraVariableIndex;

    int n1 = get_matrix_idx(node1, info.nodeIdx);
    int n2 = get_matrix_idx(node2, info.nodeIdx);

    // v1 - v2 - r*i_ctrl = 0
    stampBranch(A, n1, n2, k);
    A(k, ctrl_idx) -= value;
}

bool ccvs::linkControlSource(const std::vector<Element*>& all) {
    controlling_elem = findControl(name, controlling_name, all);
    return controlling_elem != nullptr;
}

void SinusoidalSource::stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const StampInfo& info) {
    int n1_idx = get_matrix_idx(node1, info.nodeIdx);
    int n2_idx = get_matrix_idx(node2, info.nodeIdx);
    int k = info.extraStart + extraVariableIndex;

    stampBranch(A, n1_idx, n2_idx, k);
    b(k) += getInstantaneousValue();
}

void PulseSource::stampMNA(Eigen::MatrixXd& A, Eigen::VectorXd& b, const StampInfo& info) {
    int n1_idx = get_matrix_idx(node1, info.nodeIdx);
    int n2_idx = get_matrix_idx(node2, info.nodeIdx);
    int k = info.extraStart + extraVariableIndex;

    stampBranch(A, n1_idx, n2_idx, k);
    b(k) += getInstantaneousValue();
}

double PulseSource::getInstantaneousValue() const {
    // Before the delay time, the voltage is at its initial value
    if (time <= td) {
        return v1;
    }

    // per <= 0 means a single pulse
    double t = time - td;
    if (per > 0) t = std::fmod(t, per);

    if (t < tr) {
        return v1 + (v2 - v1) * (t / tr);
    } else if (t < tr + pw) {
        return v2;
    } else if (t < tr + pw + tf) {
        return v2 - (v2 - v1) * ((t - (tr + pw)) / tf);
    }
    return v1;
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
              << "TD=" << td << "s, TR=" << tr << "s, TF=" << tf << "s, "
              << "PW=" << pw << "s, PER=" << per << "s, "
              << "Nodes: " << node1 << "-" << node2 << std::endl;
}
