#include "SimulationRunner.h"
#include "Model/Elements.h"
#include <iostream>
#include <iomanip>
#include <cmath>

using cplx = std::complex<double>;

// newton-raphson settings (only matters when there is a diode)
const int MAX_NR_ITERATIONS = 100;
const double NR_TOLERANCE = 1e-6;

// h this big turns capacitors into open and inductors into short circuits
const double DC_TIMESTEP = 1e12;

SimulationRunner::SimulationRunner(Graph* g, MNASolver* solver, NodeManager* n)
        : graph(g), mnaSolver(solver), nm(n) {}

bool SimulationRunner::prepare() {
    // wires and GND only change the node manager, so fix the element nodes first
    graph->canonicalizeNodes(*nm);
    if (!mnaSolver->initializeMatrix(*graph)) return false;

    if (!graph->isConnected()) {
        std::cerr << "Warning: some parts of the circuit are not connected to ground, their voltages are meaningless." << std::endl;
    }

    hasNonlinear = false;
    for (Element* e : graph->getElements()) {
        if (e->isNonlinear()) hasNonlinear = true;
    }
    return true;
}

bool SimulationRunner::checkOutputs(const std::vector<OutputVariable>& vars) const {
    for (const auto& v : vars) {
        if (v.type == OutputVariable::VOLTAGE) {
            if (!nm->findId(v.name)) {
                std::cerr << "Error: Node " << v.name << " not found in circuit" << std::endl;
                return false;
            }
        } else if (!graph->findElement(v.name)) {
            std::cerr << "Error: Component " << v.name << " not found in circuit" << std::endl;
            return false;
        }
    }
    return true;
}

// solves one time point. x comes in as the first guess and goes out as the answer
bool SimulationRunner::solvePoint(double h, const Eigen::VectorXd& prev, Eigen::VectorXd& x) {
    Eigen::VectorXd guess = x;
    for (int it = 0; it < MAX_NR_ITERATIONS; ++it) {
        mnaSolver->constructMNAMatrix(*graph, h, prev, guess);
        Eigen::VectorXd sol = mnaSolver->solve();
        if (sol.size() == 0) return false;

        if (!hasNonlinear || (sol - guess).norm() < NR_TOLERANCE * (1.0 + sol.norm())) {
            x = sol;
            return true;
        }
        guess = sol;
    }
    std::cerr << "Warning: Newton-Raphson did not converge, using the last iteration." << std::endl;
    x = guess;
    return true;
}

double SimulationRunner::nodeVoltage(const Eigen::VectorXd& x, int node_id) const {
    if (node_id == 0) return 0.0;
    const auto& m = mnaSolver->getNodeToMatrixIdxMap();
    auto it = m.find(node_id);
    if (it == m.end() || it->second >= x.size()) return 0.0;
    return x(it->second);
}

double SimulationRunner::calculate_element_current(Element* elem, const Eigen::VectorXd& x,
                                                   const Eigen::VectorXd& prev, double h) const {
    if (!elem) return 0.0;

    if (elem->introducesExtraVariable) {
        int k = mnaSolver->getExtraVariableStartIndex() + elem->extraVariableIndex;
        return k < x.size() ? x(k) : 0.0;
    }

    double v1 = nodeVoltage(x, elem->node1);
    double v2 = nodeVoltage(x, elem->node2);

    switch (elem->type) {
        case RESISTOR:
            return (v1 - v2) / elem->value;
        case CAPACITOR: {
            if (h >= DC_TIMESTEP || prev.size() == 0) return 0.0;
            double pv = nodeVoltage(prev, elem->node1) - nodeVoltage(prev, elem->node2);
            return elem->value * ((v1 - v2) - pv) / h;
        }
        case CURRENT_SOURCE:
            return elem->value;
        case DIODE:
            return static_cast<Diode*>(elem)->currentAt(v1 - v2);
        case VCCS: {
            auto* g = static_cast<vccs*>(elem);
            return g->value * (nodeVoltage(x, g->controlPos()) - nodeVoltage(x, g->controlNeg()));
        }
        case CCCS: {
            auto* f = static_cast<cccs*>(elem);
            if (!f->controlElement()) return 0.0;
            return f->value * calculate_element_current(f->controlElement(), x, prev, h);
        }
        default:
            return 0.0;
    }
}

double SimulationRunner::valueOf(const OutputVariable& v, const Eigen::VectorXd& x,
                                 const Eigen::VectorXd& prev, double h) const {
    if (v.type == OutputVariable::VOLTAGE) {
        auto id = nm->findId(v.name);
        return id ? nodeVoltage(x, *id) : 0.0;
    }
    return calculate_element_current(graph->findElement(v.name), x, prev, h);
}

