// Created by Ali-Er on 6/6/2025.

#include "CommandParser.h"
#include "Controller/Exceptions.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <regex>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

namespace {
    std::string toUpper(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::toupper(c); });
        return s;
    }

    std::string trim(const std::string& s) {
        size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) return "";
        size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }

    std::vector<std::string> splitWords(const std::string& s) {
        std::vector<std::string> out;
        std::istringstream iss(s);
        std::string w;
        while (iss >> w) out.push_back(w);
        return out;
    }

    bool isNumber(const std::string& s) {
        static const std::regex pattern(R"(^[-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?$)");
        return std::regex_match(s, pattern);
    }

    double needValue(const std::string& s, const std::string& what) {
        auto v = CommandParser::parseValueWithPrefix(s);
        if (!v) throw SyntaxError("invalid " + what + " '" + s + "'");
        return *v;
    }

    // "PULSE(0 5 1m 1u 1u 5m 10m)" -> {"0","5",...}
    std::vector<std::string> argsInParens(const std::string& text, const std::string& keyword) {
        size_t first = text.find('(');
        size_t last = text.rfind(')');
        if (first == std::string::npos || last == std::string::npos || last < first) {
            throw SyntaxError("malformed " + keyword + " parameters, missing parentheses");
        }
        return splitWords(text.substr(first + 1, last - first - 1));
    }

    bool isOutputToken(const std::string& tok) {
        return tok.size() > 3 && (tok[0] == 'V' || tok[0] == 'v' || tok[0] == 'I' || tok[0] == 'i') && tok[1] == '(';
    }
}

CommandParser::CommandParser(Graph* g, NodeManager* n, SimulationRunner* r)
        : graph(g), nodeManager(n), simRunner(r) {}

std::optional<double> CommandParser::parseValueWithPrefix(const std::string& str) {
    if (str.empty()) return std::nullopt;
    try {
        std::string numPart = str;
        double factor = 1.0;

        std::string lower_str = str;
        std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(),
                       [](unsigned char c){ return std::tolower(c); });

        // "meg" first, otherwise 1meg would be read as 1m + "eg"
        if (lower_str.length() > 3 && lower_str.substr(lower_str.length() - 3) == "meg") {
            factor = 1e6;
            numPart = str.substr(0, str.length() - 3);
        } else {
            char last = lower_str.back();
            if (!std::isdigit(static_cast<unsigned char>(last)) && last != '.') {
                numPart = str.substr(0, str.length() - 1);
                switch (last) {
                    case 't': factor = 1e12; break;
                    case 'g': factor = 1e9;  break;
                    case 'k': factor = 1e3;  break;
                    case 'm': factor = 1e-3; break;
                    case 'u': factor = 1e-6; break;
                    case 'n': factor = 1e-9; break;
                    case 'p': factor = 1e-12; break;
                    case 'f': factor = 1e-15; break;
                    default: return std::nullopt;
                }
            }
        }

        if (numPart.empty() || !isNumber(numPart)) return std::nullopt;
        return std::stod(numPart) * factor;
    } catch (...) {
        return std::nullopt;
    }
}

std::string CommandParser::nodeName(int id) const {
    std::string s = nodeManager->nameOf(id);
    return s.empty() ? std::to_string(id) : s;
}

