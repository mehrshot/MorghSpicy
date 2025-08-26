// Created by Ali-Er on 6/6/2025.

#include "CommandParser.h"
#include "Model/Graph.h"
#include "Model/NodeManager.h"
#include "Controller/SimulationRunner.h"
#include <sstream>
#include <iostream>
#include <fstream>
#include <map>
#include <regex>
#include <fstream>
#include <unordered_set>
#include <filesystem>

namespace {
    static std::vector<OutputVariable> currentOutputs() {
        return { { OutputVariable::VOLTAGE, std::string("N2") } };
    }

    static void appPlot(const PlotData& pd, const std::vector<OutputVariable>& outs) {
        std::cout << "[Plot] points=" << pd.time_axis.size()
                  << ", series=" << pd.data_series.size() << std::endl;
    }
}

CommandParser::CommandParser() = default;

CommandParser::CommandParser(Graph* g, NodeManager* n, SimulationRunner* r)
        : graph(g), nodeManager(n), simRunner(r) {}

bool isNumber(const std::string& s) {
    std::regex pattern(R"(^[-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?$)");
    return std::regex_match(s, pattern);
}

double parseValueWithPrefix(const std::string& str) {
    try {
        double factor = 1.0;
        std::string numPart = str;

        char last = tolower(str.back());

        switch (last) {
            case 'k': factor = 1e3; numPart = str.substr(0, str.size()-1); break;
            case 'm': factor = 1e-3; numPart = str.substr(0, str.size()-1); break;
            case 'u': factor = 1e-6; numPart = str.substr(0, str.size()-1); break;
            case 'n': factor = 1e-9; numPart = str.substr(0, str.size()-1); break;
            case 'p': factor = 1e-12; numPart = str.substr(0, str.size()-1); break;
            case 'g': factor = 1e9; numPart = str.substr(0, str.size()-1); break;
            case 'h': factor = 1e2; numPart = str.substr(0, str.size()-1); break;
            case 't': factor = 1e12; numPart = str.substr(0, str.size()-1); break;
            default:
                if (isNumber(str)) return std::stod(str);
                break;
        }

        return std::stod(numPart) * factor;
    } catch (...) {
        return -1e99; // خطا
    }
}

