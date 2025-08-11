//
// Created by Mehrshad on 6/10/2025.
//

#ifndef MORGHSPICY_EXCEPTIONS_H
#define MORGHSPICY_EXCEPTIONS_H

#include <stdexcept>
#include <string>

class CircuitException : public std::runtime_error {
public:
    CircuitException(const std::string& message) : std::runtime_error(message) {}
};

class SyntaxError : public CircuitException {
public:
    SyntaxError(const std::string& message) : CircuitException("Syntax error: " + message) {}
};

class ElementNotFound : public CircuitException {
public:
    ElementNotFound(const std::string& element) : CircuitException("Element " + element + " not found") {}
};

class ElementAlreadyExists : public CircuitException {
public:
    ElementAlreadyExists(const std::string& element) : CircuitException("Element " + element + " already exists") {}
};

class ComponentNotFound : public CircuitException {
public:
    ComponentNotFound(const std::string& component) : CircuitException("Cannot delete; component " + component + " not found") {}
};

#endif //MORGHSPICY_EXCEPTIONS_H