void SimulationRunner::startPlot(PlotData& pd, const std::vector<OutputVariable>& vars) {
    pd.data_series.assign(vars.size(), {});
    pd.series_names.clear();
    for (const auto& v : vars) {
        pd.series_names.push_back((v.type == OutputVariable::VOLTAGE ? "V(" : "I(") + v.name + ")");
    }
}

PlotData SimulationRunner::runTransient(double tstep, double tstop, double tmaxstep,
                                        const std::vector<OutputVariable>& vars) {
    PlotData plotData;
    if (tstep <= 0 || tstop <= 0) {
        std::cerr << "Error: Invalid time parameters for TRAN analysis." << std::endl;
        return plotData;
    }
    if (!prepare() || !checkOutputs(vars)) return plotData;
    startPlot(plotData, vars);

    double h = tstep;
    if (tmaxstep > 0 && h > tmaxstep) h = tmaxstep;

    // everything starts from zero (capacitors empty, inductors without current)
    Eigen::VectorXd prev = Eigen::VectorXd::Zero(mnaSolver->getTotalUnknowns());
    Eigen::VectorXd x = prev;

    size_t steps = static_cast<size_t>(std::ceil(tstop / h - 1e-9));
    plotData.time_axis.reserve(steps + 1);
    for (auto& s : plotData.data_series) s.reserve(steps + 1);

    double time = 0.0;
    for (size_t i = 1; i <= steps; ++i) {
        double t_next = std::min(i * h, tstop);
        double dt = t_next - time;

        graph->updateTimeDependentSources(t_next);
        if (!solvePoint(dt, prev, x)) {
            std::cerr << "Error: Solver failed at time " << time << ". Aborting." << std::endl;
            break;
        }
        time = t_next;

        plotData.time_axis.push_back(time);
        for (size_t k = 0; k < vars.size(); ++k) {
            plotData.data_series[k].push_back(valueOf(vars[k], x, prev, dt));
        }
        prev = x;
    }

    graph->updateTimeDependentSources(0.0);
    return plotData;
}

PlotData SimulationRunner::runDCSweep(const std::string& sourceName, double start, double stop, double increment,
                                      const std::vector<OutputVariable>& vars) {
    PlotData plotData;
    Element* swept = graph->findElement(sourceName);
    if (!swept) {
        std::cerr << "Error: Source '" << sourceName << "' not found for DC sweep." << std::endl;
        return plotData;
    }
    if (swept->type != VOLTAGE_SOURCE && swept->type != CURRENT_SOURCE) {
        std::cerr << "Error: DC sweep only works on independent sources (V or I)." << std::endl;
        return plotData;
    }
    if (increment <= 0 || stop < start) {
        std::cerr << "Error: Bad DC sweep range." << std::endl;
        return plotData;
    }
    if (!prepare() || !checkOutputs(vars)) return plotData;

    startPlot(plotData, vars);
    plotData.x_label = sourceName;

    const double saved = swept->value;
    Eigen::VectorXd x = Eigen::VectorXd::Zero(mnaSolver->getTotalUnknowns());

    // counting steps with an int so the last point isn't lost to rounding
    int n = static_cast<int>(std::floor((stop - start) / increment + 1e-9)) + 1;
    for (int i = 0; i < n; ++i) {
        double val = start + i * increment;
        swept->setValue(val);

        Eigen::VectorXd prev = x;
        if (!solvePoint(DC_TIMESTEP, prev, x)) {
            std::cerr << "Error: Solver failed at " << sourceName << " = " << val << std::endl;
            break;
        }

        plotData.time_axis.push_back(val);
        for (size_t k = 0; k < vars.size(); ++k) {
            plotData.data_series[k].push_back(valueOf(vars[k], x, prev, DC_TIMESTEP));
        }
    }

    swept->setValue(saved);
    return plotData;
}

std::vector<double> SimulationRunner::makeACGrid(ACSweepKind kind, double w_start, double w_stop, int points) const {
    std::vector<double> w;
    if (points <= 1) { w.push_back(w_start); return w; }
    w.reserve(points);

    if (kind == ACSweepKind::Linear) {
        double step = (w_stop - w_start) / double(points - 1);
        for (int i = 0; i < points; ++i) w.push_back(w_start + i * step);
    } else {
        // equally spaced on a log scale (base doesn't really matter here, but keep it for clarity)
        double base = (kind == ACSweepKind::Decade) ? 10.0 : 2.0;
        double a = std::log(w_start) / std::log(base);
        double b = std::log(w_stop) / std::log(base);
        double step = (b - a) / double(points - 1);
        for (int i = 0; i < points; ++i) w.push_back(std::pow(base, a + i * step));
    }
    return w;
}