void CommandParser::parseCommand(const std::string& rawLine) {
    std::string line = trim(rawLine);
    if (line.empty() || line[0] == '*' || line[0] == '#') return;

    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    try {
        if (cmd == "add") handleAdd(iss);
        else if (cmd == "delete") {
            std::string name;
            if (!(iss >> name)) throw SyntaxError("delete <name>");
            if (!graph->removeElementByName(name)) throw ComponentNotFound(name);
        }
        else if (cmd == "list") {
            graph->canonicalizeNodes(*nodeManager);
            std::string type_filter;
            if (iss >> type_filter) graph->displayElementsByType(type_filter);
            else graph->displayGraph();
        }
        else if (cmd == ".nodes") nodeManager->displayNodes();
        else if (cmd == "rename") {
            std::string sub_cmd, old_name, new_name;
            if (!(iss >> sub_cmd >> old_name >> new_name) || sub_cmd != "node") {
                throw SyntaxError("rename node <old_name> <new_name>");
            }
            if (!nodeManager->renameNode(old_name, new_name)) {
                throw CircuitException("Node " + old_name + " does not exist (or is ground)");
            }
            std::cout << "Node " << old_name << " renamed to " << new_name << std::endl;
        }
        else if (cmd == "label") handleLabel(iss);
        else if (cmd == "connect" || cmd == "short") handleConnect(iss);
        else if (cmd == "value") handleValue(iss);
        else if (cmd == "load") handleLoad(iss);
        else if (cmd == "save") handleSaveCommand(iss);
        else if (cmd == "show") {
            std::string t1, t2;
            if (iss >> t1 >> t2 && t1 == "existing" && t2 == "schematics") handleShowSchematics();
            else throw CircuitException("Unknown command. Did you mean 'show existing schematics'?");
        }
        else if (cmd == "print") handlePrintCommand(iss);
        else if (cmd == "plot") handlePlot(iss);
        else if (cmd == "tran") handleTranSettings(iss);
        else if (cmd == "ac") handleAC(iss, false);
        else if (cmd == "phase") handlePhase(iss, false);
        else if (cmd == "subcircuit") handleSubcircuitCreate(iss);
        else if (cmd == "scope") handleScope(splitWords(line));
        else if (cmd == "reset") {
            graph->clear();
            nodeManager->reset();
            std::cout << "Circuit cleared." << std::endl;
        }
        else if (cmd == "help") printHelp();
        else throw CircuitException("Unknown command: " + cmd + " (type 'help')");
    } catch (const CircuitException& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void CommandParser::handleAdd(std::istringstream& iss) {
    std::string name;
    if (!(iss >> name)) throw SyntaxError("add <name> <node1> <node2> <value>");

    if (name == "GND") {
        std::string node;
        if (!(iss >> node)) throw SyntaxError("add GND <node>");
        nodeManager->assignNodeAsGND(node);
        return;
    }

    // old way of adding a subcircuit: add <SubName> <n1> <n2>
    if (fs::exists("library/" + name + ".sub")) {
        std::string n1_str, n2_str;
        if (!(iss >> n1_str >> n2_str)) throw SyntaxError("add <SubName> <Node1> <Node2>");
        handleAddComponent(name, name, n1_str, n2_str);
        return;
    }

    if (graph->findElement(name)) throw ElementAlreadyExists(name);

    std::string n1_str, n2_str;
    if (!(iss >> n1_str >> n2_str)) throw SyntaxError("add <name> <node1> <node2> <value>");

    std::string rest;
    std::getline(iss, rest);
    rest = trim(rest);
    const std::string restUp = toUpper(rest);
    std::vector<std::string> params = splitWords(rest);
    char type = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));

    // PULSE(v1 v2 td tr tf pw per)
    if (restUp.rfind("PULSE", 0) == 0) {
        auto p = argsInParens(rest, "PULSE");
        if (p.size() != 7) throw SyntaxError("expected PULSE(v1 v2 td tr tf pw per)");
        double v[7];
        for (int i = 0; i < 7; ++i) v[i] = needValue(p[i], "PULSE parameter");
        int n1 = nodeManager->resolveId(n1_str);
        int n2 = nodeManager->resolveId(n2_str);
        graph->addElement(new PulseSource(name, n1, n2, v[0], v[1], v[2], v[3], v[4], v[5], v[6]));
        std::cout << "Added pulse source: " << name << std::endl;
        return;
    }

    // SIN(offset amplitude freq [phase]) works for any name, S-names can skip the parentheses
    if (restUp.rfind("SIN", 0) == 0 || type == 'S') {
        std::vector<std::string> p = (restUp.rfind("SIN", 0) == 0) ? argsInParens(rest, "SIN") : params;
        if (p.size() < 3) throw SyntaxError("add <name> <n1> <n2> <Voffset> <Vamp> <Freq> [Phase]");
        double off = needValue(p[0], "offset");
        double amp = needValue(p[1], "amplitude");
        double freq = needValue(p[2], "frequency");
        double phase = p.size() > 3 ? needValue(p[3], "phase") : 0.0;
        int n1 = nodeManager->resolveId(n1_str);
        int n2 = nodeManager->resolveId(n2_str);
        graph->addElement(new SinusoidalSource(name, n1, n2, off, amp, freq, phase));
        std::cout << "Added sinusoidal source: " << name << std::endl;
        return;
    }

    if (type == 'X') {
        if (params.size() != 1) throw SyntaxError("add X<name> <n1> <n2> <SubName>");
        handleAddComponent(name, params[0], n1_str, n2_str);
        return;
    }

    if (type == 'D') {
        if (params.empty()) throw SyntaxError("add D<name> <n1> <n2> <D|Z>");
        const std::string& model = params[0];
        if (model != "D" && model != "Z") throw CircuitException("Model " + model + " not found in library");
        int n1 = nodeManager->resolveId(n1_str);
        int n2 = nodeManager->resolveId(n2_str);
        graph->addElement(new Diode(name, n1, n2, model));
        std::cout << "Added diode: " << name << std::endl;
        return;
    }

    if (type == 'G' || type == 'E') {
        if (params.size() < 3) throw SyntaxError("add " + std::string(1, type) + "<name> <n1> <n2> <ctrl+> <ctrl-> <gain>");
        double gain = needValue(params[2], "gain");
        int n1 = nodeManager->resolveId(n1_str);
        int n2 = nodeManager->resolveId(n2_str);
        int c1 = nodeManager->resolveId(params[0]);
        int c2 = nodeManager->resolveId(params[1]);
        if (type == 'G') graph->addElement(new vccs(name, n1, n2, c1, c2, gain));
        else graph->addElement(new vcvs(name, n1, n2, c1, c2, gain));
        std::cout << "Added element: " << name << std::endl;
        return;
    }

    if (type == 'F' || type == 'H') {
        if (params.size() < 2) throw SyntaxError("add " + std::string(1, type) + "<name> <n1> <n2> <ControlSource> <gain>");
        double gain = needValue(params[1], "gain");
        int n1 = nodeManager->resolveId(n1_str);
        int n2 = nodeManager->resolveId(n2_str);
        if (type == 'F') graph->addElement(new cccs(name, n1, n2, params[0], gain));
        else graph->addElement(new ccvs(name, n1, n2, params[0], gain));
        std::cout << "Added element: " << name << std::endl;
        return;
    }

    if (type != 'R' && type != 'C' && type != 'L' && type != 'V' && type != 'I') {
        throw CircuitException("Element " + name + " not found in library");
    }
    if (params.empty()) throw SyntaxError("missing value for " + name);
    double value = needValue(params[0], "value");

    if (type != 'V' && type != 'I' && value <= 0) {
        std::string typeName = (type == 'R') ? "Resistance" : (type == 'C') ? "Capacitance" : "Inductance";
        throw CircuitException(typeName + " cannot be zero or negative");
    }

    int n1 = nodeManager->resolveId(n1_str);
    int n2 = nodeManager->resolveId(n2_str);
    Element* e = nullptr;
    switch (type) {
        case 'R': e = new Resistor(name, n1, n2, value); break;
        case 'C': e = new Capacitor(name, n1, n2, value); break;
        case 'L': e = new Inductor(name, n1, n2, value); break;
        case 'V': e = new VoltageSource(name, n1, n2, value); break;
        default:  e = new CurrentSource(name, n1, n2, value); break;
    }
    graph->addElement(e);
    std::cout << "Added element: " << name << std::endl;
}

