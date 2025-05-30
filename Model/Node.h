//
// Created by Mehrshad on 5/26/2025.
//
#include <string>

#ifndef MORGHSPICY_NODE_H
#define MORGHSPICY_NODE_H

class Node {
private:
    int id;
    std::string name;
public:
    Node (int id, std::string name): id(id), name(name) {}
    std::string getName() const {return name;}
    int getId() const {return id;}
};

#endif //MORGHSPICY_NODE_H
