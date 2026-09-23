//
// Created by Mehrshad on 5/29/2025.
//

#include <set>

#include "MNASolver.h"
#include "Graph.h"
#include "Elements.h"

bool MNASolver::initializeMatrix(const Graph& circuitGraph) {
    total_unknowns = 0;
    num_non_ground_nodes = 0;
    num_extra_vars = 0;
    node_id_to_matrix_idx.clear();
    A_matrix.resize(0, 0);
    b_vector.resize(0);

    const auto& elems = circuitGraph.getElements();
    if (elems.empty()) {
        std::cerr << "Error: The circuit is empty." << std::endl;
        return false;
    }

    // 1. collect all the non-ground nodes (control nodes of E/G count too)
    std::set<int> node_ids;
    bool has_ground = false;
    auto addNode = [&](int id) {
        if (id == 0) has_ground = true;
        else node_ids.insert(id);
    };
    for (Element* e : elems) {
        addNode(e->node1);
        addNode(e->node2);
        if (auto* g = dynamic_cast<vccs*>(e)) { addNode(g->controlPos()); addNode(g->controlNeg()); }
        if (auto* v = dynamic_cast<vcvs*>(e)) { addNode(v->controlPos()); addNode(v->controlNeg()); }
    }

    if (!has_ground) {
        std::cerr << "Error: No ground node detected in the circuit." << std::endl;
        return false;
    }

    // 2. std::set is already sorted, so the matrix order follows the node ids
    for (int id : node_ids) {
        node_id_to_matrix_idx[id] = num_non_ground_nodes++;
    }

    // 3. every V, L, E, H, SIN, PULSE gets a current variable after the node voltages
    for (Element* e : elems) {
        e->extraVariableIndex = e->introducesExtraVariable ? num_extra_vars++ : -1;
    }

    // 4. current controlled sources need the element they are looking at
    for (Element* e : elems) {
        bool ok = true;
        if (auto* f = dynamic_cast<cccs*>(e)) ok = f->linkControlSource(elems);
        else if (auto* h = dynamic_cast<ccvs*>(e)) ok = h->linkControlSource(elems);
        if (!ok) return false;
    }

    total_unknowns = num_non_ground_nodes + num_extra_vars;
    if (total_unknowns == 0) {
        std::cerr << "Error: Nothing to solve, every element is connected to ground only." << std::endl;
        return false;
    }

    A_matrix = Eigen::MatrixXd::Zero(total_unknowns, total_unknowns);
    b_vector = Eigen::VectorXd::Zero(total_unknowns);
    solution_vector = Eigen::VectorXd::Zero(total_unknowns);
    return true;
}

void MNASolver::constructMNAMatrix(const Graph& circuitGraph, double timestep_h,
                                   const Eigen::VectorXd& prev_solution,
                                   const Eigen::VectorXd& guess) {
    if (total_unknowns <= 0) return;

    A_matrix.setZero();
    b_vector.setZero();

    StampInfo info{node_id_to_matrix_idx, getExtraVariableStartIndex(), prev_solution, guess, timestep_h};
    for (Element* elem_ptr : circuitGraph.getElements()) {
        elem_ptr->stampMNA(A_matrix, b_vector, info);
    }

    for (int i = 0; i < num_non_ground_nodes; ++i) {
        A_matrix(i, i) += gmin;
    }
}

Eigen::VectorXd MNASolver::solve() {
    if (total_unknowns == 0) {
        std::cerr << "Error: Cannot solve. Total unknowns is zero." << std::endl;
        return {};
    }

    Eigen::FullPivLU<Eigen::MatrixXd> lu(A_matrix);
    if (!lu.isInvertible()) {
        std::cerr << "Error: Circuit matrix is singular (check for voltage source loops or floating parts)." << std::endl;
        return {};
    }

    solution_vector = lu.solve(b_vector);
    return solution_vector;
}

void MNASolver::displayMatrix() const {
    std::cout << "\n--- MNA Matrix (A) ---" << std::endl;
    std::cout << A_matrix << std::endl;
    std::cout << "\n--- RHS Vector (b) ---" << std::endl;
    std::cout << b_vector.transpose() << std::endl;
}

void MNASolver::displaySolution() const {
    std::cout << "\n--- Solution Vector (x) ---" << std::endl;
    std::cout << solution_vector.transpose() << std::endl;
}