std::vector<OutputVariable> CommandParser::readOutputs(std::istringstream& iss) const {
    static const std::regex var_regex(R"(([VvIi])\((.+)\))");
    std::vector<OutputVariable> vars;
    std::string tok;
    while (iss >> tok) {
        std::smatch m;
        if (!std::regex_match(tok, m, var_regex)) throw SyntaxError("invalid variable format: " + tok);
        OutputVariable v;
        v.type = (std::toupper(m[1].str()[0]) == 'V') ? OutputVariable::VOLTAGE : OutputVariable::CURRENT;
        v.name = m[2].str();
        vars.push_back(v);
    }
    if (vars.empty()) throw CircuitException("No output variables specified (for example V(N1) or I(R1))");
    return vars;
}

// print the table and/or send it to the plot window
void CommandParser::emit(const PlotData& pd, bool printTable) {
    if (pd.empty()) return;
    if (printTable || !onPlotData) SimulationRunner::printTable(pd, std::cout);
    if (onPlotData) onPlotData(pd);
}

void CommandParser::handlePrintCommand(std::istringstream& iss) {
    std::string analysis_type;
    iss >> analysis_type;
    analysis_type = toUpper(analysis_type);

    if (analysis_type == "TRAN") {
        // print TRAN <tstep> <tstop> [<tmaxstep>] V(n) I(e) ...
        std::vector<double> nums;
        std::string tok;
        std::streampos pos = iss.tellg();
        while (iss >> tok) {
            if (isOutputToken(tok)) break;
            nums.push_back(needValue(tok, "time"));
            pos = iss.tellg();
        }
        if (nums.size() < 2 || nums.size() > 3) throw SyntaxError("print TRAN <tstep> <tstop> [<tmaxstep>] <var1> ...");

        iss.clear();
        iss.seekg(pos);
        auto vars = readOutputs(iss);

        double tstep = nums[0], tstop = nums[1], tmax = nums.size() > 2 ? nums[2] : 0.0;
        if (tstep <= 0 || tstop <= 0 || tmax < 0 || tstep > tstop) {
            throw CircuitException("Invalid time parameters for TRAN analysis.");
        }
        emit(simRunner->runTransient(tstep, tstop, tmax, vars), true);

    } else if (analysis_type == "DC") {
        std::string sourceName, start_str, end_str, inc_str;
        if (!(iss >> sourceName >> start_str >> end_str >> inc_str)) {
            throw SyntaxError("print DC <SourceName> <Start> <End> <Increment> <var1> ...");
        }
        double start = needValue(start_str, "start value");
        double stop = needValue(end_str, "end value");
        double inc = needValue(inc_str, "increment");
        if (inc <= 0) throw CircuitException("Increment for DC sweep must be positive.");
        auto vars = readOutputs(iss);
        emit(simRunner->runDCSweep(sourceName, start, stop, inc, vars), true);

    } else if (analysis_type == "AC") {
        handleAC(iss, true);
    } else if (analysis_type == "PHASE") {
        handlePhase(iss, true);
    } else {
        throw CircuitException("Analysis type '" + analysis_type + "' not supported.");
    }
}

