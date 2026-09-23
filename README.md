# MorghSpicy

A small circuit simulator in C++, something like a mini LTspice. You can draw a circuit on a grid
(or type it as a netlist), then run transient, DC sweep, AC sweep and phase sweep analyses and look at
the results in a built in plot window.

It was our term project for the Object Oriented Programming course (Electrical Engineering,
Sharif University of Technology, spring 2025). Phase 1 was a text-only simulator, phase 2 added the
SDL3 GUI.

![schematic editor](docs/schematic_editor.png)

![voltage and current probes on the RLC circuit above](docs/transient_probe.png)

## Features

**Elements**
- R, L, C
- independent voltage / current sources (DC)
- `PULSE(...)` and `SIN(...)` voltage sources
- diode and zener diode (Newton-Raphson for the nonlinear part)
- dependent sources: VCVS (E), VCCS (G), CCVS (H), CCCS (F)
- subcircuits: save a part of a circuit as a two port block and reuse it

**Analyses**
- transient (backward Euler, fixed step with an optional max step)
- DC sweep of a voltage or current source
- AC sweep (linear, decade or octave), sinusoidal sources are the inputs
- phase sweep of a sinusoidal source at a fixed frequency

**GUI**
- schematic editor on a grid: place parts, wires and ground, edit values and names
- voltage probe (click a node) and current probe (click an element) plot directly
- plot window with legend, zoom / pan, one or two cursors (prints dx, dy and slope),
  hide / recolor / rename series, add or subtract two signals
- log frequency axis for AC sweeps
- command bar inside the window, same commands as the console
- load a sampled signal from a text file and show it on the scope

**Other**
- save / load circuits as text files
- named nodes, `connect` / `short`, `rename node`
- a console mode (`--cli`) without any window

## How it works (short version)

- **Modified Nodal Analysis.** Every element stamps itself into `A x = b`
  (`Element::stampMNA`). Voltage sources, inductors, E and H add a current variable, so current
  controlled sources can look up their control current directly.
- **Transient:** capacitors and inductors use their backward Euler companion models. If there is a
  diode, each time step runs Newton-Raphson until the solution stops changing.
- **DC:** the same stamps with a huge time step, so capacitors act open and inductors act shorted.
- **AC:** a separate complex matrix (`SimulationRunner::solveACOnce`). DC sources are zeroed and the
  sinusoidal sources become phasors.
- **Nodes:** `NodeManager` keeps labels like `N3_4` or `vdd` and a union-find for things that are
  shorted together (wires, GND). Right before solving, every element is moved to the root of its set.
- Code is split roughly like MVC: `Model/` (elements, graph, solver), `Controller/` (command parser,
  analyses), `View/` (SDL windows).

## Building

You need a C++20 compiler, CMake 3.24+, **SDL3**, **SDL3_ttf** and **Eigen3**.

### Windows (MSYS2, what we used)

In an MSYS2 UCRT64 terminal:

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-sdl3 mingw-w64-ucrt-x86_64-sdl3-ttf mingw-w64-ucrt-x86_64-eigen3
```

```bash
cmake -S . -B build -G Ninja
```

```bash
cmake --build build
```

```bash
./build/MorghSpicy.exe
```

CLion works too: in *Settings > Build, Execution, Deployment > Toolchains* add a MinGW toolchain with
`C:\msys64\ucrt64` as its path and move it to the top, then *Tools > CMake > Reset Cache and Reload Project*.
(An old `C:\MinGW` install is 32 bit and can't build this.)
The build copies the font, the example circuits and the SDL dlls next to the exe. If you start it from
Explorer and it complains about a missing dll, run it from the MSYS2 terminal or add
`C:\msys64\ucrt64\bin` to `PATH`.

### Linux / macOS

Install SDL3, SDL3_ttf and Eigen3 with your package manager (`brew install sdl3 sdl3_ttf eigen` on
macOS; on Linux, SDL3 may have to be built from source if your distro doesn't ship it yet). Then build
with the same two cmake commands and run `./build/MorghSpicy`.

## Using it

### Schematic editor (the first page)

| Key / mouse | What it does |
|---|---|
| `W` `R` `C` `L` `V` `I` `D` | wire, resistor, capacitor, inductor, voltage source, current source, diode |
| `G` | ground (single click) |
| left click, left click | place the part between two grid points (it snaps to horizontal / vertical) |
| click on a part | select it |
| double click or `E` | edit the value (diode: `D` or `Z`) |
| `N` | rename the selected part |
| `Del` / `Backspace` | delete the selected part |
| right click / `Esc` | cancel |
| `P` then click a node | plot the node voltage |
| `O` then click a part | plot the current through it |
| `F2` | go to the plot / command page |

The first click of a voltage source is its `+` side, and the arrow of a current source points from the
first click to the second. Probes use the transient settings from the `tran` command
(10 us / 50 ms by default).

### Plot page

Press `Enter` to type a command, `Enter` again to run it (`Up` / `Down` for history).

| Key | What it does |
|---|---|
| `1`-`9` | select a series |
| `V` / `Del` / `B` | hide, remove, change color of the selected series |
| `G` | show / hide legend |
| mouse wheel, `[` `]` `-` `=` | zoom |
| drag (right button while the cursor is on) | pan |
| `A` | auto zoom again |
| `C` / `D` | cursor on/off, second cursor on/off (values are printed in the console) |
| `M` / `N` / `K` | last two series: sum, difference; last series times 2 |
| `X` | clear the plot |
| `L` | load the demo signal from `assets/signal_demo.txt` (`S` toggles reading only the first chunk) |

Extra commands on this page: `clear`, `relabel <index> <name>`, `grid`.

### Commands

The same commands work in the GUI command bar and in the console (`MorghSpicy --cli [file]`).
Values accept the usual suffixes: `f p n u m k meg g t` (`1k`, `4.7u`, `2meg`).

```text
add R1 n1 n2 1k                    R, C, L, V, I
add D1 anode cathode D             D = normal diode, Z = zener (5.1 V)
add V1 in 0 PULSE(0 5 0 1u 1u 5m 10m)      v1 v2 delay rise fall width period
add V2 in 0 SIN(0 10 50 0)         offset amplitude freq(Hz) [phase(deg)]
add S1 in 0 0 10 50                same sine source, old syntax
add E1 out 0 cp cn 10              VCVS (G1 ... for VCCS)
add H1 out 0 Vsense 100            CCVS, controlled by the current of Vsense (F1 ... for CCCS)
add GND n1                         connect n1 to ground (0 and gnd are ground too)
connect a b                        short two nodes
label 3 vout / rename node a b
delete R1  |  value R1 2k  |  list [R]  |  .nodes  |  reset

