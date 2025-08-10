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

// Defines a single output variable to be plotted (e.g., V(N1) or I(R1)).
struct OutputVariable {
    enum VarType { VOLTAGE, CURRENT };
    VarType type;
    std::string name;
};

// Holds all data required for rendering a complete plot.
struct PlotData {
    std::vector<double> time_axis;                  // X-axis values
    std::vector<std::vector<double>> data_series;   // One or more Y-axis data series
    std::vector<std::string> series_names;          // Legend names for each data series
};


class SimulationRunner {
private:
    Graph* graph;
    MNASolver* mnaSolver;
    NodeManager* nodeManager;
    double calculate_element_current(Element* elem, const Eigen::VectorXd& solution_vector, const Eigen::VectorXd& prev_solution, double h);

public:
    SimulationRunner(Graph* g, MNASolver* solver, NodeManager* nm);

    // Runs a transient analysis and returns the results packaged for plotting.
    PlotData runTransient(double tstep_initial, double tstop, double tmaxstep, const std::vector<OutputVariable>& requested_vars);

    // Runs a DC sweep analysis.
    void runDCSweep(const std::string& sourceName, double start, double stop, double increment, const std::vector<OutputVariable>& requested_vars);
};


#endif //MORGHSPICY_SIMULATIONRUNNER_H
