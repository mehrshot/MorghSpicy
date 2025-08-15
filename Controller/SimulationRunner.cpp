#include "SimulationRunner.h"
#include "Model/MNASolver.h"
#include "Model/NodeManager.h"
#include "Model/Elements.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <functional>

// Constants for the Newton-Raphson iterative solver
const int MAX_NR_ITERATIONS = 100;
const double NR_TOLERANCE = 1e-6;

SimulationRunner::SimulationRunner(Graph* g, MNASolver* solver, NodeManager* n)
        : graph(g), mnaSolver(solver), nm(n) {}

PlotData SimulationRunner::runTransient(double tstep_initial, double tstop, double tmaxstep, const std::vector<OutputVariable>& requested_vars) {
    PlotData plotData;
    mnaSolver->initializeMatrix(*graph);

    graph->canonicalizeNodes(*nm);

    nm->displayNodes();
    for (auto& e : graph->elements) {        // adjust if accessor differs
        const Element* el = e;         // or `e` if you store raw pointers
        std::cout << el->name << ": (" << el->node1 << ", " << el->node2 << ")\n";
    }


    mnaSolver->initializeMatrix(*graph);

    // Initial sanity checks
    if (mnaSolver->getTotalUnknowns() == 0 || !graph->isConnected()) {
        std::cerr << "[WARN] Pre-check failed; attempting transient-only with gmin.\n";
        // DO NOT return; let the code proceed to build the transient MNA and step
    }

    // --- Optimization: Pre-resolve output variable getters ---
    // Instead of searching for nodes/elements by name inside the loop,
    // we create a vector of functions that directly access the results.
    std::vector<std::function<double(const Eigen::VectorXd&, const Eigen::VectorXd&, double)>> value_getters;
    for (const auto& var : requested_vars) {
        plotData.series_names.push_back((var.type == OutputVariable::VOLTAGE ? "V(" : "I(") + var.name + ")");
        plotData.data_series.emplace_back();

        if (var.type == OutputVariable::VOLTAGE) {
            int node_id = nm->resolveId(var.name);
            if (node_id != -1 && mnaSolver->getNodeToMatrixIdxMap().count(node_id)) {
                int matrix_idx = mnaSolver->getNodeToMatrixIdxMap().at(node_id);
                value_getters.push_back([matrix_idx](const Eigen::VectorXd& sol, const Eigen::VectorXd&, double) {
                    return sol(matrix_idx);
                });
            } else {
                value_getters.push_back([](const Eigen::VectorXd&, const Eigen::VectorXd&, double) { return 0.0; }); // Placeholder
            }
        } else { // CURRENT
            Element* elem = graph->findElement(var.name);
            if (elem) {
                value_getters.push_back([this, elem](const Eigen::VectorXd& sol, const Eigen::VectorXd& prev_sol, double h) {
                    return this->calculate_element_current(elem, sol, prev_sol, h);
                });
            } else {
                value_getters.push_back([](const Eigen::VectorXd&, const Eigen::VectorXd&, double) { return 0.0; }); // Placeholder
            }
        }
    }

    Eigen::VectorXd prev_solution(mnaSolver->getTotalUnknowns());
    prev_solution.setZero();

    // --- Optimization: Pre-allocate vector memory ---
    size_t estimated_steps = static_cast<size_t>(tstop / tstep_initial) + 100;
    plotData.time_axis.reserve(estimated_steps);
    for(auto& series : plotData.data_series) {
        series.reserve(estimated_steps);
    }

    double time = 0.0;
    double h = tstep_initial;

    // Store initial conditions at t=0
    plotData.time_axis.push_back(time);
    for(auto& series : plotData.data_series) {
        series.push_back(0.0);
    }

    // --- Main simulation loop ---
    while (time < tstop) {
        if (time + h > tstop) { h = tstop - time; }
        if (h > tmaxstep) { h = tmaxstep; }

        // Update time-dependent sources
        graph->updateTimeDependentSources(time + h);

        mnaSolver->constructMNAMatrix(*graph, h, prev_solution);
        Eigen::VectorXd final_solution = mnaSolver->solve();

        if (final_solution.size() == 0) {
            std::cerr << "Error: Solver failed at time " << time << ". Aborting." << std::endl;
            break;
        }

        time += h;

        // Store results
        plotData.time_axis.push_back(time);
        for (size_t i = 0; i < value_getters.size(); ++i) {
            double result = value_getters[i](final_solution, prev_solution, h);
            plotData.data_series[i].push_back(result);
        }

        prev_solution = final_solution;

        // Note: Variable time-stepping logic (LTE) could be re-implemented here for adaptive step control.
    }

    return plotData;
}
void SimulationRunner::runDCSweep(const std::string& sourceName, double start, double stop, double increment, const std::vector<OutputVariable>& requested_vars) {
    mnaSolver->initializeMatrix(*graph);

    graph->canonicalizeNodes(*nm);
    mnaSolver->initializeMatrix(*graph);


    if (mnaSolver->getTotalUnknowns() == 0) {
        std::cerr << "Error: Simulation cannot run, the circuit is not correctly defined." << std::endl;
        return;
    }
    if (!graph->isConnected()) {
        std::cerr << "Error: Circuit is disconnected or contains floating nodes." << std::endl;
        return;
    }

    Element* swept_element = graph->findElement(sourceName);
    if (!swept_element) {
        std::cerr << "Error: Source '" << sourceName << "' not found for DC sweep." << std::endl;
        return;
    }

    std::cout << "Running DC Sweep Analysis..." << std::endl;

    std::cout << std::left << std::setw(15) << sourceName;
    for (const auto& var : requested_vars) {
        std::string header = (var.type == OutputVariable::VOLTAGE ? "V(" : "I(") + var.name + ")";
        std::cout << std::setw(15) << header;
    }
    std::cout << std::endl;

    Eigen::VectorXd current_guess(mnaSolver->getTotalUnknowns());
    current_guess.setZero();
    double large_timestep_for_dc = 1e12; // To simulate DC conditions

    // Main DC sweep loop
    for (double current_val = start; current_val <= stop; current_val += increment) {
        swept_element->setValue(current_val);
        Eigen::VectorXd final_solution;
        bool converged = false;

        // Newton-Raphson inner loop for each sweep step
        for (int nr_iter = 0; nr_iter < MAX_NR_ITERATIONS; ++nr_iter) {
            mnaSolver->constructMNAMatrix(*graph, large_timestep_for_dc, current_guess);
            final_solution = mnaSolver->solve();

            if (final_solution.size() == 0) break; // Solver failed

            Eigen::VectorXd diff = final_solution - current_guess;
            if (diff.norm() < NR_TOLERANCE) {
                converged = true;
                break;
            }
            current_guess = final_solution;
        }

        if (!converged) {
            std::cerr << "Warning: Newton-Raphson failed to converge for " << sourceName << " = " << current_val << "." << std::endl;
        }

        // Print results for the current sweep step
        std::cout << std::left << std::fixed << std::setprecision(6);
        std::cout << std::setw(15) << current_val;
        for (const auto& var : requested_vars) {
            double result = 0.0;
            if (var.type == OutputVariable::VOLTAGE) {
                int node_id = -1;
                for(const auto& node : graph->getNodes()){ if(node->getName() == var.name){ node_id = node->getId(); break; } }
                if (node_id != -1 && mnaSolver->getNodeToMatrixIdxMap().count(node_id)) {
                    result = final_solution(mnaSolver->getNodeToMatrixIdxMap().at(node_id));
                }
            } else { // CURRENT
                Element* elem = graph->findElement(var.name);
                if (elem) {
                    result = calculate_element_current(elem, final_solution, current_guess, large_timestep_for_dc);
                }
            }
            std::cout << std::setw(15) << result;
        }
        std::cout << std::endl;

        current_guess = final_solution; // Use as initial guess for next sweep step
    }
}