print TRAN <tstep> <tstop> [<tmaxstep>] V(n1) I(R1) ...
print DC <source> <start> <stop> <step> V(n1) ...
ac <lin|dec|oct> <w_start> <w_stop> <points> V(n1) ...      (w in rad/s)
phase <w> <phi_start> <phi_stop> <points> [source] V(n1) ...
tran <tstep> <tstop> [<tmaxstep>]                  settings used by plot / probes
plot v <node> [node2]  |  plot i <element>

save my_circuit.txt  |  load my_circuit.txt  |  show existing schematics  |  load 2
subcircuit create Divider from p q    then    add X1 in out Divider
scope load path=assets/signal_demo.txt Fs=10k t=0.1  |  scope clear
help
```

`print` commands show a table in the console and also plot the result when the GUI is open.

### Circuit files

Saved circuits are plain text, one element per line, with the type letter first (`*` or `#` start a
comment). Any normal command can be in the file too:

```text
* RC charging, tau = 10ms
V V1 N1 0 5
R R1 N1 N2 1k
C C1 N2 0 10u
```

Some examples are in [`schematics/`](schematics):

| File | Try |
|---|---|
| `rc_step.txt` | `print TRAN 1m 50m V(N2) I(C1)` |
| `rlc_pulse.txt` | `print TRAN 10u 30m V(in) V(out)` |
| `half_wave_rectifier.txt` | `print TRAN 50u 60m V(in) V(out)` |
| `rc_lowpass_ac.txt` | `ac dec 10 1meg 200 V(out)` |
| `dependent_sources.txt` | `print DC V1 0 2 0.5 V(a) V(c) I(F1)` |

![AC sweep of the RC low pass](docs/ac_sweep.png)

![half wave rectifier with two cursors](docs/rectifier_cursors.png)

## Project structure

```text
main.cpp                  GUI by default, --cli for the console
Model/
  Elements.h/.cpp         all element classes and their MNA stamps
  Graph.h/.cpp            the element list, connectivity check
  NodeManager.h/.cpp      node names + union-find for shorted nodes
  MNASolver.h/.cpp        builds and solves A x = b (Eigen)
Controller/
  CommandParser.h/.cpp    every text command, save / load, subcircuits
  SimulationRunner.h/.cpp transient, DC, AC and phase analyses
  Signal.h/.cpp           reads sampled signals from text files
  Exceptions.h
View/
  App.h/.cpp              window, pages, command bar
  CircuitGrid.h/.cpp      schematic editor
  Plotter.h/.cpp          plot window
assets/                   font and a demo signal
schematics/               example circuits
```

## Limitations

- fixed time step only (no automatic step control), and the transient always starts from zero
  (empty capacitors, no inductor current)
- the diode is treated as open in AC analysis (no small signal model)
- AC and phase sweeps show magnitude only
- the grid editor and the netlist commands are two separate ways in: a circuit loaded from a file is
  not drawn on the grid
- subcircuits can only hold R, L, C, V, I and diodes

## Team

- Mehrshad ([@mehrshot](https://github.com/mehrshot))
- Ali
- ER
