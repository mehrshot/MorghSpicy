//
// Created by Mehrshad on 6/6/2025.
//

#include "SimulatorController.h"

#include <eigen3\Eigen/Dense>
#include <iomanip>

using namespace std;

SimulatorController::SimulatorController() {
    // Constructor initialization
}

SimulatorController::~SimulatorController() {
    // Destructor cleanup
}

void SimulatorController::setupExampleCircuit() {
    // Define Nodes
    Node* gnd = new Node(0, "GND");
    Node* n1 = new Node(1, "N1");
    Node* n2 = new Node(2, "N2");
    Node* n3 = new Node(3, "N3");

    circuitGraph.addNode(gnd);
    circuitGraph.addNode(n1);
    circuitGraph.addNode(n2);
    circuitGraph.addNode(n3);

    // Define circuit elements
    double R_val = 1000.0;
    double L_val = 1.0e-3;
    double C_val = 1.0e-6;
    double V1_val = 5.0;

    VoltageSource* V_src = new VoltageSource("Vs", 0, 1, V1_val);
    Resistor* R_test = new Resistor("R_test", 1, 2, R_val);
    Capacitor* C_test = new Capacitor("C_test", 2, 3, C_val);
    Inductor* L_test = new Inductor("L_test", 3, 0, L_val);
    CurrentSource* I1_src = new CurrentSource("I1", 1, 0, 0.001);

    circuitGraph.addElement(V_src);
    circuitGraph.addElement(R_test);
    circuitGraph.addElement(C_test);
    circuitGraph.addElement(L_test);
    circuitGraph.addElement(I1_src);

    cout << "Circuit setup complete. Elements:\n";
    circuitGraph.desplayGraph();
}

void SimulatorController::run() {
    cout << "Welcome to Morgh Spicy Circuit Simulator!\n";
    setupExampleCircuit();

    mnaSolver.initializeMatrix(circuitGraph);

    runTransientAnalysis();
    runParameterSweep();

    cout << "\nSimulation session ended.\n";
}

void SimulatorController::runTransientAnalysis() {
    cout << "\n--- Starting Transient Analysis ---" << endl;

    double Tstart = 0.0;
    double Tstop = 100.0e-6;
    double Tstep = 1.0e-6;

    Eigen::VectorXd prev_solution(mnaSolver.getNumNonGroundNodes() + mnaSolver.getNumVoltageSources() + mnaSolver.getNumInductors());
    prev_solution.setZero();

    if (mnaSolver.getTotalUnknowns() == 0) {
        cerr << "Transient analysis cannot run: MNASolver not initialized correctly or circuit is invalid." << endl;
        return;
    }

    cout << "Time (s)\tV(N1) (V)\tV(N2) (V)\tV(N3) (V)\tI(Vs) (A)\tI(L_test) (A)" << endl;

    for (double current_time = Tstart; current_time <= Tstop; current_time += Tstep) {
        mnaSolver.constructMNAMatrix(circuitGraph, Tstep, prev_solution);
        Eigen::VectorXd current_solution = mnaSolver.solve();

        if (current_solution.isZero(0)) {
            cerr << "Warning: Solution failed at time t = " << current_time << " s. Analysis stopped." << endl;
            break;
        }

        prev_solution = current_solution;

        cout.precision(6);
        cout << fixed;
        cout << current_time << "\t\t"
             << current_solution(mnaSolver.getNodeToMatrixIdxMap().at(1)) << "\t\t"
             << current_solution(mnaSolver.getNodeToMatrixIdxMap().at(2)) << "\t\t"
             << current_solution(mnaSolver.getNodeToMatrixIdxMap().at(3)) << "\t\t"
             << current_solution(mnaSolver.getExtraVariableStartIndex() + circuitGraph.findElement("Vs")->extraVariableIndex) << "\t\t"
             << current_solution(mnaSolver.getExtraVariableStartIndex() + circuitGraph.findElement("L_test")->extraVariableIndex)
             << endl;
    }
    cout << "\n--- Transient Analysis Complete ---" << endl;
}

void SimulatorController::runParameterSweep() {
    cout << "\n--- Starting Parameter Sweep Analysis ---" << endl;

    string param_name = "R_test";
    double start_value = 100.0;
    double end_value = 2000.0;
    double increment = 500.0;

    Element* swept_element = circuitGraph.findElement(param_name);
    if (!swept_element) {
        cerr << "Error: Element '" << param_name << "' not found for parameter sweep." << endl;
        return;
    }

    if (mnaSolver.getTotalUnknowns() == 0) {
        cerr << "Parameter sweep cannot run: MNASolver not initialized correctly or circuit is invalid." << endl;
        return;
    }

    cout << "Sweeping " << param_name << " from " << start_value << " to " << end_value << " with increment " << increment << endl;
    cout << param_name << " (Ohms)\tV(N1) (V)\tV(N2) (V)\tV(N3) (V)\tI(Vs) (A)" << endl;

    Eigen::VectorXd dc_prev_solution(mnaSolver.getNumNonGroundNodes() + mnaSolver.getNumVoltageSources() + mnaSolver.getNumInductors());
    dc_prev_solution.setZero();
    double dc_timestep_h = 1.0;

    for (double current_param_value = start_value; current_param_value <= end_value; current_param_value += increment) {
        swept_element->setValue(current_param_value);

        mnaSolver.constructMNAMatrix(circuitGraph, dc_timestep_h, dc_prev_solution);
        Eigen::VectorXd current_sweep_solution = mnaSolver.solve();

        if (current_sweep_solution.isZero(0)) {
            cerr << "Warning: Solution failed for " << param_name << " = " << current_param_value << " Ohms. Sweep stopped." << endl;
            break;
        }

        cout.precision(6);
        cout << fixed;
        cout << current_param_value << "\t\t"
             << current_sweep_solution(mnaSolver.getNodeToMatrixIdxMap().at(1)) << "\t\t"
             << current_sweep_solution(mnaSolver.getNodeToMatrixIdxMap().at(2)) << "\t\t"
             << current_sweep_solution(mnaSolver.getNodeToMatrixIdxMap().at(3)) << "\t\t"
             << current_sweep_solution(mnaSolver.getExtraVariableStartIndex() + circuitGraph.findElement("Vs")->extraVariableIndex)
             << endl;
    }
    cout << "\n--- Parameter Sweep Analysis Complete ---" << endl;
}
