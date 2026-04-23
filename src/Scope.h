#pragma once
#ifndef PYTHON_INTERPRETER_SCOPE_H
#define PYTHON_INTERPRETER_SCOPE_H

#include "Value.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <string>
#include <any>

class Scope : public std::enable_shared_from_this<Scope> {
private:
    std::unordered_map<std::string, Value> variables;
    std::shared_ptr<Scope> parent;
    bool isFunctionScope;

public:
    Scope(std::shared_ptr<Scope> parent = nullptr, bool isFunctionScope = false)
        : parent(parent), isFunctionScope(isFunctionScope) {}

    void setVariable(const std::string& name, const Value& value) {
        variables[name] = value;
    }

    Value getVariable(const std::string& name) {
        auto it = variables.find(name);
        if (it != variables.end()) {
            return it->second;
        }

        // Global variables are accessible everywhere
        if (parent) {
            return parent->getVariable(name);
        }

        throw std::runtime_error("Undefined variable: " + name);
    }

    bool hasVariable(const std::string& name) const {
        return variables.find(name) != variables.end() ||
               (parent && parent->hasVariable(name));
    }

    std::shared_ptr<Scope> getParent() const {
        return parent;
    }

    bool getIsFunctionScope() const {
        return isFunctionScope;
    }

    // For function calls, create a new scope
    std::shared_ptr<Scope> createFunctionScope() {
        return std::make_shared<Scope>(shared_from_this(), true);
    }

    // For global scope
    static std::shared_ptr<Scope> createGlobalScope() {
        return std::make_shared<Scope>(nullptr, false);
    }
};

#endif // PYTHON_INTERPRETER_SCOPE_H