void CommandParser::parseCommandCore(const std::string& line) {
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    if (cmd == "add") {
        std::string name;
        if (!(iss >> name)) {
            std::cerr << "Error: Syntax error\n";
            return;
        }
        std::string subcircuit_path = "library/" + name + ".sub";
        std::ifstream sub_file(subcircuit_path);

        if (sub_file.is_open()) {
            sub_file.close(); // We only needed to check for existence
            std::string n1_str, n2_str;
            if (!(iss >> n1_str >> n2_str)) {
                std::cerr << "Error: Syntax for adding subcircuit is 'add <InstanceName> <Node1> <Node2>'" << std::endl;
                return;
            }
            // Call the helper function to perform the flattening logic
            handleAddComponent(name, n1_str, n2_str);
            return; // IMPORTANT: Exit the function after handling the subcircuit
        }

        char type = name[0];

        if (name == "GND") {
            std::string node;
            if (!(iss >> node)) {
                std::cerr << "Error: Syntax error\n";
                return;
            }
            nodeManager->assignNodeAsGND(node);
//            std::cout << "Node " << node << " assigned as GND\n";
            return;
        }

        for (Element* e : graph->getElements()) {
            if (e->name == name) {
                std::cerr << "Error: " << name << " already exists in the circuit\n";
                return;
            }
        }

        std::string n1_str, n2_str;
        if (!(iss >> n1_str >> n2_str)) {
            std::cerr << "Error: Syntax error\n";
            return;
        }
        std::string next_token;
        // Get the current position in the stream
        std::streampos original_pos = iss.tellg();

        if (iss >> next_token && next_token == "PULSE") {
            std::string params_str;
            std::getline(iss, params_str); // Read the rest of the line: (v1 v2 td tr tf pw per)

            // Remove parentheses from the parameters string
            size_t first = params_str.find('(');
            size_t last = params_str.rfind(')');
            if (first == std::string::npos || last == std::string::npos) {
                std::cerr << "Error: Malformed PULSE parameters. Missing parentheses." << std::endl;
                return;
            }
            params_str = params_str.substr(first + 1, last - first - 1);

            std::istringstream param_stream(params_str);
            std::vector<std::string> param_tokens;
            std::string tok;
            while (param_stream >> tok) {
                param_tokens.push_back(tok);
            }

            // minimal whitespace split (local helper is fine too)
            auto split_ws = [](const std::string& s){
                std::vector<std::string> out; std::string w; std::istringstream iss(s);
                while (iss >> w) out.push_back(w);
                return out;
            };

            std::vector<std::string> parts = split_ws(line);
            if (!parts.empty() && parts[0] == "scope") {
                // scope load path=<file> Fs=10000 t=0.1 chunk=8192
                // scope clear
                if (parts.size() >= 2 && parts[1] == "load") {
                    std::string path;
                    double Fs = 1000.0, tStop = 1.0;
                    int chunk = 4096;
                    for (size_t i = 2; i < parts.size(); ++i) {
                        auto& s = parts[i];
                        auto eq = s.find('=');
                        if (eq == std::string::npos) continue;
                        auto k = s.substr(0, eq);
                        auto v = s.substr(eq + 1);
                        if (k == "path") path = v;
                        else if (k == "Fs") Fs = std::stod(v);
                        else if (k == "t") tStop = std::stod(v);
                        else if (k == "chunk") chunk = std::stoi(v);
                    }
                    // hook this into App if you already added callbacks; otherwise just echo:
                    std::cout << "[scope] load path=" << path << " Fs=" << Fs
                              << " t=" << tStop << " chunk=" << chunk << "\n";
                    return; // prevent the rest of your parser from re-handling "scope"
                }
                if (parts.size() >= 2 && parts[1] == "clear") {
                    std::cout << "[scope] clear\n";
                    return;
                }
            }


            if (param_tokens.size() != 7) {
                std::cerr << "Error: Incorrect PULSE parameters. Expected: PULSE(v1 v2 td tr tf pw per)" << std::endl;
                return;
            }

            double v1 = parseValueWithPrefix(param_tokens[0]);
            double v2 = parseValueWithPrefix(param_tokens[1]);
            double td = parseValueWithPrefix(param_tokens[2]);
            double tr = parseValueWithPrefix(param_tokens[3]);
            double tf = parseValueWithPrefix(param_tokens[4]);
            double pw = parseValueWithPrefix(param_tokens[5]);
            double per = parseValueWithPrefix(param_tokens[6]);

            int n1 = nodeManager->resolveId(n1_str);
            int n2 = nodeManager->resolveId(n2_str);

            graph->addElement(new PulseSource(name, n1, n2, v1, v2, td, tr, tf, pw, per));
            std::cout << "Added pulse source: " << name << std::endl;
            return; // Important: Exit after handling the pulse source
        }
        else {
            // If it wasn't a PULSE source, go back to the original position in the stream
            iss.seekg(original_pos);
        }


        if (type == 'D') {
            std::string model;
            if (!(iss >> model)) {
                std::cerr << "Error: Syntax error\n";
                return;
            }
            static std::unordered_set<std::string> validModels = {"D", "Z"};
            if (!validModels.count(model)) {
                std::cerr << "Error: Model " << model << " not found in library\n";
                return;
            }
            int n1 = nodeManager->resolveId(n1_str);
            int n2 = nodeManager->resolveId(n2_str);
            graph->addElement(new Diode(name, n1, n2, model));
            std::cout << "Added diode: " << name << std::endl;
            return;
        }

        if (type == 'G' || type == 'E') {
            std::string c1_str, c2_str, gain_str;
            if (!(iss >> c1_str >> c2_str >> gain_str)) {
                std::cerr << "Error: Syntax error for VCCS/VCVS\n";
                return;
            }
            double gain = parseValueWithPrefix(gain_str);
            if (gain == -1e99) {
                std::cerr << "Error: Invalid gain value\n";
                return;
            }
            int n1 = nodeManager->resolveId(n1_str);
            int n2 = nodeManager->resolveId(n2_str);
            int c1 = nodeManager->resolveId(c1_str);
            int c2 = nodeManager->resolveId(c2_str);
            Element* e = nullptr;
            if (type == 'G') {
                e = new vccs(name, n1, n2, c1, c2, gain);
            } else {
                e = new vcvs(name, n1, n2, c1, c2, gain);
            }
            graph->addElement(e);
            std::cout << "Added element: " << name << std::endl;
            return;
        }

        if (type == 'F' || type == 'H') {
            std::string ctrl_name, gain_str;
            if (!(iss >> ctrl_name >> gain_str)) {
                std::cerr << "Error: Syntax error for CCCS/CCVS\n";
                return;
            }
            double gain = parseValueWithPrefix(gain_str);
            if (gain == -1e99) {
                std::cerr << "Error: Invalid gain value\n";
                return;
            }
            int n1 = nodeManager->resolveId(n1_str);
            int n2 = nodeManager->resolveId(n2_str);
            Element* e = nullptr;
            if (type == 'F') {
                e = new cccs(name, n1, n2, ctrl_name, gain);
            } else {
                e = new ccvs(name, n1, n2, ctrl_name, gain);
            }
            graph->addElement(e);
            std::cout << "Added element: " << name << std::endl;
            return;
        }
        if (type == 'S') { // Sinusoidal source
            std::string voffset_str, vamp_str, freq_str, phase_str;
            if (!(iss >> voffset_str >> vamp_str >> freq_str)) {
                std::cerr << "Error: Syntax error for Sinusoidal Source. Usage: add Sname node1 node2 Voffset Vamp Freq [Phase]" << std::endl;
                return;
            }

            double voffset = parseValueWithPrefix(voffset_str);
            double vamp = parseValueWithPrefix(vamp_str);
            double freq = parseValueWithPrefix(freq_str);
            double phase = 0.0;

            std::string optional;
            if (iss >> optional) {
                phase = parseValueWithPrefix(optional);
            }

            int n1 = nodeManager->resolveId(n1_str);
            int n2 = nodeManager->resolveId(n2_str);

            Element* e = new SinusoidalSource(name, n1, n2, voffset, vamp, freq, phase);
            graph->addElement(e);
            std::cout << "Added sinusoidal source: " << name << std::endl;
            return;
        }


        std::string value_str;
        if (!(iss >> value_str)) {
            std::cerr << "Error: Syntax error (missing value)\n";
            return;
        }
        float posX = 100.0f, posY = 100.0f; // Default position if not provided
        std::string x_str, y_str;
        if (iss >> x_str >> y_str) {
            try {
                posX = std::stof(x_str);
                posY = std::stof(y_str);
            } catch(...) {
                // If conversion fails, just ignore and use the default position
            }
        }

        double value = parseValueWithPrefix(value_str);

        if ((type != 'V' && type != 'I' && value <= 0) || value == -1e99) {
            std::string typeName = "Value";
            if (type == 'R') typeName = "Resistance";
            else if (type == 'C') typeName = "Capacitance";
            else if (type == 'L') typeName = "Inductance";
            std::cerr << "Error: " << typeName << " cannot be zero or negative\n";
            return;
        }


        int n1 = nodeManager->resolveId(n1_str);
        int n2 = nodeManager->resolveId(n2_str);
        Element* e = nullptr;
        switch (type) {
            case 'R': e = new Resistor(name, n1, n2, value); break;
            case 'C': e = new Capacitor(name, n1, n2, value); break;
            case 'L': e = new Inductor(name, n1, n2, value); break;
            case 'V': e = new VoltageSource(name, n1, n2, value); break;
            case 'I': e = new CurrentSource(name, n1, n2, value); break;
            default:
                std::cerr << "Error: Element " << name << " not found in library\n";
                return;
        }
        graph->addElement(e);
        std::cout << "Added element: " << name << std::endl;


    } else if (cmd == "delete") {
        std::string name;
        if (!(iss >> name)) {
            std::cerr << "Error: Syntax error\n";
            return;
        }
        if (!graph->removeElementByName(name)) {
            std::cerr << "Error: Cannot delete " << name << "; component not found\n";
        }
    } else if (cmd == "list") {
        std::string type_filter;
        if (iss >> type_filter) {
            graph->displayElementsByType(type_filter);
        } else {
            graph->desplayGraph();
        }
    } else if (cmd == ".nodes") {
        nodeManager->displayNodes();
    } else if (cmd == "rename") {
        std::string sub_cmd, old_name, new_name;
        if (!(iss >> sub_cmd >> old_name >> new_name) || sub_cmd != "node") {
            std::cerr << "ERROR: Invalid syntax. correct format: rename node <old_name> <new_name>" << std::endl;
            return;
        }
        nodeManager->renameNode(old_name, new_name);

    }else if (cmd == "load") {
        std::string filepath;
        if (!(iss >> filepath)) {
            std::cerr << "Error: Syntax error. Usage: load <file_path>" << std::endl;
            return;
        }

        std::ifstream infile(filepath);
        if (!infile.is_open()) {
            std::cerr << "Error: Cannot open file: " << filepath << std::endl;
            return;
        }

        std::cout << "Loading circuit from file: " << filepath << std::endl;

        std::string file_line;
        while (std::getline(infile, file_line)) {
            if (file_line.empty() || file_line[0] == '*' || file_line[0] == '#') {
                continue;
            }

            std::istringstream line_stream(file_line);
            std::string type, name, n1, n2, val;

            if (!(line_stream >> type >> name >> n1 >> n2 >> val)) {
                std::cerr << "Error: Malformed line in file: " << file_line << std::endl;
                continue;
            }

            std::string command_to_process = "add " + name + " " + n1 + " " + n2 + " " + val;

            parseCommand(command_to_process);
        }
        std::cout << std::flush;
        std::cout << "Finished loading from file." << std::endl;
    }
    else if (cmd == "show") {
        std::string token1, token2;
        if (iss >> token1 >> token2 && token1 == "existing" && token2 == "schematics") {
            handleShowSchematics();
        } else {
            std::cerr << "Error: Unknown command. Did you mean 'show existing schematics'?" << std::endl;
        }

    }
    else if (cmd == "print") {
        handlePrintCommand(iss);
    }
    else if (cmd == "save") {
        handleSaveCommand(iss);
    }
    else if (cmd == "subcircuit") {
        std::string action, subName, from_keyword, n1_str, n2_str;
        if (!(iss >> action >> subName >> from_keyword >> n1_str >> n2_str) || action != "create" || from_keyword != "from") {
            std::cerr << "Error: Syntax error. Usage: subcircuit create <SubName> from <Node1> <Node2>" << std::endl;
            return;
        }
        std::filesystem::path library_dir = "library";
        if (!std::filesystem::exists(library_dir)) {
            std::filesystem::create_directory(library_dir);
        }

        // Define the output file path
        std::string sub_path = library_dir.string() + "/" + subName + ".sub";
        std::ofstream outfile(sub_path);

        if (!outfile.is_open()) {
            std::cerr << "Error: Could not open file for writing: " << sub_path << std::endl;
            return;
        }

        // --- Main Saving Logic ---

        // 1. Write the 'ports' line to the file.
        // The node names provided by the user become the internal port names for the subcircuit.
        outfile << "ports " << n1_str << " " << n2_str << std::endl;

        // 2. Iterate through all elements in the current graph and write them to the file.
        for (const auto& elem : graph->getElements()) {
            // We need the string names of the nodes for saving, not their numeric IDs.
            std::string node1_name = nodeManager->nameOf(elem->node1);
            std::string node2_name = nodeManager->nameOf(elem->node2);

            // If a node doesn't have a label, use its numeric ID as its name.
            if (node1_name.empty()) node1_name = std::to_string(elem->node1);
            if (node2_name.empty()) node2_name = std::to_string(elem->node2);

            // Write the element's definition in the "add <name> <n1> <n2> <value/model>" format.
            outfile << "add " << elem->name << " " << node1_name << " " << node2_name << " ";

            if (elem->type == DIODE) {
                outfile << dynamic_cast<Diode*>(elem)->model << std::endl;
            } else {
                // For now, we save the simple value. More complex sources would need more logic.
                outfile << elem->value << std::endl;
            }
        }

        outfile.close();
        std::cout << "SUCCESS: Subcircuit '" << subName << "' saved to " << sub_path << std::endl;
    }

    else if (cmd == "ac") { handleAC(iss); }
    else if (cmd == "phase") { handlePhase(iss); }




    else {
        std::cerr << "Error: Unknown command: " << cmd << std::endl;
    }

    }

