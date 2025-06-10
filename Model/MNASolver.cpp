//
// Created by Mehrshad on 5/29/2025.
//

#include "MNASolver.h"

#include "Graph.h"
#include "Elements.h"
#include "Node.h"

#include "Common_Includes.h"

MNASolver::MNASolver() :
        num_ground_nodes(0), // Node 0 => GND
        num_non_ground_nodes(0),
        num_voltage_sources(0),
        num_inductors(0),
        total_unknowns(0) {}


// Method to construct the MNA matrix (A and b)
void MNASolver::constructMNAMatrix(const Graph& circuitGraph, double timestep_h,
                                   const Eigen::VectorXd& prev_solution) {
    A_matrix.setZero();
    b_vector.setZero();

    const std::vector<Element*>& all_elements_in_graph = circuitGraph.getElements();
    for (Element* elem_ptr : all_elements_in_graph) {
        elem_ptr->stampMNA(A_matrix, b_vector, node_id_to_matrix_idx,
                           getExtraVariableStartIndex(), prev_solution, timestep_h);
    }
//    std::cout << "MNA Matrix constructed." << std::endl;
}

Eigen::VectorXd MNASolver::solve() {
    if (total_unknowns == 0) {
        std::cerr << "Error: Cannot solve. Total unknowns is zero." << std::endl;
        solution_vector.setZero();
        return solution_vector;
    }

    if (!A_matrix.fullPivLu().isInvertible()) {
        std::cerr << "Error: Circuit matrix is singular (not invertible). Cannot solve." << std::endl;
        solution_vector.setZero(); // Return zero vector to indicate failure
        return solution_vector;
    }

    solution_vector = A_matrix.lu().solve(b_vector);
//    std::cout << "MNA System solved." << std::endl;
    return solution_vector;
}

// Display methods
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

void MNASolver::displayNodeVoltages() const {
    std::cout << "\n--- Node Voltages ---" << std::endl;
    std::cout << "Node 0 (GND): 0.000000 V" << std::endl;

    for (const auto& pair : node_id_to_matrix_idx) {
        int node_id = pair.first;
        int matrix_idx = pair.second;

        if (matrix_idx < num_non_ground_nodes) {
            std::cout.precision(6);
            std::cout << std::fixed;
            std::cout << "Node " << node_id << " (V_" << node_id << "): "
                      << solution_vector(matrix_idx) << " V" << std::endl;
        }
    }
}
void MNASolver::initializeMatrix(const Graph& circuitGraph) {
    // 1. Identify all unique non-ground nodes from the graph.
    std::set<int> unique_node_ids;
    const std::vector<Node*>& all_nodes_in_graph = circuitGraph.getNodes();
    for (Node* node_ptr : all_nodes_in_graph) {
        if (node_ptr->getId() != 0) {
            unique_node_ids.insert(node_ptr->getId());
        }
    }

    // 2. Check for the existence of a ground node (ID 0). This is critical.
    bool has_ground_node = false;
    for (Node* node_ptr : all_nodes_in_graph) {
        if (node_ptr->getId() == 0) {
            has_ground_node = true;
            break;
        }
    }
    if (!has_ground_node && !all_nodes_in_graph.empty()) {
        std::cerr << "Error: No ground node (ID 0) detected in the circuit." << std::endl;
        total_unknowns = 0;
        return;
    }

    // 3. Create a sorted map from node ID to its corresponding matrix index (0, 1, 2, ...).
    std::vector<int> sorted_non_ground_node_ids(unique_node_ids.begin(), unique_node_ids.end());
    std::sort(sorted_non_ground_node_ids.begin(), sorted_non_ground_node_ids.end());

    num_non_ground_nodes = sorted_non_ground_node_ids.size();
    node_id_to_matrix_idx.clear();
    for (int i = 0; i < num_non_ground_nodes; ++i) {
        node_id_to_matrix_idx[sorted_non_ground_node_ids[i]] = i;
    }

    // 4. Link current-controlled sources to their controlling voltage source.

    // This step is necessary before counting extra variables.
    const std::vector<Element*>& all_elements = circuitGraph.getElements();



    int num_extra_vars = 0;
    for (Element* elem : all_elements) {
        if (elem->introducesExtraVariable) {
            elem->extraVariableIndex = num_extra_vars++;
        }
    }


    for (Element* e : all_elements) {
        if (e->type == CCCS) {
            dynamic_cast<cccs*>(e)->linkControlSource(all_elements);
        } else if (e->type == CCVS) {
            dynamic_cast<ccvs*>(e)->linkControlSource(all_elements);
        }
    }

    // 5. Count ALL elements that introduce an extra variable (V, L, VCVS, CCVS, etc.)
    // and assign them their index in the 'extra variables' section of the matrix.

    // 6. Calculate the total size of the MNA matrix.
    total_unknowns = num_non_ground_nodes + num_extra_vars;

    // Final sanity check
    if (total_unknowns == 0 && !all_nodes_in_graph.empty()) {
        std::cerr << "Error: No non-ground nodes or extra variables. Circuit might not be solvable." << std::endl;
        return;
    }

    // 7. Resize matrices to the correct dimensions and initialize with zeros.
    A_matrix.resize(total_unknowns, total_unknowns);
    b_vector.resize(total_unknowns);
    A_matrix.setZero();
    b_vector.setZero();
}
void MNASolver::displayElementCurrents(const Graph& circuitGraph) const {
    std::cout << "\n--- Element Currents (from extra variables) ---" << std::endl;
    int extra_var_start_idx = getExtraVariableStartIndex();

    const std::vector<Element*>& all_elements_in_graph = circuitGraph.getElements();
    for (Element* elem_ptr : all_elements_in_graph) {
        if (elem_ptr->introducesExtraVariable) {
            int current_idx_in_solution = extra_var_start_idx + elem_ptr->extraVariableIndex;
            if (current_idx_in_solution < solution_vector.size()) {
                std::cout.precision(6);
                std::cout << std::fixed;
                std::cout << "Current through " << elem_ptr->name << " (I_" << elem_ptr->name << "): "
                          << solution_vector(current_idx_in_solution) << " A" << std::endl;
            }
        }
    }
}