//
// Created by Mehrshad on 6/7/2025.
//

#include "SimulationRunner.h"
#include <iostream>
#include <iomanip>

SimulationRunner::SimulationRunner(Graph* g, MNASolver* solver) : graph(g), mnaSolver(solver) {}

void SimulationRunner::runTransient(double tstep, double tstop, const std::vector<OutputVariable>& requested_vars) {
    mnaSolver->initializeMatrix(*graph);

    if (mnaSolver->getTotalUnknowns() == 0) {
        std::cerr << "Error: Simulation cannot run, the circuit is not correctly defined." << std::endl;
        return;
    }

    // Initialize the previous solution vector with zeros for t=0
    Eigen::VectorXd prev_solution(mnaSolver->getTotalUnknowns());
    prev_solution.setZero();

    std::cout << "MNA system is being constructed and solved at each time step..." << std::endl;

    // Print table header
    std::cout << std::left << std::setw(12) << "Time";
    for (const auto& var : requested_vars) {
        std::string header = (var.type == OutputVariable::VOLTAGE ? "V(" : "I(") + var.name + ")";
        std::cout << std::setw(15) << header;
    }
    std::cout << std::endl;

    // 1. Print the initial state at t=0
    std::cout << std::left << std::fixed << std::setprecision(6);
    std::cout << std::setw(12) << 0.0;
    for (const auto& var : requested_vars) {
        // At t=0, all dynamic variables are zero.
        std::cout << std::setw(15) << 0.0;
    }
    std::cout << std::endl;

    // 2. Start the simulation loop from the first time step
    for (double time = tstep; time <= tstop; time += tstep) {

        mnaSolver->constructMNAMatrix(*graph, tstep, prev_solution);
        Eigen::VectorXd current_solution = mnaSolver->solve();

        if (current_solution.size() == 0) {
            std::cerr << "Warning: Simulation failed at time t = " << time << "s. Analysis stopped." << std::endl;
            break;
        }

        // Print current time step results
        std::cout << std::left << std::fixed << std::setprecision(6);
        std::cout << std::setw(12) << time;

        for (const auto& var : requested_vars) {
            double result = 0.0;
            // The logic for calculating voltage and current remains the same
            if (var.type == OutputVariable::VOLTAGE) {
                // To get node ID by name, we iterate through nodes
                int node_id = -1;
                for(const auto& node : graph->getNodes()){
                    if(node->getName() == var.name){
                        node_id = node->getId();
                        break;
                    }
                }
                if (node_id != -1 && mnaSolver->getNodeToMatrixIdxMap().count(node_id)) {
                    result = current_solution(mnaSolver->getNodeToMatrixIdxMap().at(node_id));
                }
            } else { // CURRENT
                Element* elem = graph->findElement(var.name);
                if (elem) {
                    result = calculate_element_current(elem, current_solution, prev_solution, tstep);
                }
            }
            std::cout << std::setw(15) << result;
        }
        std::cout << std::endl;

        prev_solution = current_solution; // Update solution for the next step
    }
}
// Private helper to calculate current for any element type
double SimulationRunner::calculate_element_current(Element* elem, const Eigen::VectorXd& solution_vector, const Eigen::VectorXd& prev_solution, double h) {
    if (!elem) return 0.0;

    // If the element introduces an extra variable, its current is directly in the solution vector
    if (elem->introducesExtraVariable) {
        int extra_var_idx = mnaSolver->getExtraVariableStartIndex() + elem->extraVariableIndex;
        return solution_vector(extra_var_idx);
    }

    // Otherwise, calculate it based on node voltages
    const auto& node_map = mnaSolver->getNodeToMatrixIdxMap();
    int n1_id = elem->node1;
    int n2_id = elem->node2;

    double v1 = (n1_id == 0) ? 0.0 : solution_vector(node_map.at(n1_id));
    double v2 = (n2_id == 0) ? 0.0 : solution_vector(node_map.at(n2_id));

    switch (elem->type) {
        case RESISTOR:
            return (v1 - v2) / elem->value;
        case CAPACITOR: {
            double prev_v1 = (n1_id == 0) ? 0.0 : prev_solution(node_map.at(n1_id));
            double prev_v2 = (n2_id == 0) ? 0.0 : prev_solution(node_map.at(n2_id));
            // Using backward euler: I = C * (V(t) - V(t-h)) / h
            return elem->value * ((v1 - v2) - (prev_v1 - prev_v2)) / h;
        }
        case CURRENT_SOURCE:
            return elem->value;
        default:
            return 0.0; // Should not happen for valid elements
    }
}