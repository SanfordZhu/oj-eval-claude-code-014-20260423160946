#pragma once
#ifndef PYTHON_INTERPRETER_FUNCTION_H
#define PYTHON_INTERPRETER_FUNCTION_H

#include "Value.h"
#include "Scope.h"
#include <vector>
#include <string>
#include <any>
#include "Python3Parser.h"

class Function {
private:
    std::vector<std::string> parameters;
    std::vector<Value> defaultValues;
    antlr4::tree::ParseTree* body;
    std::shared_ptr<Scope> closure;

public:
    Function(const std::vector<std::string>& params,
             const std::vector<Value>& defaults,
             antlr4::tree::ParseTree* functionBody,
             std::shared_ptr<Scope> scope)
        : parameters(params), defaultValues(defaults), body(functionBody), closure(scope) {}

    const std::vector<std::string>& getParameters() const { return parameters; }
    const std::vector<Value>& getDefaultValues() const { return defaultValues; }
    antlr4::tree::ParseTree* getBody() const { return body; }
    std::shared_ptr<Scope> getClosure() const { return closure; }

    int getMinArgs() const {
        return parameters.size() - defaultValues.size();
    }

    int getMaxArgs() const {
        return parameters.size();
    }
};

#endif // PYTHON_INTERPRETER_FUNCTION_H