void CommandParser::parseCommand(const std::string& line) {
    auto split_ws = [](const std::string& s){
        std::vector<std::string> out; out.reserve(8);
        std::istringstream iss(s); std::string w;
        while (iss >> w) out.push_back(w);
        return out;
    };

    std::istringstream in(line);
    std::string cmd;
    if (!(in >> cmd)) return;

    if (cmd == "label") { handleLabel(in); return; }
    if (cmd == "connect" || cmd == "short") { handleConnect(in); return; }

    const auto parts = split_ws(line);
    if (!parts.empty() && parts[0] == "scope") {
        if (parts.size() >= 2 && parts[1] == "load") {
            std::string path; double Fs = 10000.0, tStop = 0.1; int chunk = 8192;
            for (size_t i = 2; i < parts.size(); ++i) {
                auto& tok = parts[i];
                auto eq = tok.find('=');
                if (eq == std::string::npos) continue;
                std::string k = tok.substr(0, eq), v = tok.substr(eq+1);
                if (k == "path") path = v;
                else if (k == "Fs") Fs = parseValueWithPrefix(v);
                else if (k == "t") tStop = parseValueWithPrefix(v);
                else if (k == "chunk") chunk = (int)parseValueWithPrefix(v);
            }
            if (onScopeLoad) onScopeLoad(path, Fs, tStop, chunk);
            return;
        }
        if (parts.size() >= 2 && parts[1] == "clear") {
            if (onScopeClear) onScopeClear();
            return;
        }
    }

    if (cmd == "plot") {
        std::string what; if (!(in >> what)) { return; }
        for (auto& c : what) c = char(std::tolower(c));

        double tstep = 1e-5, tstop = 0.05, tmax = 1e-3;

        if (what == "v" || what == "voltage") {
            std::string npos, nneg;
            if (!(in >> npos)) { return; }
            std::vector<OutputVariable> vars;
            vars.push_back({OutputVariable::VOLTAGE, npos});
            if (in >> nneg) vars.push_back({OutputVariable::VOLTAGE, nneg});
            PlotData pd = simRunner->runTransient(tstep, tstop, tmax, vars);
            if (onPlotData) onPlotData(pd);
            return;
        }

        if (what == "i" || what == "current") {
            std::string elem; if (!(in >> elem)) { return; }
            std::vector<OutputVariable> vars = { {OutputVariable::CURRENT, elem} };
            PlotData pd = simRunner->runTransient(tstep, tstop, tmax, vars);
            if (onPlotData) onPlotData(pd);
            return;
        }

        return;
    }

    parseCommandCore(line);
}