// ac <lin|dec|oct> <w_start> <w_stop> <points> V(n) ...
void CommandParser::handleAC(std::istringstream& iss, bool print) {
    std::string sweep, ws, we, np;
    if (!(iss >> sweep >> ws >> we >> np)) throw SyntaxError("ac <lin|dec|oct> <w_start> <w_stop> <points> <var1> ...");

    ACSweepSettings s;
    sweep = toUpper(sweep);
    if (sweep == "LIN") s.kind = ACSweepKind::Linear;
    else if (sweep == "DEC") s.kind = ACSweepKind::Decade;
    else if (sweep == "OCT") s.kind = ACSweepKind::Octave;
    else throw SyntaxError("sweep type must be lin, dec or oct");

    s.w_start = needValue(ws, "w_start");
    s.w_stop = needValue(we, "w_stop");
    s.points = static_cast<int>(needValue(np, "number of points"));
    auto vars = readOutputs(iss);
    emit(simRunner->runACSweep(s, vars), print);
}

// phase <w> <phi_start> <phi_stop> <points> [source] V(n) ...
void CommandParser::handlePhase(std::istringstream& iss, bool print) {
    std::string w0, ps, pe, np;
    if (!(iss >> w0 >> ps >> pe >> np)) throw SyntaxError("phase <w> <phi_start> <phi_stop> <points> [source] <var1> ...");

    PhaseSweepSettings s;
    s.w_base = needValue(w0, "w");
    s.phi_start = needValue(ps, "start phase");
    s.phi_stop = needValue(pe, "stop phase");
    s.points = static_cast<int>(needValue(np, "number of points"));

    std::string tok;
    std::streampos pos = iss.tellg();
    if (iss >> tok && !isOutputToken(tok)) {
        s.source = tok;
    } else {
        iss.clear();
        iss.seekg(pos);
    }
    auto vars = readOutputs(iss);
    emit(simRunner->runPhaseSweep(s, vars), print);
}

