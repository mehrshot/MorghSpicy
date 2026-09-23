//
// Created by Mehrshad on 5/29/2025.
//
#ifndef MORGHSPICY_MNASOLVER_H
#define MORGHSPICY_MNASOLVER_H

#include <Eigen/Dense>
#include <map>

class Graph;

class MNASolver {
private:
    Eigen::MatrixXd A_matrix;        // The MNA matrix
    Eigen::VectorXd b_vector;        // RHS
    Eigen::VectorXd solution_vector; // node voltages + extra currents

    int num_non_ground_nodes{};
    int num_extra_vars{};
    int total_unknowns{};

    std::map<int, int> node_id_to_matrix_idx;

    // tiny conductance from every node to ground so floating nodes don't make A singular
    double gmin = 1e-12;

public:
    MNASolver() = default;

    // returns false if the circuit can't be solved (no ground, empty, ...)
    bool initializeMatrix(const Graph& circuitGraph);

    // build A and b for one time step / newton iteration
    void constructMNAMatrix(const Graph& circuitGraph, double timestep_h,
                            const Eigen::VectorXd& prev_solution,
                            const Eigen::VectorXd& guess);

    // empty vector if the matrix is singular
    Eigen::VectorXd solve();

    const Eigen::VectorXd& getSolution() const { return solution_vector; }
    int getNumNonGroundNodes() const { return num_non_ground_nodes; }
    const std::map<int, int>& getNodeToMatrixIdxMap() const { return node_id_to_matrix_idx; }
    int getTotalUnknowns() const { return total_unknowns; }
    int getExtraVariableStartIndex() const { return num_non_ground_nodes; }

    void displayMatrix() const;
    void displaySolution() const;

    void setGmin(double g) { gmin = g; }
};

#endif //MORGHSPICY_MNASOLVER_H