// phasor analysis at one angular frequency. uses the same row numbering as the transient
// solver so we can reuse its node map. sinusoidal sources are the AC inputs, DC sources are zeroed.
Eigen::VectorXcd SimulationRunner::solveACOnce(double w, const Element* phaseSrc, double phaseDeg) const {
    const int N = mnaSolver->getTotalUnknowns();
    const int ext = mnaSolver->getExtraVariableStartIndex();
    const auto& nodeMap = mnaSolver->getNodeToMatrixIdxMap();
    const cplx jw(0.0, w);

    Eigen::MatrixXcd A = Eigen::MatrixXcd::Zero(N, N);
    Eigen::VectorXcd b = Eigen::VectorXcd::Zero(N);

    auto rowOf = [&](int node) -> int {
        if (node == 0) return -1;
        auto it = nodeMap.find(node);
        return it == nodeMap.end() ? -1 : it->second;
    };
    auto admittance = [&](int r1, int r2, cplx Y) {
        if (r1 != -1) A(r1, r1) += Y;
        if (r2 != -1) A(r2, r2) += Y;
        if (r1 != -1 && r2 != -1) { A(r1, r2) -= Y; A(r2, r1) -= Y; }
    };
    auto branch = [&](int r1, int r2, int k) {
        if (r1 != -1) { A(r1, k) += 1.0; A(k, r1) += 1.0; }
        if (r2 != -1) { A(r2, k) -= 1.0; A(k, r2) -= 1.0; }
    };

    for (Element* e : graph->getElements()) {
        int r1 = rowOf(e->node1);
        int r2 = rowOf(e->node2);
        int k = e->introducesExtraVariable ? ext + e->extraVariableIndex : -1;

        switch (e->type) {
            case RESISTOR:
                admittance(r1, r2, 1.0 / e->value);
                break;
            case CAPACITOR:
                admittance(r1, r2, jw * e->value);
                break;
            case INDUCTOR:
                branch(r1, r2, k);
                A(k, k) -= jw * e->value;
                break;
            case VOLTAGE_SOURCE:
            case PULSE_SOURCE:
                branch(r1, r2, k);   // zero volts in AC -> short
                break;
            case SINUSOIDAL_SOURCE: {
                auto* s = static_cast<SinusoidalSource*>(e);
                double ph = (e == phaseSrc) ? phaseDeg : s->phaseDeg();
                branch(r1, r2, k);
                b(k) += std::polar(s->amplitude(), ph * std::numbers::pi / 180.0);
                break;
            }
            case VCCS: {
                auto* g = static_cast<vccs*>(e);
                int c1 = rowOf(g->controlPos()), c2 = rowOf(g->controlNeg());
                if (r1 != -1 && c1 != -1) A(r1, c1) += g->value;
                if (r1 != -1 && c2 != -1) A(r1, c2) -= g->value;
                if (r2 != -1 && c1 != -1) A(r2, c1) -= g->value;
                if (r2 != -1 && c2 != -1) A(r2, c2) += g->value;
                break;
            }
            case VCVS: {
                auto* v = static_cast<vcvs*>(e);
                int c1 = rowOf(v->controlPos()), c2 = rowOf(v->controlNeg());
                branch(r1, r2, k);
                if (c1 != -1) A(k, c1) -= v->value;
                if (c2 != -1) A(k, c2) += v->value;
                break;
            }
            case CCCS: {
                auto* f = static_cast<cccs*>(e);
                if (!f->controlElement()) break;
                int kc = ext + f->controlElement()->extraVariableIndex;
                if (r1 != -1) A(r1, kc) += f->value;
                if (r2 != -1) A(r2, kc) -= f->value;
                break;
            }
            case CCVS: {
                auto* hsrc = static_cast<ccvs*>(e);
                branch(r1, r2, k);
                if (hsrc->controlElement()) A(k, ext + hsrc->controlElement()->extraVariableIndex) -= hsrc->value;
                break;
            }
            case CURRENT_SOURCE:   // DC current source is open in AC
            case DIODE:            // no small signal model for now, treated as open
            default:
                break;
        }
    }

    for (int i = 0; i < ext; ++i) A(i, i) += 1e-12;

    Eigen::FullPivLU<Eigen::MatrixXcd> lu(A);
    if (!lu.isInvertible()) return {};
    return lu.solve(b);
}