// plot v <node> [node2]   /   plot i <element>
void CommandParser::handlePlot(std::istringstream& in) {
    std::string what;
    if (!(in >> what)) throw SyntaxError("plot v <node> [node2] | plot i <element>");
    for (auto& c : what) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    std::vector<OutputVariable> vars;
    std::string a, b;
    if (what == "v" || what == "voltage") {
        if (!(in >> a)) throw SyntaxError("plot v <node> [node2]");
        vars.push_back({OutputVariable::VOLTAGE, a});
        if (in >> b) vars.push_back({OutputVariable::VOLTAGE, b});
    } else if (what == "i" || what == "current") {
        if (!(in >> a)) throw SyntaxError("plot i <element>");
        vars.push_back({OutputVariable::CURRENT, a});
    } else {
        throw SyntaxError("plot v <node> [node2] | plot i <element>");
    }
    emit(simRunner->runTransient(tranStep, tranStop, tranMax, vars), false);
}

void CommandParser::handleTranSettings(std::istringstream& in) {
    std::string a, b, c;
    if (!(in >> a >> b)) {
        std::cout << "tran settings: tstep=" << tranStep << " tstop=" << tranStop << " tmaxstep=" << tranMax << std::endl;
        return;
    }
    double step = needValue(a, "tstep");
    double stop = needValue(b, "tstop");
    double mx = (in >> c) ? needValue(c, "tmaxstep") : 0.0;
    if (step <= 0 || stop <= 0 || step > stop || mx < 0) throw CircuitException("Invalid time parameters for TRAN analysis.");
    tranStep = step;
    tranStop = stop;
    tranMax = mx;
    std::cout << "plot will use tstep=" << tranStep << " tstop=" << tranStop << std::endl;
}

void CommandParser::handleValue(std::istringstream& iss) {
    std::string name, valueExpr;
    if (!(iss >> name >> valueExpr)) throw SyntaxError("value <element> <new value>");
    double v = needValue(valueExpr, "value");
    Element* e = graph->findElement(name);
    if (!e) throw ElementNotFound(name);
    if (e->type == DIODE || e->type == SINUSOIDAL_SOURCE || e->type == PULSE_SOURCE) {
        throw CircuitException(name + " has more than one parameter, delete it and add it again");
    }
    if ((e->type == RESISTOR || e->type == CAPACITOR || e->type == INDUCTOR) && v <= 0) {
        throw CircuitException("Value of " + name + " must be positive");
    }
    simRunner->setElementValue(name, v);
    std::cout << name << " = " << v << std::endl;
}

void CommandParser::handleShowSchematics() {
    lastSchematics.clear();
    if (!fs::exists("schematics")) {
        std::cout << "No schematics folder yet (save something first)." << std::endl;
        return;
    }
    for (const auto& entry : fs::directory_iterator("schematics")) {
        if (entry.path().extension() == ".txt") lastSchematics.push_back(entry.path());
    }
    std::sort(lastSchematics.begin(), lastSchematics.end());

    if (lastSchematics.empty()) {
        std::cout << "No schematic files (.txt) found." << std::endl;
        return;
    }
    std::cout << "-Choose existing schematic to load:" << std::endl;
    for (size_t i = 0; i < lastSchematics.size(); ++i) {
        std::cout << i + 1 << "-" << lastSchematics[i].stem().string() << std::endl;
    }
    std::cout << "(type: load <number>)" << std::endl;
}