void CommandParser::handlePrintCommand(std::istringstream& iss) {
    std::string analysis_type;
    iss >> analysis_type;

    if (analysis_type == "TRAN") {
        std::string tstep_str, tstop_str, tmaxstep_str;
        if (!(iss >> tstep_str >> tstop_str >> tmaxstep_str)) {
            std::cerr << "Error: Syntax error. Usage: print TRAN <tstep> <tstop> <tmaxstep> <var1> ..." << std::endl;
            return;
        }
        double tstep = parseValueWithPrefix(tstep_str);
        double tstop = parseValueWithPrefix(tstop_str);
        double tmaxstep = parseValueWithPrefix(tmaxstep_str);

        if (tstep <= 0 || tstop <= 0 || tmaxstep <= 0 || tstep > tstop) {
            std::cerr << "Error: Invalid time parameters for TRAN analysis." << std::endl;
            return;
        }

        std::vector<OutputVariable> requested_vars;
        std::string var_token;
        std::regex var_regex(R"((V|I)\((.+)\))");
        while (iss >> var_token) {
            std::smatch matches;
            if (std::regex_match(var_token, matches, var_regex)) {
                OutputVariable out_var;
                out_var.type = (matches[1].str() == "V") ? OutputVariable::VOLTAGE : OutputVariable::CURRENT;
                out_var.name = matches[2].str();
                if (out_var.type == OutputVariable::CURRENT && !graph->findElement(out_var.name)) {
                    std::cout << "Error: Component " << out_var.name << " not found in circuit" << std::endl;
                    return;
                }
                requested_vars.push_back(out_var);
            } else {
                std::cerr << "Error: Invalid variable format: " << var_token << std::endl;
                return;
            }
        }
        if (requested_vars.empty()) {
            std::cerr << "Error: No output variables specified for print command." << std::endl;
            return;
        }
        simRunner->runTransient(tstep, tstop, tmaxstep, requested_vars);

    } else if (analysis_type == "DC") {
        std::string sourceName, start_str, end_str, inc_str;
        if (!(iss >> sourceName >> start_str >> end_str >> inc_str)) {
            std::cerr << "Error: Syntax error. Usage: print DC <SourceName> <Start> <End> <Increment> <var1>..." << std::endl;
            return;
        }
        double start_val = parseValueWithPrefix(start_str);
        double end_val = parseValueWithPrefix(end_str);
        double inc_val = parseValueWithPrefix(inc_str);
        if (inc_val <= 0) {
            std::cerr << "Error: Increment for DC sweep must be positive." << std::endl;
            return;
        }
        std::vector<OutputVariable> requested_vars;
        std::string var_token;
        std::regex var_regex(R"((V|I)\((.+)\))");
        while (iss >> var_token) {
            std::smatch matches;
            if (std::regex_match(var_token, matches, var_regex)) {
                OutputVariable out_var;
                out_var.type = (matches[1].str() == "V") ? OutputVariable::VOLTAGE : OutputVariable::CURRENT;
                out_var.name = matches[2].str();
                requested_vars.push_back(out_var);
            } else {
                std::cerr << "Error: Invalid variable format: " << var_token << std::endl;
                return;
            }
        }
        if (requested_vars.empty()) {
            std::cerr << "Error: No output variables specified for print command." << std::endl;
            return;
        }
        simRunner->runDCSweep(sourceName, start_val, end_val, inc_val, requested_vars);
    } else {
        std::cerr << "Error: Analysis type '" << analysis_type << "' not supported." << std::endl;
    }
}
void CommandParser::handleShowSchematics() {
    while (true) {
        std::vector<std::filesystem::path> schematics;
        std::cout << "\n-Choose existing schematic to load:" << std::endl;

        int i = 1;
        try {
            // Find all .txt files in the "schematics" directory
            for (const auto& entry : std::filesystem::directory_iterator("schematics")) {
                if (entry.path().extension() == ".txt") {
                    schematics.push_back(entry.path());
                    std::cout << i++ << "-" << entry.path().stem().string() << std::endl;
                }
            }
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Error accessing directory: " << e.what() << std::endl;
            return;
        }

        if (schematics.empty()) {
            std::cout << "No schematic files (.txt) found." << std::endl;
            return;
        }

        std::cout << ">>> ";
        std::string choice;
        if (!std::getline(std::cin, choice)) {
            break; // Exit on EOF
        }
        if (choice == "return") {
            break; // Exit if user types "return"
        }

        int choice_num;
        try {
            // Convert user input to a number
            choice_num = std::stoi(choice);
            if (choice_num < 1 || choice_num > schematics.size()) {
                std::cerr << "-Error: Inappropriate input" << std::endl;
                continue; // Re-display menu on invalid number
            }
        } catch (...) {
            std::cerr << "-Error: Inappropriate input" << std::endl;
            continue; // Re-display menu on non-numeric input
        }

        // Get the path of the chosen schematic
        std::filesystem::path chosen_path = schematics[choice_num - 1];

        // Construct a "load" command string from the chosen path
        std::string load_command = "load " + chosen_path.string();

        // Execute the load command by calling the main parser function
        std::cout << "Executing: " << load_command << std::endl;
        parseCommand(load_command);

        // Exit the menu loop to return to the main command prompt
        break;
    }
}

