#include "SimulationRunner.h"
#include "Model/MNASolver.h"
#include "Model/NodeManager.h"
#include "Model/Elements.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <functional>

using cplx = std::complex<double>;
static inline cplx j() { return cplx(0.0, 1.0); }

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

std::vector<double> SimulationRunner::makeACGrid(ACSweepKind kind, double w_start, double w_stop, int points) const {
    std::vector<double> w;
    w.reserve(points);
    if (kind == ACSweepKind::Linear) {
        if (points <= 1) { w.push_back(w_start); return w; }
        double step = (w_stop - w_start) / double(points - 1);
        for (int i = 0; i < points; ++i) w.push_back(w_start + i * step);
    } else {
        double base = (kind == ACSweepKind::Decade) ? 10.0 : 2.0;
        double a = std::log(w_start) / std::log(base);
        double b = std::log(w_stop) / std::log(base);
        if (points <= 1) { w.push_back(w_start); return w; }
        double step = (b - a) / double(points - 1);
        for (int i = 0; i < points; ++i) w.push_back(std::pow(base, a + i * step));
    }
    return w;
}

Eigen::VectorXcd SimulationRunner::solveACOnce(double w) const {
    const auto& elems = graph->getElements();
    std::unordered_map<int,int> node2idx;
    std::vector<int> nodeIds;
    nodeIds.reserve(graph->getNodes().size());
    for (auto* n : graph->getNodes()) {
        for (auto* n : graph->getNodes()) {
            const int nid = n->getId();
            if (nid != 0) {                       // ground node is id==0 in your codebase
                node2idx[nid] = (int)nodeIds.size();
                nodeIds.push_back(nid);
            }
        }
    }
    int n = int(nodeIds.size());

    int m = 0;
    for (auto* e : elems) {
        if (e->type == ElementType::VOLTAGE_SOURCE || e->type == ElementType::VCVS || e->type == ElementType::CCVS) ++m;
    }

    Eigen::MatrixXcd A = Eigen::MatrixXcd::Zero(n + m, n + m);
    Eigen::VectorXcd b = Eigen::VectorXcd::Zero(n + m);

    auto rowOf = [&](int node)->int{
        auto it = node2idx.find(node);
        return it==node2idx.end()? -1 : it->second;
    };

    int extra = n;
    auto takeExtra = [&](){ return extra++; };

    for (auto* e : elems) {
        int r1 = rowOf(e->node1);
        int r2 = rowOf(e->node2);

        switch (e->type) {
            case ElementType::RESISTOR: {
                double R = e->value;
                cplx Y = 1.0 / R;
                if (r1!=-1) A(r1,r1) += Y;
                if (r2!=-1) A(r2,r2) += Y;
                if (r1!=-1 && r2!=-1) { A(r1,r2) -= Y; A(r2,r1) -= Y; }
            } break;

            case ElementType::CAPACITOR: {
                double C = e->value;
                cplx Y = j()*w*C;
                if (r1!=-1) A(r1,r1) += Y;
                if (r2!=-1) A(r2,r2) += Y;
                if (r1!=-1 && r2!=-1) { A(r1,r2) -= Y; A(r2,r1) -= Y; }
            } break;

            case ElementType::INDUCTOR: {
                double L = e->value;
                cplx Y = 1.0 / (j()*w*L);
                if (r1!=-1) A(r1,r1) += Y;
                if (r2!=-1) A(r2,r2) += Y;
                if (r1!=-1 && r2!=-1) { A(r1,r2) -= Y; A(r2,r1) -= Y; }
            } break;

            case ElementType::CURRENT_SOURCE: {
                double I = e->value;
                if (r1!=-1) b(r1) -= I;
                if (r2!=-1) b(r2) += I;
            } break;

            case ElementType::VOLTAGE_SOURCE:
            case ElementType::VCVS:
            case ElementType::CCVS: {
                int k = takeExtra();
                if (r1!=-1) { A(r1,k) += 1.0; A(k,r1) += 1.0; }
                if (r2!=-1) { A(r2,k) -= 1.0; A(k,r2) -= 1.0; }
                if (e->type == ElementType::VOLTAGE_SOURCE) {
                    b(k) += e->value;
                } else if (e->type == ElementType::VCVS) {
                    auto* v = static_cast<vcvs*>(e);
                    int rc1 = rowOf(v->controlPos());
                    int rc2 = rowOf(v->controlNeg());
                    double mu = v->getValue();
                    if (rc1!=-1) A(k,rc1) -= mu;
                    if (rc2!=-1) A(k,rc2) += mu;
                } else {
                    auto* v = static_cast<ccvs*>(e);
                    double r = v->getValue();
                    if (v->extraVariableIndex >= 0) {
                        int kc = n + v->extraVariableIndex;
                        A(k,kc) -= r;
                    }
                }
            } break;

            case ElementType::DIODE:
            case ElementType::VCCS:
            case ElementType::CCCS:
            case ElementType::SINUSOIDAL_SOURCE:
            case ElementType::PULSE_SOURCE:
            default:
                break;
        }
    }

    return A.fullPivLu().solve(b);
}