void CommandParser::handleLoad(std::istringstream& iss) {
    std::string arg;
    if (!(iss >> arg)) throw SyntaxError("load <file_path> | load <number>");

    fs::path path = arg;
    bool isIndex = !arg.empty() && std::all_of(arg.begin(), arg.end(), ::isdigit);
    if (isIndex && !fs::exists(path)) {
        size_t idx = std::stoul(arg);
        if (idx < 1 || idx > lastSchematics.size()) throw CircuitException("Inappropriate input");
        path = lastSchematics[idx - 1];
    }
    if (!fs::exists(path) && fs::exists(fs::path("schematics") / path)) path = fs::path("schematics") / path;

    std::ifstream infile(path);
    if (!infile.is_open()) throw CircuitException("Cannot open file: " + path.string());

    // loading replaces whatever was there before
    graph->clear();
    nodeManager->reset();
    std::cout << "Loading circuit from file: " << path.string() << std::endl;

    std::string file_line;
    while (std::getline(infile, file_line)) {
        auto words = splitWords(file_line);
        if (words.empty() || words[0][0] == '*' || words[0][0] == '#') continue;

        // saved format is "R R1 n1 n2 1k", anything else is treated as a normal command
        if (words[0].size() == 1 && std::isalpha(static_cast<unsigned char>(words[0][0])) && words.size() >= 4) {
            std::string rest = trim(file_line);
            parseCommand("add " + rest.substr(1));
        } else {
            parseCommand(file_line);
        }
    }
    std::cout << "Finished loading from file." << std::endl;
}

void CommandParser::handleSaveCommand(std::istringstream& iss) {
    std::string user_path_str;
    if (!(iss >> user_path_str)) throw SyntaxError("save <path/to/filename.txt>");

    fs::path final_path(user_path_str);
    if (!final_path.has_extension()) final_path += ".txt";

    if (final_path.has_parent_path()) {
        // full or relative path given, make the folder if it's not there
        std::error_code ec;
        fs::create_directories(final_path.parent_path(), ec);
        if (ec) throw CircuitException("Could not create directory " + final_path.parent_path().string());
    } else {
        // only a file name -> goes to the default schematics folder
        fs::create_directories("schematics");
        final_path = fs::path("schematics") / final_path;
    }

    std::ofstream outfile(final_path);
    if (!outfile.is_open()) throw CircuitException("Could not open file for writing: " + final_path.string());

    graph->canonicalizeNodes(*nodeManager);
    outfile << std::setprecision(12);
    outfile << "* MorghSpicy circuit\n";

    for (Element* elem : graph->getElements()) {
        std::string n1 = nodeName(elem->node1);
        std::string n2 = nodeName(elem->node2);
        char letter = static_cast<char>(std::toupper(static_cast<unsigned char>(elem->name[0])));

        switch (elem->type) {
            case DIODE:
                outfile << "D " << elem->name << " " << n1 << " " << n2 << " " << static_cast<Diode*>(elem)->model;
                break;
            case VCCS:
            case VCVS: {
                int c1, c2;
                if (auto* g = dynamic_cast<vccs*>(elem)) { c1 = g->controlPos(); c2 = g->controlNeg(); }
                else { auto* e = static_cast<vcvs*>(elem); c1 = e->controlPos(); c2 = e->controlNeg(); }
                outfile << letter << " " << elem->name << " " << n1 << " " << n2 << " "
                        << nodeName(c1) << " " << nodeName(c2) << " " << elem->value;
                break;
            }
            case CCCS:
                outfile << "F " << elem->name << " " << n1 << " " << n2 << " "
                        << static_cast<cccs*>(elem)->controlName() << " " << elem->value;
                break;
            case CCVS:
                outfile << "H " << elem->name << " " << n1 << " " << n2 << " "
                        << static_cast<ccvs*>(elem)->controlName() << " " << elem->value;
                break;
            case SINUSOIDAL_SOURCE: {
                auto* s = static_cast<SinusoidalSource*>(elem);
                outfile << "V " << elem->name << " " << n1 << " " << n2 << " SIN("
                        << s->offset() << " " << s->amplitude() << " " << s->freq() << " " << s->phaseDeg() << ")";
                break;
            }
            case PULSE_SOURCE: {
                auto* p = static_cast<PulseSource*>(elem);
                outfile << "V " << elem->name << " " << n1 << " " << n2 << " PULSE("
                        << p->v1 << " " << p->v2 << " " << p->td << " " << p->tr << " "
                        << p->tf << " " << p->pw << " " << p->per << ")";
                break;
            }
            default:
                outfile << letter << " " << elem->name << " " << n1 << " " << n2 << " " << elem->value;
                break;
        }
        outfile << "\n";
    }

    std::cout << "SUCCESS: Circuit saved to " << final_path.string() << std::endl;
}

