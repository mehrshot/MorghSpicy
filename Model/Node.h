//
// Created by Mehrshad on 5/26/2025.
//
#include <string>
using namespace std;

#ifndef MORGHSPICY_NODE_H
#define MORGHSPICY_NODE_H
class Node {
private:
    int id;
    string name;
public:
    Node (int id, string name): id(id), name(name) {}
    string getName() {return name;}
    int getId() {return id;}
};
#endif //MORGHSPICY_NODE_H
