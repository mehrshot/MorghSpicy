//
// Created by Mehrshad on 6/7/2025.
//

#ifndef MORGHSPICY_SIMULATIONRUNNER_H
#define MORGHSPICY_SIMULATIONRUNNER_H


#include "Model/Graph.h"
#include "Model/MNASolver.h"
#include <string>
#include <vector>

// A struct to hold information about a requested output variable
struct OutputVariable {
    enum VarType { VOLTAGE, CURRENT };
    VarType type;
    std::string name; // Node name for voltage, element name for current
};

class SimulationRunner {
private:
    Graph* graph;
    MNASolver* mnaSolver;

public:
    SimulationRunner(Graph* g, MNASolver* solver);

    void runTransient(double tstep_initial, double tstop, double tmaxstep, const std::vector<OutputVariable>& requested_vars);

    void runDCSweep(const std::string& sourceName, double start, double stop, double increment, const std::vector<OutputVariable>& requested_vars);

private:
    double calculate_element_current(Element* elem, const Eigen::VectorXd& solution_vector, const Eigen::VectorXd& prev_solution, double h);
};


#endif //MORGHSPICY_SIMULATIONRUNNER_H
