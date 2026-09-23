#include <iostream>
#include <string>
#include "View/App.h"

// text only mode, same commands as the GUI (this is how phase 1 worked)
static int runConsole(const char* netlist) {
    Graph graph;
    NodeManager nodeManager;
    MNASolver mnaSolver;
    SimulationRunner simRunner(&graph, &mnaSolver, &nodeManager);
    CommandParser parser(&graph, &nodeManager, &simRunner);

    if (netlist) parser.parseCommand(std::string("load ") + netlist);

    std::cout << "MorghSpicy console, type 'help' for commands or 'exit' to quit" << std::endl;
    std::string line;
    while (std::cout << ">>> " << std::flush, std::getline(std::cin, line)) {
        if (line == "exit" || line == "quit") break;
        parser.parseCommand(line);
    }
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "--cli") {
        return runConsole(argc > 2 ? argv[2] : nullptr);
    }

    App app;
    return app.run();
}
