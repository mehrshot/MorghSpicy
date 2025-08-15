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

public:
    SimulationRunner(Graph* g, MNASolver* s, NodeManager* n);

    PlotData runTransient(double t0, double tstop, double h,
                          const std::vector<OutputVariable>& vars);

    void runDCSweep(const std::string& elemName,
                    double start, double stop, double step,
                    const std::vector<OutputVariable>& vars);
};


#endif //MORGHSPICY_SIMULATIONRUNNER_H
