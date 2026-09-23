//
// Created by Mehrshad on 6/7/2025.
//

#ifndef MORGHSPICY_SIMULATIONRUNNER_H
#define MORGHSPICY_SIMULATIONRUNNER_H

#include "Model/Graph.h"
#include "Model/MNASolver.h"
#include "Model/NodeManager.h"
#include <string>
#include <vector>
#include <optional>
#include <complex>
#include <iosfwd>
#include <Eigen/Dense>

enum class ACSweepKind { Linear, Decade, Octave };

struct ACSweepSettings {
    ACSweepKind kind = ACSweepKind::Decade;
    double w_start = 1.0;     // rad/s
    double w_stop = 1e3;      // rad/s
    int points = 100;
};

struct PhaseSweepSettings {
    double w_base = 1.0;      // rad/s
    double phi_start = 0.0;   // degrees
    double phi_stop = 360.0;
    int points = 73;
    std::string source;       // empty = first sinusoidal source
};

struct OutputVariable {
    enum Type { VOLTAGE, CURRENT };
    Type        type{};
    std::string name;   // node name or element name
};

struct PlotData {
    std::vector<double>              time_axis;    // x axis (time, sweep value, w or phase)
    std::vector<std::vector<double>> data_series;  // one vector per requested variable
    std::vector<std::string>         series_names; // "V(n1)", "I(R1)", ...
    std::string x_label = "t (s)";
    bool log_x = false;

    bool empty() const { return time_axis.empty(); }
};

class SimulationRunner {
private:
    Graph*       graph{};
    MNASolver*   mnaSolver{};
    NodeManager* nm{};
    bool hasNonlinear = false;

    bool prepare();
    bool checkOutputs(const std::vector<OutputVariable>& vars) const;
    bool solvePoint(double h, const Eigen::VectorXd& prev, Eigen::VectorXd& x);

    double nodeVoltage(const Eigen::VectorXd& x, int node_id) const;
    double calculate_element_current(Element* elem, const Eigen::VectorXd& x,
                                     const Eigen::VectorXd& prev, double h) const;
    double valueOf(const OutputVariable& v, const Eigen::VectorXd& x,
                   const Eigen::VectorXd& prev, double h) const;

    std::vector<double> makeACGrid(ACSweepKind kind, double w_start, double w_stop, int points) const;
    Eigen::VectorXcd solveACOnce(double w, const Element* phaseSrc = nullptr, double phaseDeg = 0.0) const;
    std::complex<double> getPhasorAt(const OutputVariable& v, const Eigen::VectorXcd& x, double w) const;

    static void startPlot(PlotData& pd, const std::vector<OutputVariable>& vars);

public:
    SimulationRunner(Graph* g, MNASolver* s, NodeManager* n);

    PlotData runTransient(double tstep, double tstop, double tmaxstep,
                          const std::vector<OutputVariable>& vars);

    PlotData runDCSweep(const std::string& elemName,
                        double start, double stop, double step,
                        const std::vector<OutputVariable>& vars);

    PlotData runACSweep(const ACSweepSettings& s,
                        const std::vector<OutputVariable>& vars);

    PlotData runPhaseSweep(const PhaseSweepSettings& s,
                           const std::vector<OutputVariable>& vars);

    bool setElementValue(const std::string& name, double value);

    static void printTable(const PlotData& pd, std::ostream& out);
};

#endif //MORGHSPICY_SIMULATIONRUNNER_H