// subcircuit create <SubName> from <Node1> <Node2>
void CommandParser::handleSubcircuitCreate(std::istringstream& iss) {
    std::string action, subName, from_keyword, p1, p2;
    if (!(iss >> action >> subName >> from_keyword >> p1 >> p2) || action != "create" || from_keyword != "from") {
        throw SyntaxError("subcircuit create <SubName> from <Node1> <Node2>");
    }
    auto id1 = nodeManager->findId(p1);
    auto id2 = nodeManager->findId(p2);
    if (!id1 || !id2) throw CircuitException("Port nodes must exist in the circuit");

    fs::create_directories("library");
    std::string sub_path = "library/" + subName + ".sub";
    std::ofstream outfile(sub_path);
    if (!outfile.is_open()) throw CircuitException("Could not open file for writing: " + sub_path);

    graph->canonicalizeNodes(*nodeManager);
    auto portName = [&](int id) {
        if (id == *id1) return p1;
        if (id == *id2) return p2;
        return nodeName(id);
    };

    outfile << std::setprecision(12);
    outfile << "ports " << p1 << " " << p2 << "\n";
    for (Element* elem : graph->getElements()) {
        if (elem->type != RESISTOR && elem->type != CAPACITOR && elem->type != INDUCTOR &&
            elem->type != VOLTAGE_SOURCE && elem->type != CURRENT_SOURCE && elem->type != DIODE) {
            std::cerr << "Warning: " << elem->name << " can't be stored in a subcircuit, skipped." << std::endl;
            continue;
        }
        outfile << "add " << elem->name << " " << portName(elem->node1) << " " << portName(elem->node2) << " ";
        if (elem->type == DIODE) outfile << static_cast<Diode*>(elem)->model << "\n";
        else outfile << elem->value << "\n";
    }
    std::cout << "SUCCESS: Subcircuit '" << subName << "' saved to " << sub_path << std::endl;
}

void CommandParser::handleAddComponent(const std::string& instance_name, const std::string& sub_name,
                                       const std::string& external_n1_str, const std::string& external_n2_str) {
    std::string sub_path = "library/" + sub_name + ".sub";
    std::ifstream sub_file(sub_path);
    if (!sub_file.is_open()) throw CircuitException("Cannot open subcircuit file: " + sub_path);

    // first line: ports <name1> <name2>
    std::string line, keyword, port1, port2;
    std::getline(sub_file, line);
    std::istringstream port_iss(line);
    if (!(port_iss >> keyword >> port1 >> port2) || keyword != "ports") {
        throw CircuitException("Invalid subcircuit file. First line must be 'ports <name1> <name2>'.");
    }

    // internal node name -> node id in the main circuit
    std::map<std::string, int> nodeMap;
    nodeMap[port1] = nodeManager->resolveId(external_n1_str);
    nodeMap[port2] = nodeManager->resolveId(external_n2_str);

    int added = 0;
    while (std::getline(sub_file, line)) {
        std::istringstream elem_iss(line);
        std::string add_cmd, elem_name, n1s, n2s, val;
        if (!(elem_iss >> add_cmd >> elem_name >> n1s >> n2s >> val) || add_cmd != "add") continue;

        for (const auto& node_str : {n1s, n2s}) {
            if (nodeMap.count(node_str)) continue;
            // ground inside the subcircuit is the same ground, every other node is private
            if (NodeManager::isGroundToken(node_str)) nodeMap[node_str] = 0;
            else nodeMap[node_str] = nodeManager->labelNode(instance_name + "." + node_str);
        }

        int n1 = nodeMap.at(n1s);
        int n2 = nodeMap.at(n2s);
        std::string final_name = instance_name + "." + elem_name;
        if (graph->findElement(final_name)) throw ElementAlreadyExists(final_name);

        double value = parseValueWithPrefix(val).value_or(0.0);
        Element* e = nullptr;
        switch (std::toupper(static_cast<unsigned char>(elem_name[0]))) {
            case 'R': e = new Resistor(final_name, n1, n2, value); break;
            case 'C': e = new Capacitor(final_name, n1, n2, value); break;
            case 'L': e = new Inductor(final_name, n1, n2, value); break;
            case 'V': e = new VoltageSource(final_name, n1, n2, value); break;
            case 'I': e = new CurrentSource(final_name, n1, n2, value); break;
            case 'D': e = new Diode(final_name, n1, n2, val); break;
            default:
                std::cerr << "Warning: Skipping unknown element '" << elem_name << "' inside subcircuit." << std::endl;
                continue;
        }
        graph->addElement(e);
        ++added;
    }
    std::cout << "Added subcircuit " << instance_name << " (" << sub_name << ", " << added << " elements)" << std::endl;
}