std::complex<double> SimulationRunner::getPhasorAt(const OutputVariable& v,
                                                   const Eigen::VectorXcd& x) const {
    if (v.type == OutputVariable::VOLTAGE) {
        int id = nm->resolveId(v.name);
        const auto& map = mnaSolver->getNodeToMatrixIdxMap();
        auto it = map.find(id);
        if (it == map.end()) return cplx(0,0);
        int idx = it->second;
        if (idx >= 0 && idx < x.size()) return x(idx);
        return cplx(0,0);
    }
    return cplx(0,0);
}

PlotData SimulationRunner::runACSweep(const ACSweepSettings& s,
                                      const std::vector<OutputVariable>& what) {
    graph->canonicalizeNodes(*nm);
    auto wgrid = makeACGrid(s.kind, s.w_start, s.w_stop, s.points);

    PlotData plot;
    plot.time_axis.clear();
    plot.data_series.assign(what.size(), {});
    plot.series_names.clear();
    plot.series_names.reserve(what.size());
    for (const auto& ov : what) {
        plot.series_names.push_back(ov.type == OutputVariable::VOLTAGE
                                    ? "V(" + ov.name + ")"
                                    : "I(" + ov.name + ")");
    }

    for (double w : wgrid) {
        Eigen::VectorXcd x = solveACOnce(w);
        plot.time_axis.push_back(w);

        for (size_t k = 0; k < what.size(); ++k) {
            auto ph = getPhasorAt(what[k], x);
            plot.data_series[k].push_back(std::abs(ph));
        }
    }
    return plot;
}


PlotData SimulationRunner::runPhaseSweep(const PhaseSweepSettings& s,
                                         const std::vector<OutputVariable>& what) {
    graph->canonicalizeNodes(*nm);

    std::vector<double> phis;
    phis.reserve(std::max(1, s.points));
    if (s.points <= 1) {
        phis.push_back(s.phi_start);
    } else {
        double step = (s.phi_stop - s.phi_start) / double(s.points - 1);
        for (int i = 0; i < s.points; ++i) phis.push_back(s.phi_start + i * step);
    }

    PlotData plot;
    plot.time_axis.clear();
    plot.data_series.assign(what.size(), {});
    plot.series_names.clear();
    plot.series_names.reserve(what.size());
    for (const auto& ov : what) {
        plot.series_names.push_back(ov.type == OutputVariable::VOLTAGE
                                    ? "V(" + ov.name + ")"
                                    : "I(" + ov.name + ")");
    }

    const auto& elems = graph->getElements();
    for (double phi : phis) {
        double savedV = 0.0;
        for (auto* e : elems) {
            if (e->type == ElementType::VOLTAGE_SOURCE) {
                savedV = e->value;
                // Set a unit-magnitude source with the requested phase (real part used in this model).
                e->value = std::polar(1.0, phi).real();
                break;
            }
        }

        Eigen::VectorXcd x = solveACOnce(s.w_base);

        for (auto* e : elems) {
            if (e->type == ElementType::VOLTAGE_SOURCE) { e->value = savedV; break; }
        }

        plot.time_axis.push_back(phi);
        for (size_t k = 0; k < what.size(); ++k) {
            auto ph = getPhasorAt(what[k], x);
            plot.data_series[k].push_back(std::abs(ph));
        }
    }

    return plot;
}

void SimulationRunner::setElementValue(const std::string& name, double value) {
    Element* e = graph->findElement(name);
    if (e) e->value = value;
}