// This helper function calculates element currents based on the final solution
double SimulationRunner::calculate_element_current(Element* elem, const Eigen::VectorXd& solution_vector, const Eigen::VectorXd& prev_solution, double h) {
    if (!elem) return 0.0;

    if (elem->introducesExtraVariable) {
        int extra_var_idx = mnaSolver->getExtraVariableStartIndex() + elem->extraVariableIndex;
        return solution_vector(extra_var_idx);
    }

    const auto& node_map = mnaSolver->getNodeToMatrixIdxMap();
    int n1_id = elem->node1;
    int n2_id = elem->node2;

    double v1 = (n1_id == 0) ? 0.0 : solution_vector(node_map.at(n1_id));
    double v2 = (n2_id == 0) ? 0.0 : solution_vector(node_map.at(n2_id));

    switch (elem->type) {
        case RESISTOR:
            return (v1 - v2) / elem->value;
        case CAPACITOR: {
            if (h >= 1e12) return 0.0; // No current through capacitor in DC
            double prev_v1 = (n1_id == 0) ? 0.0 : prev_solution(node_map.at(n1_id));
            double prev_v2 = (n2_id == 0) ? 0.0 : prev_solution(node_map.at(n2_id));
            return elem->value * ((v1 - v2) - (prev_v1 - prev_v2)) / h;
        }
        case CURRENT_SOURCE:
            return elem->value;
        case DIODE: {
            // For a diode, we re-calculate the current using the final converged voltage
            auto* diode = static_cast<Diode*>(elem);
            double vd = v1 - v2;
            if (diode->model == "Z" && vd < -diode->Vz) {
                return (vd - (-diode->Vz)) / 1.0; // Current in Zener breakdown
            } else {
                return diode->Is * (std::exp(vd / (diode->n * diode->Vt)) - 1.0);
            }
        }
        default:
            return 0.0;
    }
}