void CommandParser::handleLabel(std::istringstream& in) {
    // label <node|label> <name>   or   label <name> (new empty node)
    std::string a, b;
    if (!(in >> a)) throw SyntaxError("label <node|label> <name>");
    if (!(in >> b)) {
        nodeManager->labelNode(a);
        std::cout << "labeled new node as " << a << "\n";
        return;
    }
    int id = nodeManager->resolveId(a);
    nodeManager->setLabel(id, b);
    std::cout << "labeled node " << id << " as " << b << "\n";
}

void CommandParser::handleConnect(std::istringstream& in) {
    std::string a, b;
    if (!(in >> a >> b)) throw SyntaxError("connect <a> <b>");
    nodeManager->connect(a, b);
    std::cout << "connected " << a << " and " << b << "\n";
}

// scope load path=<file> Fs=10000 t=0.1 chunk=8192   /   scope clear
void CommandParser::handleScope(const std::vector<std::string>& parts) {
    if (parts.size() >= 2 && parts[1] == "load") {
        std::string path;
        double Fs = 10000.0, tStop = 0.1;
        int chunk = 8192;
        for (size_t i = 2; i < parts.size(); ++i) {
            auto eq = parts[i].find('=');
            if (eq == std::string::npos) continue;
            std::string k = parts[i].substr(0, eq), v = parts[i].substr(eq + 1);
            if (k == "path") path = v;
            else if (k == "Fs") Fs = parseValueWithPrefix(v).value_or(Fs);
            else if (k == "t") tStop = parseValueWithPrefix(v).value_or(tStop);
            else if (k == "chunk") chunk = static_cast<int>(parseValueWithPrefix(v).value_or(chunk));
        }
        if (path.empty()) throw SyntaxError("scope load path=<file> [Fs=..] [t=..] [chunk=..]");
        if (onScopeLoad) onScopeLoad(path, Fs, tStop, chunk);
        else std::cout << "scope only works in the GUI" << std::endl;
        return;
    }
    if (parts.size() >= 2 && parts[1] == "clear") {
        if (onScopeClear) onScopeClear();
        return;
    }
    throw SyntaxError("scope load ... | scope clear");
}

void CommandParser::printHelp() const {
    std::cout <<
        "Circuit:\n"
        "  add R1 n1 n2 1k            (R, C, L, V, I)\n"
        "  add D1 n1 n2 D|Z           diode / zener\n"
        "  add V1 n1 n2 PULSE(v1 v2 td tr tf pw per)\n"
        "  add V1 n1 n2 SIN(off amp f [phase])   or   add S1 n1 n2 off amp f [phase]\n"
        "  add E1 n1 n2 c+ c- gain    (G = VCCS)      add H1 n1 n2 Vctrl gain  (F = CCCS)\n"
        "  add GND n1    connect a b    label n1 name    rename node a b\n"
        "  delete R1    value R1 2k    list [type]    .nodes    reset\n"
        "  subcircuit create Name from n1 n2    add X1 n1 n2 Name\n"
        "Files:\n"
        "  save file.txt    load file.txt    show existing schematics    load <number>\n"
        "Analysis:\n"
        "  print TRAN tstep tstop [tmaxstep] V(n1) I(R1) ...\n"
        "  print DC V1 start stop step V(n1) ...\n"
        "  ac dec|oct|lin w_start w_stop points V(n1) ...     (w in rad/s)\n"
        "  phase w phi_start phi_stop points [source] V(n1) ...\n"
        "  tran tstep tstop [tmaxstep]    plot v n1 [n2]    plot i R1\n"
        "  scope load path=file.txt Fs=10k t=0.1    scope clear\n";
}
