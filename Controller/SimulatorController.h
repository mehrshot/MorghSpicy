//
// Created by Mehrshad on 6/6/2025.
//

#ifndef MORGHSPICY_SIMULATORCONTROLLER_H
#define MORGHSPICY_SIMULATORCONTROLLER_H


#include "Model/Graph.h"
#include "Model/MNASolver.h"
#include "Model/Elements.h"

#include <iostream>
#include <vector>
#include <string>
#include <limits>

class SimulatorController {
private:
    Graph circuitGraph;
    MNASolver mnaSolver;

    void setupExampleCircuit();
    void runTransientAnalysis();
    void runParameterSweep();

public:
    SimulatorController();
    ~SimulatorController();

    void run();
};


#endif //MORGHSPICY_SIMULATORCONTROLLER_H
