// Created by Ali-Er on 6/6/2025.

#include "CommandParser.h"
#include <sstream>
#include <iostream>
#include <cctype>
#include <unordered_set>
#include <regex>
#include <fstream>
CommandParser::CommandParser(Graph* g, NodeManager* nm, SimulationRunner* runner)
        : graph(g), nodeManager(nm), simRunner(runner) {}

bool isNumber(const std::string& s) {
    std::regex pattern(R"(^[-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?$)");
    return std::regex_match(s, pattern);
}

// برای ساپورت کردن کیلو و مگا و نانو واینا و نماد علمی
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

void CommandParser::parseCommand(const std::string& line) {
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    if (cmd == "add") {
        std::string name, n1_str, n2_str;
        if (!(iss >> name)) {
            std::cerr << "Error: Syntax error\n";
            return;
        }

        char type = name[0];

        // 📍 پشتیبانی از GND: add GND <node>
        if (name == "GND") {
            if (!(iss >> n1_str)) {
                std::cerr << "Error: Syntax error\n";
                return;
            }
            nodeManager->assignNodeAsGND(n1_str);
            std::cout << "Node " << n1_str << " assigned as GND\n";
            return;
        }

        if (!isupper(type)) {
            std::cerr << "Error: Element " << name << " not found in library\n";
            return;
        }

        for (Element* e : graph->getElements()) {
            if (e->name == name) {
                std::cerr << "Error: " << name << " already exists in the circuit\n";
                return;
            }
        }

        if (!(iss >> n1_str >> n2_str)) {
            std::cerr << "Error: Syntax error\n";
            return;
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
            int n1 = nodeManager->getOrCreateNodeId(n1_str);
            int n2 = nodeManager->getOrCreateNodeId(n2_str);
            graph->addElement(new Diode(name, n1, n2, model));
            std::cout << "Added diode: " << name << std::endl;
            return;
        }

        std::string value_str;
        if (!(iss >> value_str)) {
            std::cerr << "Error: Syntax error (missing value)\n";
            return;
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

        int n1 = nodeManager->getOrCreateNodeId(n1_str);
        int n2 = nodeManager->getOrCreateNodeId(n2_str);
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
    }else if (cmd == "print") {
        handlePrintCommand(iss);
    } else {
        std::cerr << "Error: Unknown command: " << cmd << std::endl;
    }
}

void CommandParser::handlePrintCommand(std::istringstream& iss) {
    std::string analysis_type;
    iss >> analysis_type;

    if (analysis_type == "TRAN") {
        std::string tstep_str, tstop_str;
        if (!(iss >> tstep_str >> tstop_str)) {
            std::cerr << "Error: Syntax error. Usage: print TRAN <tstep> <tstop> <var1> <var2> ..." << std::endl;
            return;
        }
        double tstep = parseValueWithPrefix(tstep_str);
        double tstop = parseValueWithPrefix(tstop_str);
        if (tstep <= 0 || tstop <= 0 || tstep > tstop) {
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
        simRunner->runTransient(tstep, tstop, requested_vars);

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