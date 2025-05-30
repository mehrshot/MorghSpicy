//
// Created by Mehrshad on 5/29/2025.
//

#ifndef MORGHSPICY_MNASOLVER_H
#define MORGHSPICY_MNASOLVER_H

#include <bits/stdc++.h>
#include <eigen3/Eigen/Dense>

class Graph;
class Elements;

class MNASolver {
private:
    Eigen::MatrixXd A_matrix;        // The MNA matrix
    Eigen::VectorXd b_vector;        // The right-hand side vector of equations
    Eigen::VectorXd solution_vector; // The solved vector (node voltages and extra currents)

    int num_ground_nodes;
    int num_non_ground_nodes;
    int num_voltage_sources;
    int num_inductors;
    int total_unknowns;

    std::map<int, int> node_id_to_matrix_idx;

    int getExtraVariableStartIndex() const {
        return num_non_ground_nodes;
    }

public:
    MNASolver();
    ~MNASolver() = default;

    // 1. Matrix Initialization Method:
    void initializeMatrix(const Graph& circuitGraph);

    // 2. MNA Matrix Construction Method:
    void constructMNAMatrix(const Graph& circuitGraph, double timestep_h,
                            const Eigen::VectorXd& prev_solution);

    // 3. System Solving Method:
    Eigen::VectorXd solve();

    const Eigen::VectorXd& getSolution() const { return solution_vector; }
    int getNumNonGroundNodes() const { return num_non_ground_nodes; }
    int getNumVoltageSources() const { return num_voltage_sources; }
    int getNumInductors() const { return num_inductors; }
    const std::map<int, int>& getNodeToMatrixIdxMap() const { return node_id_to_matrix_idx; }

    // Display methods
    void displayMatrix() const;
    void displaySolution() const;
    void displayNodeVoltages() const;
    void displayElementCurrents(const Graph& circuitGraph) const;
};




#endif //MORGHSPICY_MNASOLVER_H
