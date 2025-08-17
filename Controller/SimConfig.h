//
// Created by Mehrshad on 8/11/2025.
//

#ifndef MORGHSPICY_SIMCONFIG_H
#define MORGHSPICY_SIMCONFIG_H


#pragma once
#include <string>
#include <vector>

enum class SweepType { Octave, Decade, Linear };

struct TransientConfig {
    double t_stop   = 0.05;   // s
    double t_start  = 0.0;    // s (time to start saving)
    double dt_max   = 1e-3;   // s
    double dt_init  = 1e-5;   // s
};

struct ACSweepConfig {
    SweepType type  = SweepType::Decade;  // Octave/Decade/Linear
    double w_start  = 2.0 * 3.141592653589793 * 1.0;   // rad/s
    double w_stop   = 2.0 * 3.141592653589793 * 1e3;   // rad/s
    int    N        = 101;   // number of points (inclusive)
    bool   out_in_dB   = true;   // |H| in dB
    bool   phase_in_deg = true;
};

struct PhaseSweepConfig {
    double w0       = 2.0 * 3.141592653589793 * 60.0;  // base rad/s
    double phi_start_deg = 0.0;
    double phi_stop_deg  = 360.0;
    int    N        = 73;    // 5-degree steps by default
    std::string source_name = "V1"; // which source’s phase to sweep
};

// What to plot
enum class OutputKind { Voltage, Current };
struct OutputVariable {
    OutputKind kind;
    std::string id; // "N2" for V, "R1" for I(element)
};



#endif //MORGHSPICY_SIMCONFIG_H