std::complex<double> SimulationRunner::getPhasorAt(const OutputVariable& v, const Eigen::VectorXcd& x, double w) const {
    const auto& nodeMap = mnaSolver->getNodeToMatrixIdxMap();
    auto V = [&](int node) -> cplx {
        auto it = nodeMap.find(node);
        return (node == 0 || it == nodeMap.end()) ? cplx(0, 0) : x(it->second);
    };

    if (v.type == OutputVariable::VOLTAGE) {
        auto id = nm->findId(v.name);
        return id ? V(*id) : cplx(0, 0);
    }

    Element* e = graph->findElement(v.name);
    if (!e) return {0, 0};
    const int ext = mnaSolver->getExtraVariableStartIndex();
    if (e->introducesExtraVariable) return x(ext + e->extraVariableIndex);

    switch (e->type) {
        case RESISTOR:  return (V(e->node1) - V(e->node2)) / e->value;
        case CAPACITOR: return cplx(0, w) * e->value * (V(e->node1) - V(e->node2));
        case VCCS: {
            auto* g = static_cast<vccs*>(e);
            return g->value * (V(g->controlPos()) - V(g->controlNeg()));
        }
        case CCCS: {
            auto* f = static_cast<cccs*>(e);
            if (!f->controlElement()) return {0, 0};
            return f->value * x(ext + f->controlElement()->extraVariableIndex);
        }
        default: return {0, 0};
    }
}

PlotData SimulationRunner::runACSweep(const ACSweepSettings& s, const std::vector<OutputVariable>& vars) {
    PlotData plot;
    if (s.points < 1 || s.w_stop < s.w_start || (s.kind != ACSweepKind::Linear && s.w_start <= 0)) {
        std::cerr << "Error: Bad AC sweep range (log sweeps need w_start > 0)." << std::endl;
        return plot;
    }
    if (!prepare() || !checkOutputs(vars)) return plot;

    bool hasSource = false;
    for (Element* e : graph->getElements()) {
        if (e->type == SINUSOIDAL_SOURCE) hasSource = true;
    }
    if (!hasSource) {
        std::cerr << "Error: AC analysis needs at least one sinusoidal source (add S1 n1 n2 offset amp freq)." << std::endl;
        return plot;
    }

    startPlot(plot, vars);
    plot.x_label = "w (rad/s)";
    plot.log_x = (s.kind != ACSweepKind::Linear);

    for (double w : makeACGrid(s.kind, s.w_start, s.w_stop, s.points)) {
        Eigen::VectorXcd x = solveACOnce(w);
        if (x.size() == 0) {
            std::cerr << "Error: AC matrix is singular at w = " << w << std::endl;
            break;
        }
        plot.time_axis.push_back(w);
        for (size_t k = 0; k < vars.size(); ++k) {
            plot.data_series[k].push_back(std::abs(getPhasorAt(vars[k], x, w)));
        }
    }
    return plot;
}

PlotData SimulationRunner::runPhaseSweep(const PhaseSweepSettings& s, const std::vector<OutputVariable>& vars) {
    PlotData plot;
    if (!prepare() || !checkOutputs(vars)) return plot;

    Element* src = nullptr;
    for (Element* e : graph->getElements()) {
        if (e->type != SINUSOIDAL_SOURCE) continue;
        if (s.source.empty() || e->name == s.source) { src = e; break; }
    }
    if (!src) {
        std::cerr << "Error: Phase sweep needs a sinusoidal source"
                  << (s.source.empty() ? "" : " named " + s.source) << "." << std::endl;
        return plot;
    }

    startPlot(plot, vars);
    plot.x_label = "phase of " + src->name + " (deg)";

    int n = std::max(1, s.points);
    for (int i = 0; i < n; ++i) {
        double phi = (n == 1) ? s.phi_start : s.phi_start + i * (s.phi_stop - s.phi_start) / double(n - 1);
        Eigen::VectorXcd x = solveACOnce(s.w_base, src, phi);
        if (x.size() == 0) {
            std::cerr << "Error: AC matrix is singular." << std::endl;
            break;
        }
        plot.time_axis.push_back(phi);
        for (size_t k = 0; k < vars.size(); ++k) {
            plot.data_series[k].push_back(std::abs(getPhasorAt(vars[k], x, s.w_base)));
        }
    }
    return plot;
}

bool SimulationRunner::setElementValue(const std::string& name, double value) {
    Element* e = graph->findElement(name);
    if (!e) return false;
    e->setValue(value);
    return true;
}

void SimulationRunner::printTable(const PlotData& pd, std::ostream& out) {
    if (pd.empty()) return;
    out << std::left << std::setw(15) << pd.x_label << ' ';
    for (const auto& n : pd.series_names) out << std::setw(15) << n << ' ';
    out << "\n";

    std::ios oldState(nullptr);
    oldState.copyfmt(out);
    out << std::setprecision(6);
    for (size_t i = 0; i < pd.time_axis.size(); ++i) {
        out << std::setw(16) << pd.time_axis[i];
        for (const auto& s : pd.data_series) out << std::setw(16) << (i < s.size() ? s[i] : 0.0);
        out << "\n";
    }
    out.copyfmt(oldState);
    out << std::flush;
}
