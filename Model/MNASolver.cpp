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

// Method to initialize the MNA matrix dimensions
void MNASolver::initializeMatrix(const Graph& circuitGraph) {
    // 1. Identify all unique non-ground nodes and assign them matrix indices.
    std::set<int> unique_node_ids;

    const std::vector<Node*>& all_nodes_in_graph = circuitGraph.getNodes();
    for (Node* node_ptr : all_nodes_in_graph) {
        if (node_ptr->getId() != 0) {
            unique_node_ids.insert(node_ptr->getId());
        }
    }

    std::vector<int> sorted_non_ground_node_ids(unique_node_ids.begin(), unique_node_ids.end());
    std::sort(sorted_non_ground_node_ids.begin(), sorted_non_ground_node_ids.end());

    num_non_ground_nodes = sorted_non_ground_node_ids.size();

    // Check for a ground node or any non-ground nodes for a solvable circuit
    if (num_non_ground_nodes == 0 && all_nodes_in_graph.empty()) { // If no nodes are defined at all.
        std::cerr << "Error: Circuit is empty. No nodes defined." << std::endl;
        total_unknowns = 0; // Set unknowns to 0 to prevent matrix resizing with 0 dimensions
        return; // Exit early if circuit is empty
    }
    bool has_ground_node = false;
    for (Node* node_ptr : all_nodes_in_graph) {
        if (node_ptr->getId() == 0) {
            has_ground_node = true;
            break;
        }
    }
    if (!has_ground_node) {
        std::cerr << "Error: No ground node (ID 0) detected in the circuit. A reference node is required." << std::endl;
        total_unknowns = 0;
        return;
    }


    node_id_to_matrix_idx.clear();
    for (int i = 0; i < num_non_ground_nodes; ++i) {
        node_id_to_matrix_idx[sorted_non_ground_node_ids[i]] = i;
    }

    // 2. Count voltage sources and inductors for extra variables.
    num_voltage_sources = 0;
    num_inductors = 0;

    const std::vector<Element*>& all_elements_in_graph = circuitGraph.getElements();
    for (Element* elem_ptr : all_elements_in_graph) {
        if (elem_ptr->type == VOLTAGE_SOURCE) {
            num_voltage_sources++;
        } else if (elem_ptr->type == INDUCTOR) {
            num_inductors++;
        }
    }

    // 3. Calculate total MNA matrix dimensions.
    total_unknowns = num_non_ground_nodes + num_voltage_sources + num_inductors;

    // If, after counting, total_unknowns is still 0, it indicates an issue (e.g., only ground node).
    if (total_unknowns == 0) {
        std::cerr << "Error: No non-ground nodes or extra variables. Circuit might not be solvable." << std::endl;
        return;
    }

    // 4. Resize Eigen matrices and vectors and set them to zero.
    A_matrix.resize(total_unknowns, total_unknowns);
    b_vector.resize(total_unknowns);
    solution_vector.resize(total_unknowns);

    A_matrix.setZero();
    b_vector.setZero();

    // 5. Assign extra variable indices to specific elements (voltage sources and inductors).
    int current_extra_var_idx_counter = 0;
    for (Element* elem_ptr : all_elements_in_graph) {
        if (elem_ptr->introducesExtraVariable) {
            elem_ptr->extraVariableIndex = current_extra_var_idx_counter++;
        }
    }

    std::cout << "MNASolver Initialized. Total unknowns: " << total_unknowns << std::endl;
    std::cout << "Non-ground nodes: " << num_non_ground_nodes << std::endl;
    std::cout << "Voltage sources: " << num_voltage_sources << std::endl;
    std::cout << "Inductors: " << num_inductors << std::endl;
}

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