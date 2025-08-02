#include "SimulationRunner.h"
#include <iostream>
#include <iomanip>
#include <cmath>

// Constants for the Newton-Raphson iterative solver
const int MAX_NR_ITERATIONS = 100;
const double NR_TOLERANCE = 1e-6;

SimulationRunner::SimulationRunner(Graph* g, MNASolver* solver) : graph(g), mnaSolver(solver) {}

void SimulationRunner::runTransient(double tstep_initial, double tstop, double tmaxstep, const std::vector<OutputVariable>& requested_vars) {
    mnaSolver->initializeMatrix(*graph);

    if (mnaSolver->getTotalUnknowns() == 0) {
        std::cerr << "Error: Simulation cannot run, circuit is not correctly defined." << std::endl;
        return;
    }

    if (!graph->isConnected()) {
        std::cerr << "Error: Circuit is disconnected or contains floating nodes." << std::endl;
        return;
    }

    // پارامترهای کنترل گام زمانی
    const double reltol = 1e-3;
    const double abstol = 1e-6;
    const double safety_factor = 0.9;
    const double max_increase_factor = 2.0;
    const double min_timestep = 1e-15;

    Eigen::VectorXd prev_solution(mnaSolver->getTotalUnknowns());
    prev_solution.setZero();
    Eigen::VectorXd final_solution;

    // چاپ هدر جدول
    std::cout << std::left << std::setw(12) << "Time";
    for (const auto& var : requested_vars) {
        std::string header = (var.type == OutputVariable::VOLTAGE ? "V(" : "I(") + var.name + ")";
        std::cout << std::setw(15) << header;
    }
    std::cout << std::endl;

    double time = 0.0;
    double h = tstep_initial;

    // چاپ مقادیر اولیه در زمان صفر
    std::cout << std::left << std::fixed << std::setprecision(6);
    std::cout << std::setw(12) << time;
    for (const auto& var : requested_vars) {
        std::cout << std::setw(15) << 0.0;
    }
    std::cout << std::endl;

    // حلقه اصلی شبیه‌سازی با گام متغیر
    while (time < tstop) {
        if (time + h > tstop) { h = tstop - time; }
        if (h > tmaxstep) { h = tmaxstep; }
        if (h < min_timestep) { h = min_timestep; }

        for (Element* e : graph->getElements()) {
            if (e->type == SINUSOIDAL_SOURCE) {
                dynamic_cast<SinusoidalSource*>(e)->updateTime(time + h);
            }
            else if (e->type == PULSE_SOURCE) {
                dynamic_cast<PulseSource*>(e)->updateTime(time + h);
            }
        }

        // *** مهم‌ترین تغییر اینجاست ***
        // برای مدارهای خطی (مثل RC)، نیازی به حلقه نیوتن-رافسون نیست.
        // ماتریس فقط یک بار با استفاده از تاریخچه واقعی (prev_solution) ساخته و حل می‌شود.
        mnaSolver->constructMNAMatrix(*graph, h, prev_solution);
        final_solution = mnaSolver->solve();

        if (final_solution.size() == 0) {
            std::cerr << "Error: Solver failed at time " << time << ". Aborting." << std::endl;
            break;
        }

        // --- تخمین خطا ---
        double error_norm = 0.0;
        if (final_solution.size() > 0) {
            for (int i = 0; i < final_solution.size(); ++i) {
                double estimated_error = std::abs(final_solution(i) - prev_solution(i));
                double tolerance = reltol * std::abs(final_solution(i)) + abstol;
                if (tolerance > 1e-20) {
                    error_norm += pow(estimated_error / tolerance, 2);
                }
            }
            error_norm = sqrt(error_norm / final_solution.size());
        }

        // چون حلقه نیوتن-رافسون حذف شده، دیگر نیازی به بررسی همگرایی آن نیست
        // و مستقیماً به سراغ منطق گام زمانی می‌رویم.

        time += h;

        // چاپ نتایج
        std::cout << std::left << std::fixed << std::setprecision(6);
        std::cout << std::setw(12) << time;
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
                    result = calculate_element_current(elem, final_solution, prev_solution, h);
                }
            }
            std::cout << std::setw(15) << result;
        }
        std::cout << std::endl;

        prev_solution = final_solution;

        // محاسبه گام زمانی بعدی
        if (error_norm < 1.0) {
            if (error_norm < 1e-9) {
                h *= max_increase_factor;
            } else {
                double h_new = h * safety_factor * pow(error_norm, -0.2);
                if (h_new > h * max_increase_factor) {
                    h = h * max_increase_factor;
                } else {
                    h = h_new;
                }
            }
        } else {
            h = h * safety_factor * pow(error_norm, -0.25);
        }
    }
}

void SimulationRunner::runDCSweep(const std::string& sourceName, double start, double stop, double increment, const std::vector<OutputVariable>& requested_vars) {
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