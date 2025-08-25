//
// Created by Mehrshad on 6/7/2025.
//

#ifndef MORGHSPICY_SIMULATIONRUNNER_H
#define MORGHSPICY_SIMULATIONRUNNER_H


#pragma once
#include "Model/Graph.h"
#include "Model/MNASolver.h"
#include "Model/NodeManager.h"
#include <string>
#include <vector>
#include <Eigen/Dense>
#include <complex>
#include <numbers>

enum class ACSweepKind { Linear, Decade, Octave };

struct ACSweepSettings {
    ACSweepKind kind;
    double w_start;
    double w_stop;
    int points;
};

struct PhaseSweepSettings {
    double w_base;
    double phi_start;
    double phi_stop;
    int points;
};


struct OutputVariable {
    // unscoped enum so you can write OutputVariable::VOLTAGE
    enum Type { VOLTAGE, CURRENT };
    Type        type{};
    std::string name;   // node label or element name
};

struct PlotData {
    std::vector<double>              time_axis;    // t
    std::vector<std::vector<double>> data_series;  // one vector per requested variable
    std::vector<std::string>         series_names; // "V(n1)", "I(R1)", ...
};

class SimulationRunner {
private:
    Graph*       graph{};
    MNASolver*   mnaSolver{};
    NodeManager* nm{};     // keep this name: .cpp mostly uses nm

    double calculate_element_current(
            Element* elem,
            const Eigen::VectorXd& solution_vector,
            const Eigen::VectorXd& prev_solution,
            double h
    );

    std::vector<double> makeACGrid(ACSweepKind kind,
                                   double w_start, double w_stop, int points) const;
    Eigen::VectorXcd solveACOnce(double w) const;

    std::complex<double> getPhasorAt(const OutputVariable& v,
                                     const Eigen::VectorXcd& x) const;

public:
    SimulationRunner(Graph* g, MNASolver* s, NodeManager* n);

    PlotData runTransient(double t0, double tstop, double h,
                          const std::vector<OutputVariable>& vars);

    void runDCSweep(const std::string& elemName,
                    double start, double stop, double step,
                    const std::vector<OutputVariable>& vars);

    PlotData runACSweep(const ACSweepSettings& s,
                        const std::vector<OutputVariable>& what);

    PlotData runPhaseSweep(const PhaseSweepSettings& s,
                           const std::vector<OutputVariable>& what);

};


#endif //MORGHSPICY_SIMULATIONRUNNER_H