void CommandParser::handleSaveCommand(std::istringstream& iss) {
    std::string user_path_str;
    if (!(iss >> user_path_str)) {
        std::cerr << "Error: Syntax error. Usage: save <path/to/filename.txt>" << std::endl;
        return;
    }

    std::filesystem::path user_path(user_path_str);
    std::filesystem::path final_path;

    // بررسی می‌کنیم که آیا کاربر مسیر کامل داده یا فقط نام فایل
    // متد has_parent_path() چک می‌کند که آیا در مسیر، پوشه‌ای ذکر شده است یا نه
    if (user_path.has_parent_path() && !user_path.parent_path().empty()) {
        // کاربر یک مسیر کامل یا نسبی داده است (مثلا C:\circuits\c1.txt یا ../my_circuits/c1.txt)
        final_path = user_path;
        // اگر پوشه والد وجود ندارد، آن را می‌سازیم
        if (!std::filesystem::exists(final_path.parent_path())) {
            try {
                std::filesystem::create_directories(final_path.parent_path());
            } catch (const std::filesystem::filesystem_error& e) {
                std::cerr << "Error: Could not create directory " << final_path.parent_path() << ": " << e.what() << std::endl;
                return;
            }
        }
    } else {
        // کاربر فقط نام فایل را داده است (مثلا c1.txt)، پس در پوشه پیش‌فرض schematics ذخیره می‌کنیم
        std::filesystem::path schematics_dir = "schematics";
        if (!std::filesystem::exists(schematics_dir)) {
            std::filesystem::create_directory(schematics_dir);
        }
        final_path = schematics_dir / user_path;
    }

    std::ofstream outfile(final_path);

    if (!outfile.is_open()) {
        std::cerr << "Error: Could not open file for writing: " << final_path << std::endl;
        return;
    }

    // نوشتن اطلاعات هر قطعه در فایل
    for (const auto& elem : graph->getElements()) {
        char type_char = ' ';
        // تبدیل enum به کاراکتر برای ذخیره‌سازی
        switch (elem->type) {
            case RESISTOR: type_char = 'R'; break;
            case CAPACITOR: type_char = 'C'; break;
            case INDUCTOR: type_char = 'L'; break;
            case VOLTAGE_SOURCE: type_char = 'V'; break;
            case CURRENT_SOURCE: type_char = 'I'; break;
            case DIODE: type_char = 'D'; break;
            case VCCS: type_char = 'G'; break;
            case VCVS: type_char = 'E'; break;
            case CCCS: type_char = 'F'; break;
            case CCVS: type_char = 'H'; break;
                // SINUSOIDAL_SOURCE و PULSE_SOURCE به فرمت متفاوتی نیاز دارند و در اینجا ساده‌سازی شده‌اند
                // برای ذخیره کامل آن‌ها باید منطق جداگانه‌ای پیاده‌سازی شود.
            default: continue; // قطعات ناشناس یا پیچیده را ذخیره نکن
        }

        std::string n1_name = nodeManager->getNodeNameById(elem->node1);
        std::string n2_name = nodeManager->getNodeNameById(elem->node2);

        // فرمت ذخیره‌سازی باید مشابه فرمت خواندن دستور add باشد
        outfile << type_char << " " << elem->name << " " << n1_name << " " << n2_name << " ";

        if (elem->type == DIODE) {
            // برای دیود، مدل آن ذخیره می‌شود
            outfile << dynamic_cast<Diode*>(elem)->model << std::endl;
        } else {
            // برای سایر قطعات ساده، مقدارشان ذخیره می‌شود
            outfile << elem->value << std::endl;
        }
    }

    outfile.close();
    std::cout << "SUCCESS: Circuit saved to " << final_path.string() << std::endl;
}

