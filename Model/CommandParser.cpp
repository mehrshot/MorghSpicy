//
// Created by Ali on 6/6/2025.
//

#include "CommandParser.h"
#include <sstream>
#include <iostream>
#include <cctype>
#include <unordered_set>
#include <regex>

CommandParser::CommandParser(Graph* g, NodeManager* nm) : graph(g), nodeManager(nm) {}

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
        std::string name, n1_str, n2_str, extra;
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
            int gnd_id = 0;
            nodeManager->assignNodeAsGND(n1_str); // باید این متد رو در NodeManager بنویسی
            std::cout << "Node " << n1_str << " assigned as GND\n";
            return;
        }

        // بررسی حروف بزرگ بودن
        if (!isupper(type)) {
            std::cerr << "Error: Element " << name << " not found in library\n";
            return;
        }

        // بررسی تکراری بودن نام
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

        // // ✅ دیود
        // if (type == 'D') {
        //     std::string model;
        //     if (!(iss >> model)) {
        //         std::cerr << "Error: Syntax error\n";
        //         return;
        //     }
        //
        //     // مدل مجاز؟ فقط D و Z فعلاً
        //     static std::unordered_set<std::string> validModels = {"D", "Z"};
        //     if (!validModels.count(model)) {
        //         std::cerr << "Error: Model " << model << " not found in library\n";
        //         return;
        //     }
        //
        //     std::cout << "Info: Diode " << name << " between " << n1_str << " and " << n2_str
        //               << " with model " << model << " added (placeholder)\n";
        //     // اگر بعداً کلاس Diode رو ساختی، اینجا new Diode(...) بزن
        //     return;
        // }

        // سایر المان‌ها: مقدار باید باشه
        std::string value_str;
        if (!(iss >> value_str)) {
            std::cerr << "Error: Syntax error (missing value)\n";
            return;
        }

        double value = parseValueWithPrefix(value_str);
        if (value <= 0 || value == -1e99) {
            std::string typeName = "Value";
            if (type == 'R') typeName = "Resistance";
            else if (type == 'C') typeName = "Capacitance";
            else if (type == 'L') typeName = "Inductance";
            else if (type == 'V') typeName = "Voltage";
            else if (type == 'I') typeName = "Current";

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
        graph->desplayGraph();

    } else {
        std::cerr << "Error: Unknown command: " << cmd << std::endl;
    }
}




