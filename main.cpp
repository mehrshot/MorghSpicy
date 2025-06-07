#include <iostream>
#include <string>
#include <vector>
#include <map>

#include "Model/Node.h"
#include "Model/Elements.h"
#include "Model/Graph.h"
#include "Model/MNASolver.h"
#include "Model/NodeManager.h"
#include "Controller/CommandParser.h"
#include "Controller/SimulationRunner.h"

#include <eigen3/Eigen/Dense>

using namespace std;

int main() {
    // Graph circuitGraph;
    //
    // Node* gnd = new Node(0, "GND");
    // Node* n1 = new Node(1, "N1");
    // Node* n2 = new Node(2, "N2");
    // Node* n3 = new Node(3, "N3");
    //
    // circuitGraph.addNode(gnd);
    // circuitGraph.addNode(n1);
    // circuitGraph.addNode(n2);
    // circuitGraph.addNode(n3);
    //
    // double R_val = 1000.0;
    // double L_val = 1.0e-3;
    // double C_val = 1.0e-6;
    // double V1_val = 5.0;
    // double h = 1.0e-6;
    //
    // VoltageSource* V_src = new VoltageSource("Vs", 0, 1, V1_val);
    // Resistor* R_test = new Resistor("R_test", 1, 2, R_val);
    // Capacitor* C_test = new Capacitor("C_test", 2, 3, C_val);
    // Inductor* L_test = new Inductor("L_test", 3, 0, L_val);
    // CurrentSource* I1_src = new CurrentSource("I1", 1, 0, 0.001);
    //
    // circuitGraph.addElement(V_src);
    // circuitGraph.addElement(R_test);
    // circuitGraph.addElement(C_test);
    // circuitGraph.addElement(L_test);
    // circuitGraph.addElement(I1_src);
    //
    // circuitGraph.desplayGraph();
    //
    // MNASolver mnaSolver;
    //
    // double timestep_h = 1.0e-6;
    //
    // Eigen::VectorXd prev_solution;
    //
    // mnaSolver.initializeMatrix(circuitGraph);
    //
    // prev_solution.resize(mnaSolver.getNumNonGroundNodes() + mnaSolver.getNumVoltageSources() + mnaSolver.getNumInductors());
    // prev_solution.setZero();
    //
    // mnaSolver.constructMNAMatrix(circuitGraph, timestep_h, prev_solution);
    //
    // cout << "\n--- MNA Matrix constructed by general stamping ---" << endl;
    // mnaSolver.displayMatrix();
    //
    // Eigen::VectorXd result = mnaSolver.solve();
    //
    // mnaSolver.displaySolution();
    // mnaSolver.displayNodeVoltages();
    // mnaSolver.displayElementCurrents(circuitGraph);
    Graph graph;
    NodeManager nodeManager(&graph);
    MNASolver mnaSolver;
    SimulationRunner simRunner(&graph, &mnaSolver);
    CommandParser parser(&graph, &nodeManager, &simRunner);

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "exit") {
            break;
        }
        parser.parseCommand(line);
    }

    return 0;
}