int CommandParser::nodeFromToken(const std::string& tok) {
    return nodeManager->resolveId(tok);
}

void CommandParser::handleLabel(std::istringstream& in) {
    // syntax 1:  label <node|label> <name>
    // syntax 2:  label <name>             (creates a new isolated node with that name)
    std::string a, b;
    if (!(in >> a)) { std::cout << "usage: label <node|label> <name>\n"; return; }
    if (!(in >> b)) { // single token => create fresh node with that label
        nodeManager->labelNode(a);
        graph->canonicalizeNodes(*nodeManager);
        std::cout << "labeled new node as " << a << "\n";
        return;
    }
    int id = nodeManager->resolveId(a);
    nodeManager->setLabel(id, b);
    graph->canonicalizeNodes(*nodeManager);
    std::cout << "labeled node " << id << " as " << b << "\n";
}

void CommandParser::handleConnect(std::istringstream& in) {
    // connect two names or numbers: connect vdd net1
    std::string a,b;
    if (!(in >> a >> b)) { std::cout << "usage: connect <a> <b>\n"; return; }
    int rep = nodeManager->connect(a,b);
    graph->canonicalizeNodes(*nodeManager);
    std::cout << "connected; rep = " << rep << "\n";
}
void CommandParser::handleAddComponent(const std::string& instance_name, const std::string& external_n1_str, const std::string& external_n2_str) {
    std::string sub_path = "library/" + instance_name + ".sub";
    std::ifstream sub_file(sub_path);

    if (!sub_file.is_open()) {
        std::cerr << "Error: Cannot open subcircuit file: " << sub_path << std::endl;
        return;
    }

    // 1. Read the port definition line
    std::string line, keyword, internal_port1_name, internal_port2_name;
    std::getline(sub_file, line);
    std::istringstream port_iss(line);
    if (!(port_iss >> keyword >> internal_port1_name >> internal_port2_name) || keyword != "ports") {
        std::cerr << "Error: Invalid subcircuit file. First line must be 'ports <name1> <name2>'." << std::endl;
        return;
    }

    // 2. Resolve the EXTERNAL nodes in the main circuit
    int external_n1_id = nodeManager->resolveId(external_n1_str);
    int external_n2_id = nodeManager->resolveId(external_n2_str);

    // 3. Create a map to translate internal subcircuit node names to final IDs in the main graph
    std::map<std::string, int> nodeMap;
    nodeMap[internal_port1_name] = external_n1_id;
    nodeMap[internal_port2_name] = external_n2_id;

    // 4. Read the rest of the file to add internal elements
    while (std::getline(sub_file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream elem_iss(line);
        std::string add_cmd, elem_name_internal, n1_internal_str, n2_internal_str, val_str_or_model;

        if (!(elem_iss >> add_cmd >> elem_name_internal >> n1_internal_str >> n2_internal_str >> val_str_or_model)) continue;

        // For each node of the internal element, find its final ID in the main graph.
        // If it's a new internal node, create a fresh, unique ID for it.
        for (const auto& node_str : {n1_internal_str, n2_internal_str}) {
            if (nodeMap.find(node_str) == nodeMap.end()) {
                std::string unique_label = instance_name + "." + node_str;
                int new_id = nodeManager->labelNode(unique_label);
                nodeMap[node_str] = new_id;
            }
        }

        // Create the element with the mapped node IDs and a unique name
        int final_n1_id = nodeMap.at(n1_internal_str);
        int final_n2_id = nodeMap.at(n2_internal_str);
        std::string final_elem_name = instance_name + "." + elem_name_internal;
        double value = parseValueWithPrefix(val_str_or_model); // Assumes val_str_or_model is a value

        Element* new_elem = nullptr;
        char type = toupper(elem_name_internal[0]);
        switch (type) {
            case 'R': new_elem = new Resistor(final_elem_name, final_n1_id, final_n2_id, value); break;
            case 'C': new_elem = new Capacitor(final_elem_name, final_n1_id, final_n2_id, value); break;
            case 'L': new_elem = new Inductor(final_elem_name, final_n1_id, final_n2_id, value); break;
            case 'V': new_elem = new VoltageSource(final_elem_name, final_n1_id, final_n2_id, value); break;
            case 'I': new_elem = new CurrentSource(final_elem_name, final_n1_id, final_n2_id, value); break;
            case 'D': new_elem = new Diode(final_elem_name, final_n1_id, final_n2_id, val_str_or_model); break;
                // Add cases for other element types (VCCS, VCVS, etc.) as needed
            default:
                std::cerr << "Warning: Skipping unknown element type '" << type << "' inside subcircuit." << std::endl;
                continue;
        }
        graph->addElement(new_elem);
    }
    std::cout << "Successfully added subcircuit instance '" << instance_name << "' to the graph." << std::endl;
}

void CommandParser::handleAC(std::istringstream& iss) {
    std::string sweep; double ws, we; int N;
    iss >> sweep >> ws >> we >> N;

    ACSweepSettings s{};
    if (sweep == "lin") s.kind = ACSweepKind::Linear;
    else if (sweep == "dec") s.kind = ACSweepKind::Decade;
    else if (sweep == "oct") s.kind = ACSweepKind::Octave;
    else return;

    s.w_start = ws; s.w_stop = we; s.points = N;

    std::vector<OutputVariable> outs = currentOutputs();
    PlotData pd = simRunner->runACSweep(s, outs);
    appPlot(pd, outs);
}

void CommandParser::handlePhase(std::istringstream& iss) {
    double w0, phs, phe; int N;
    iss >> w0 >> phs >> phe >> N;

    PhaseSweepSettings s{w0, phs, phe, N};
    std::vector<OutputVariable> outs = currentOutputs();
    PlotData pd = simRunner->runPhaseSweep(s, outs);
    appPlot(pd, outs);
}
