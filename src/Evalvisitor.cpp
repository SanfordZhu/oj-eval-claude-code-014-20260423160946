#include "Evalvisitor.h"
#include "Python3Lexer.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>

EvalVisitor::EvalVisitor() {
    currentScope = Scope::createGlobalScope();
}

std::any EvalVisitor::visitFile_input(Python3Parser::File_inputContext *ctx) {
    for (auto stmt : ctx->stmt()) {
        visit(stmt);
        if (shouldReturn || shouldBreak || shouldContinue) {
            break;
        }
    }
    return lastValue;
}

std::any EvalVisitor::visitStmt(Python3Parser::StmtContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitSimple_stmt(Python3Parser::Simple_stmtContext *ctx) {
    return visit(ctx->small_stmt());
}

std::any EvalVisitor::visitSmall_stmt(Python3Parser::Small_stmtContext *ctx) {
    if (ctx->expr_stmt()) {
        return visit(ctx->expr_stmt());
    } else if (ctx->flow_stmt()) {
        return visit(ctx->flow_stmt());
    }
    return {};
}

std::any EvalVisitor::visitExpr_stmt(Python3Parser::Expr_stmtContext *ctx) {
    if (ctx->augassign()) {
        // Augmented assignment (e.g., a += b)
        auto targetList = ctx->testlist(0);
        auto valueList = ctx->testlist(1);

        std::vector<std::string> targets;
        for (auto test : targetList->test()) {
            std::string varName = extractVariableName(test);
            if (!varName.empty()) {
                targets.push_back(varName);
            }
        }

        Value rightValue = evaluateExpression(valueList->test(0));
        std::string op = ctx->augassign()->getText();
        op = op.substr(0, op.length() - 1); // Remove '='

        Value result;
        for (const auto& target : targets) {
            Value leftValue = currentScope->getVariable(target);
            result = applyBinaryOp(leftValue, op, rightValue);
            currentScope->setVariable(target, result);
        }

        return result;
    } else if (!ctx->ASSIGN().empty()) {
        // Regular assignment or chained assignment
        // Get the rightmost value (last testlist)
        auto valueList = ctx->testlist(ctx->testlist().size() - 1);

        Value value;
        if (valueList->test().size() == 1) {
            value = evaluateExpression(valueList->test(0));
        } else {
            // Multiple values on right side - create tuple
            TupleType tuple;
            for (auto test : valueList->test()) {
                tuple.push_back(evaluateExpression(test));
            }
            value = Value(tuple);
        }

        // Assign to all target lists (handle chained assignment)
        for (size_t i = 0; i < ctx->testlist().size() - 1; i++) {
            auto targetList = ctx->testlist(i);

            // Handle multiple assignment
            std::vector<std::string> targets;
            for (auto test : targetList->test()) {
                // Traverse down the parse tree to find the atom with NAME
                std::string varName = extractVariableName(test);
                if (!varName.empty()) {
                    targets.push_back(varName);
                }
            }

            if (targets.size() == 1) {
                currentScope->setVariable(targets[0], value);
            } else if (targets.size() > 1) {
                // Multiple assignment - create tuple if value is not already a tuple
                if (isTuple(value) && asTuple(value).size() == targets.size()) {
                    const auto& tuple = asTuple(value);
                    for (size_t j = 0; j < targets.size(); j++) {
                        currentScope->setVariable(targets[j], std::any_cast<Value>(tuple[j]));
                    }
                } else {
                    // For now, just assign the same value to all variables
                    // This handles cases like a = b = 1
                    for (const auto& target : targets) {
                        currentScope->setVariable(target, value);
                    }
                }
            }
        }

        return value;
    } else {
        // Expression statement
        return evaluateExpression(ctx->testlist(0)->test(0));
    }
}

std::any EvalVisitor::visitAugassign(Python3Parser::AugassignContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitFlow_stmt(Python3Parser::Flow_stmtContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitBreak_stmt(Python3Parser::Break_stmtContext *ctx) {
    shouldBreak = true;
    return {};
}

std::any EvalVisitor::visitContinue_stmt(Python3Parser::Continue_stmtContext *ctx) {
    shouldContinue = true;
    return {};
}

std::any EvalVisitor::visitReturn_stmt(Python3Parser::Return_stmtContext *ctx) {
    if (ctx->testlist()) {
        auto result = visit(ctx->testlist());
        try {
            Value resultValue = std::any_cast<Value>(result);
            if (isTuple(resultValue)) {
                auto values = asTuple(resultValue);
                if (values.size() == 1) {
                    lastValue = std::any_cast<Value>(values[0]);
                } else {
                    lastValue = resultValue;
                }
            } else {
                lastValue = resultValue;
            }
        } catch (...) {
            // Not a Value, might be something else
            lastValue = noneValue();
        }
    } else {
        lastValue = noneValue();
    }
    shouldReturn = true;
    return lastValue;
}

std::any EvalVisitor::visitCompound_stmt(Python3Parser::Compound_stmtContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitIf_stmt(Python3Parser::If_stmtContext *ctx) {
    // Evaluate if condition
    Value condition = evaluateExpression(ctx->test(0));
    if (asBool(toBool(condition))) {
        return visit(ctx->suite(0));
    }

    // Check elif conditions
    int testIndex = 1;
    for (size_t i = 0; i < ctx->ELIF().size(); i++) {
        Value elifCondition = evaluateExpression(ctx->test(testIndex));
        if (asBool(toBool(elifCondition))) {
            return visit(ctx->suite(i + 1));
        }
        testIndex++;
    }

    // Check else
    if (ctx->ELSE()) {
        return visit(ctx->suite(ctx->suite().size() - 1));
    }

    return {};
}

std::any EvalVisitor::visitWhile_stmt(Python3Parser::While_stmtContext *ctx) {
    while (true) {
        Value condition = evaluateExpression(ctx->test());
        if (!asBool(toBool(condition))) {
            break;
        }

        // Save loop control states
        bool prevBreak = shouldBreak;
        bool prevContinue = shouldContinue;
        shouldBreak = false;
        shouldContinue = false;

        visit(ctx->suite());

        if (shouldBreak) {
            shouldBreak = false;
            break;
        }

        if (shouldContinue) {
            shouldContinue = false;
            continue;
        }

        if (shouldReturn) {
            break;
        }
    }

    return {};
}

std::any EvalVisitor::visitSuite(Python3Parser::SuiteContext *ctx) {
    if (ctx->simple_stmt()) {
        return visit(ctx->simple_stmt());
    } else {
        for (auto stmt : ctx->stmt()) {
            visit(stmt);
            if (shouldReturn || shouldBreak || shouldContinue) {
                break;
            }
        }
        return {};
    }
}

std::any EvalVisitor::visitFuncdef(Python3Parser::FuncdefContext *ctx) {
    std::string funcName = ctx->NAME()->getText();

    // Get parameters
    std::vector<std::string> params;
    std::vector<Value> defaults;

    if (ctx->parameters() && ctx->parameters()->typedargslist()) {
        auto typedargs = ctx->parameters()->typedargslist();
        for (size_t i = 0; i < typedargs->tfpdef().size(); i++) {
            params.push_back(typedargs->tfpdef(i)->NAME()->getText());

            // Check if there's a default value for this parameter
            if (i < typedargs->ASSIGN().size()) {
                // Has default value
                defaults.push_back(evaluateExpression(typedargs->test(i)));
            }
        }
    }

    // Create function object
    auto func = std::make_shared<Function>(params, defaults, ctx->suite(), currentScope);
    functions[funcName] = func;

    return {};
}

std::any EvalVisitor::visitParameters(Python3Parser::ParametersContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitTypedargslist(Python3Parser::TypedargslistContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitTfpdef(Python3Parser::TfpdefContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitTest(Python3Parser::TestContext *ctx) {
    return visit(ctx->or_test());
}

std::any EvalVisitor::visitOr_test(Python3Parser::Or_testContext *ctx) {
    if (ctx->OR().empty()) {
        return visit(ctx->and_test(0));
    }

    Value result = std::any_cast<Value>(visit(ctx->and_test(0)));
    if (asBool(toBool(result))) {
        return result; // Short-circuit
    }

    for (size_t i = 1; i < ctx->and_test().size(); i++) {
        Value next = std::any_cast<Value>(visit(ctx->and_test(i)));
        if (asBool(toBool(next))) {
            return next;
        }
    }

    return boolToValue(false);
}

std::any EvalVisitor::visitAnd_test(Python3Parser::And_testContext *ctx) {
    if (ctx->AND().empty()) {
        return visit(ctx->not_test(0));
    }

    Value result = std::any_cast<Value>(visit(ctx->not_test(0)));
    if (!asBool(toBool(result))) {
        return result; // Short-circuit
    }

    for (size_t i = 1; i < ctx->not_test().size(); i++) {
        Value next = std::any_cast<Value>(visit(ctx->not_test(i)));
        if (!asBool(toBool(next))) {
            return next;
        }
    }

    return result;
}

std::any EvalVisitor::visitNot_test(Python3Parser::Not_testContext *ctx) {
    if (ctx->NOT()) {
        Value val = std::any_cast<Value>(visit(ctx->not_test()));
        return boolToValue(!asBool(toBool(val)));
    } else {
        return visit(ctx->comparison());
    }
}

std::any EvalVisitor::visitComparison(Python3Parser::ComparisonContext *ctx) {
    if (ctx->comp_op().empty()) {
        return visit(ctx->arith_expr(0));
    }

    std::vector<Value> values;
    values.push_back(std::any_cast<Value>(visit(ctx->arith_expr(0))));

    for (size_t i = 1; i < ctx->arith_expr().size(); i++) {
        values.push_back(std::any_cast<Value>(visit(ctx->arith_expr(i))));
    }

    std::vector<std::string> ops;
    for (auto op : ctx->comp_op()) {
        ops.push_back(op->getText());
    }

    return applyComparison(values, ops);
}

std::any EvalVisitor::visitComp_op(Python3Parser::Comp_opContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitArith_expr(Python3Parser::Arith_exprContext *ctx) {
    if (ctx->addorsub_op().empty()) {
        return visit(ctx->term(0));
    }

    Value left = std::any_cast<Value>(visit(ctx->term(0)));

    for (size_t i = 0; i < ctx->addorsub_op().size(); i++) {
        std::string op = ctx->addorsub_op(i)->getText();
        Value right = std::any_cast<Value>(visit(ctx->term(i + 1)));
        left = applyBinaryOp(left, op, right);
    }

    return left;
}

std::any EvalVisitor::visitAddorsub_op(Python3Parser::Addorsub_opContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitTerm(Python3Parser::TermContext *ctx) {
    if (ctx->muldivmod_op().empty()) {
        return visit(ctx->factor(0));
    }

    Value left = std::any_cast<Value>(visit(ctx->factor(0)));

    for (size_t i = 0; i < ctx->muldivmod_op().size(); i++) {
        std::string op = ctx->muldivmod_op(i)->getText();
        Value right = std::any_cast<Value>(visit(ctx->factor(i + 1)));
        left = applyBinaryOp(left, op, right);
    }

    return left;
}

std::any EvalVisitor::visitMuldivmod_op(Python3Parser::Muldivmod_opContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitFactor(Python3Parser::FactorContext *ctx) {
    if (ctx->factor()) {
        std::string op = ctx->children[0]->getText();
        Value val = std::any_cast<Value>(visit(ctx->factor()));
        return applyUnaryOp(op, val);
    } else {
        return visit(ctx->atom_expr());
    }
}

std::any EvalVisitor::visitAtom_expr(Python3Parser::Atom_exprContext *ctx) {
    Value atom = std::any_cast<Value>(visit(ctx->atom()));

    if (ctx->trailer()) {
        // Function call
        auto trailer = ctx->trailer();
        if (trailer->OPEN_PAREN()) {
            // Extract function name from atom
            std::string funcName;
            if (auto atomCtx = ctx->atom()) {
                if (atomCtx->NAME()) {
                    funcName = atomCtx->NAME()->getText();
                }
            }

            std::vector<Value> args;
            std::unordered_map<std::string, Value> kwargs;

            if (trailer->arglist()) {
                auto arglist = trailer->arglist();
                for (auto arg : arglist->argument()) {
                    if (arg->ASSIGN()) {
                        // Keyword argument
                        std::string key = arg->test(0)->getText();
                        Value value = evaluateExpression(arg->test(1));
                        kwargs[key] = value;
                    } else {
                        // Positional argument
                        args.push_back(evaluateExpression(arg->test(0)));
                    }
                }
            }

            // Check if it's a built-in function
            if (funcName == "print") {
                return builtinPrint(args);
            } else if (funcName == "int") {
                return builtinInt(args);
            } else if (funcName == "float") {
                return builtinFloat(args);
            } else if (funcName == "str") {
                return builtinStr(args);
            } else if (funcName == "bool") {
                return builtinBool(args);
            }

            // User-defined function
            return executeFunctionCall(funcName, args, kwargs);
        }
    }

    return atom;
}

std::any EvalVisitor::visitTrailer(Python3Parser::TrailerContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitAtom(Python3Parser::AtomContext *ctx) {
    if (ctx->NAME()) {
        std::string name = ctx->NAME()->getText();
        // Check if it's a built-in function name
        if (name == "print" || name == "int" || name == "float" || name == "str" || name == "bool") {
            return strToValue(name);
        }
        // Check if it's a user-defined function
        if (functions.find(name) != functions.end()) {
            return strToValue(name);
        }
        return currentScope->getVariable(name);
    } else if (ctx->NUMBER()) {
        std::string numStr = ctx->NUMBER()->getText();
        if (numStr.find('.') != std::string::npos) {
            return floatToValue(std::stod(numStr));
        } else {
            // Use BigInt directly to handle arbitrarily large integers
            return intToValue(BigInt::fromString(numStr));
        }
    } else if (ctx->STRING().size() > 0) {
        std::string result;
        for (auto str : ctx->STRING()) {
            std::string strVal = str->getText();
            // Remove quotes
            strVal = strVal.substr(1, strVal.length() - 2);
            result += strVal;
        }
        return strToValue(result);
    } else if (ctx->format_string()) {
        return visit(ctx->format_string());
    } else if (ctx->NONE()) {
        return noneValue();
    } else if (ctx->TRUE()) {
        return boolToValue(true);
    } else if (ctx->FALSE()) {
        return boolToValue(false);
    } else if (ctx->OPEN_PAREN() && ctx->test()) {
        // Parenthesized expression
        return evaluateExpression(ctx->test());
    }

    return {};
}

std::any EvalVisitor::visitFormat_string(Python3Parser::Format_stringContext *ctx) {
    std::string result;

    for (auto child : ctx->children) {
        if (auto terminal = dynamic_cast<antlr4::tree::TerminalNode*>(child)) {
            std::string text = terminal->getText();
            if (text == "f\"" || text == "\"") {
                continue;
            } else if (text == "{{") {
                result += "{";
            } else if (text == "}}") {
                result += "}";
            } else if (terminal->getSymbol()->getType() == Python3Lexer::FORMAT_STRING_LITERAL) {
                result += text;
            }
        } else if (auto testlist = dynamic_cast<Python3Parser::TestlistContext*>(child)) {
            // Evaluate expression in {}
            Value val = evaluateExpression(testlist->test(0));
            result += valueToString(val);
        }
    }

    return strToValue(result);
}

std::any EvalVisitor::visitTestlist(Python3Parser::TestlistContext *ctx) {
    if (ctx->test().size() == 1) {
        return evaluateExpression(ctx->test(0));
    } else {
        TupleType tuple;
        for (auto test : ctx->test()) {
            tuple.push_back(evaluateExpression(test));
        }
        return Value(tuple);
    }
}

std::any EvalVisitor::visitArglist(Python3Parser::ArglistContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitArgument(Python3Parser::ArgumentContext *ctx) {
    return visitChildren(ctx);
}

std::any EvalVisitor::visitTerminal(antlr4::tree::TerminalNode *node) {
    return {};
}

Value EvalVisitor::evaluateExpression(Python3Parser::TestContext *ctx) {
    return std::any_cast<Value>(visit(ctx));
}

Value EvalVisitor::applyBinaryOp(const Value& left, const std::string& op, const Value& right) {
    // String operations
    if (isStr(left) && isStr(right)) {
        if (op == "+") {
            return strToValue(asStr(left) + asStr(right));
        }
    }

    if (isStr(left) && isInt(right) && op == "*") {
        std::string result;
        int times = static_cast<int>(asInt(right)->toDouble());
        for (int i = 0; i < times; i++) {
            result += asStr(left);
        }
        return strToValue(result);
    }

    // Arithmetic operations
    if (isInt(left) && isInt(right)) {
        auto leftInt = asInt(left);
        auto rightInt = asInt(right);

        if (op == "+") {
            return Value(std::make_shared<BigInt>((*leftInt) + (*rightInt)));
        } else if (op == "-") {
            return Value(std::make_shared<BigInt>((*leftInt) - (*rightInt)));
        } else if (op == "*") {
            return Value(std::make_shared<BigInt>((*leftInt) * (*rightInt)));
        } else if (op == "/") {
            // Float division
            return floatToValue(leftInt->toDouble() / rightInt->toDouble());
        } else if (op == "//") {
            // Integer division (floor division)
            return Value(std::make_shared<BigInt>((*leftInt) / (*rightInt)));
        } else if (op == "%") {
            return Value(std::make_shared<BigInt>((*leftInt) % (*rightInt)));
        }
    }

    // Mixed int/float operations
    if ((isInt(left) || isFloat(left)) && (isInt(right) || isFloat(right))) {
        double leftVal = isInt(left) ? asInt(left)->toDouble() : asFloat(left);
        double rightVal = isInt(right) ? asInt(right)->toDouble() : asFloat(right);

        if (op == "+") {
            return floatToValue(leftVal + rightVal);
        } else if (op == "-") {
            return floatToValue(leftVal - rightVal);
        } else if (op == "*") {
            return floatToValue(leftVal * rightVal);
        } else if (op == "/") {
            return floatToValue(leftVal / rightVal);
        } else if (op == "//") {
            return intToValue(static_cast<long long>(std::floor(leftVal / rightVal)));
        } else if (op == "%") {
            return floatToValue(std::fmod(leftVal, rightVal));
        }
    }

    // Comparison operations
    if (op == "==") {
        if (left.index() != right.index()) {
            // Try type conversion
            try {
                if (isInt(left) && isFloat(right)) {
                    return boolToValue(asInt(left)->toDouble() == asFloat(right));
                } else if (isFloat(left) && isInt(right)) {
                    return boolToValue(asFloat(left) == asInt(right)->toDouble());
                }
            } catch (...) {
                return boolToValue(false);
            }
            return boolToValue(false);
        }

        if (isInt(left)) {
            return boolToValue(*asInt(left) == *asInt(right));
        } else if (isFloat(left)) {
            return boolToValue(asFloat(left) == asFloat(right));
        } else if (isBool(left)) {
            return boolToValue(asBool(left) == asBool(right));
        } else if (isStr(left)) {
            return boolToValue(asStr(left) == asStr(right));
        } else if (isNone(left)) {
            return boolToValue(true);
        }
    } else if (op == "!=") {
        Value eq = applyBinaryOp(left, "==", right);
        return boolToValue(!asBool(eq));
    }

    // String comparison
    if (isStr(left) && isStr(right)) {
        if (op == "<") {
            return boolToValue(asStr(left) < asStr(right));
        } else if (op == ">") {
            return boolToValue(asStr(left) > asStr(right));
        } else if (op == "<=") {
            return boolToValue(asStr(left) <= asStr(right));
        } else if (op == ">=") {
            return boolToValue(asStr(left) >= asStr(right));
        }
    }

    // Numeric comparison
    if ((isInt(left) || isFloat(left)) && (isInt(right) || isFloat(right))) {
        double leftVal = isInt(left) ? asInt(left)->toDouble() : asFloat(left);
        double rightVal = isInt(right) ? asInt(right)->toDouble() : asFloat(right);

        if (op == "<") {
            return boolToValue(leftVal < rightVal);
        } else if (op == ">") {
            return boolToValue(leftVal > rightVal);
        } else if (op == "<=") {
            return boolToValue(leftVal <= rightVal);
        } else if (op == ">=") {
            return boolToValue(leftVal >= rightVal);
        }
    }

    throw std::runtime_error("Unsupported binary operation: " + op);
}

Value EvalVisitor::applyUnaryOp(const std::string& op, const Value& val) {
    if (op == "+") {
        return val;
    } else if (op == "-") {
        if (isInt(val)) {
            return Value(std::make_shared<BigInt>(-(*asInt(val))));
        } else if (isFloat(val)) {
            return floatToValue(-asFloat(val));
        }
    } else if (op == "not") {
        return boolToValue(!asBool(toBool(val)));
    }

    throw std::runtime_error("Unsupported unary operation: " + op);
}

Value EvalVisitor::applyComparison(const std::vector<Value>& values, const std::vector<std::string>& ops) {
    for (size_t i = 0; i < ops.size(); i++) {
        Value left = values[i];
        Value right = values[i + 1];
        Value result = applyBinaryOp(left, ops[i], right);

        if (!asBool(result)) {
            return boolToValue(false);
        }
    }

    return boolToValue(true);
}

Value EvalVisitor::executeFunctionCall(const std::string& name, const std::vector<Value>& args, const std::unordered_map<std::string, Value>& kwargs) {
    auto funcIt = functions.find(name);
    if (funcIt == functions.end()) {
        throw std::runtime_error("Undefined function: " + name);
    }

    auto func = funcIt->second;

    // Create new function scope
    auto funcScope = std::make_shared<Scope>(func->getClosure(), true);

    // Bind parameters
    const auto& params = func->getParameters();
    const auto& defaults = func->getDefaultValues();

    // First, bind positional arguments
    size_t argIndex = 0;
    for (size_t i = 0; i < params.size() && argIndex < args.size(); i++) {
        funcScope->setVariable(params[i], args[argIndex]);
        argIndex++;
    }

    // Then, bind keyword arguments
    for (const auto& [key, value] : kwargs) {
        auto it = std::find(params.begin(), params.end(), key);
        if (it != params.end()) {
            funcScope->setVariable(key, value);
        } else {
            throw std::runtime_error("Unexpected keyword argument: " + key);
        }
    }

    // Finally, use default values for remaining parameters
    size_t defaultIndex = 0;
    for (size_t i = argIndex; i < params.size(); i++) {
        if (defaultIndex < defaults.size()) {
            funcScope->setVariable(params[i], defaults[defaultIndex]);
            defaultIndex++;
        } else {
            throw std::runtime_error("Missing required argument: " + params[i]);
        }
    }

    // Save current scope and control states
    auto savedScope = currentScope;
    bool prevReturn = shouldReturn;
    bool prevBreak = shouldBreak;
    bool prevContinue = shouldContinue;

    // Set new scope and reset control states
    currentScope = funcScope;
    shouldReturn = false;
    shouldBreak = false;
    shouldContinue = false;

    // Execute function body
    try {
        visit(func->getBody());
    } catch (...) {
        // Restore scope and control states
        currentScope = savedScope;
        shouldReturn = prevReturn;
        shouldBreak = prevBreak;
        shouldContinue = prevContinue;
        throw;
    }

    // Get return value
    Value returnValue = lastValue;

    // Restore scope and control states
    currentScope = savedScope;
    shouldReturn = prevReturn;
    shouldBreak = prevBreak;
    shouldContinue = prevContinue;

    return returnValue;
}

std::string EvalVisitor::extractVariableName(Python3Parser::TestContext *ctx) {
    // For simple variable names, we can check if the test contains just an atom with a NAME
    // This is a simplified approach - we traverse the parse tree

    if (!ctx) return "";

    auto orTest = ctx->or_test();
    if (!orTest || orTest->and_test().size() != 1) return "";

    auto andTest = orTest->and_test(0);
    if (!andTest || andTest->not_test().size() != 1) return "";

    auto notTest = andTest->not_test(0);
    if (!notTest) return "";

    // Skip 'not' operator
    if (notTest->NOT()) {
        return "";
    }

    auto comparison = notTest->comparison();
    if (!comparison || !comparison->comp_op().empty() || comparison->arith_expr().size() != 1) {
        return "";
    }

    // Continue traversing down to find the atom
    auto arithExpr = comparison->arith_expr(0);
    if (!arithExpr || !arithExpr->addorsub_op().empty() || arithExpr->term().size() != 1) {
        return "";
    }

    auto term = arithExpr->term(0);
    if (!term || !term->muldivmod_op().empty() || term->factor().size() != 1) {
        return "";
    }

    auto factor = term->factor(0);
    if (!factor || factor->factor() || !factor->atom_expr()) {
        return "";
    }

    auto atomExpr = factor->atom_expr();
    if (!atomExpr || atomExpr->trailer()) {
        return "";
    }

    auto atom = atomExpr->atom();
    if (atom && atom->NAME()) {
        return atom->NAME()->getText();
    }

    return "";
}

// Built-in functions
Value EvalVisitor::builtinPrint(const std::vector<Value>& args) {
    bool first = true;
    for (const auto& arg : args) {
        if (!first) {
            std::cout << " ";
        }
        first = false;

        if (isFloat(arg)) {
            std::cout << std::fixed << std::setprecision(6) << asFloat(arg);
        } else if (isInt(arg)) {
            std::cout << asInt(arg)->toString();
        } else {
            std::cout << valueToString(arg);
        }
    }
    std::cout << std::endl;
    return noneValue();
}

Value EvalVisitor::builtinInt(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("int() takes exactly one argument");
    }
    return toInt(args[0]);
}

Value EvalVisitor::builtinFloat(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("float() takes exactly one argument");
    }
    return toFloat(args[0]);
}

Value EvalVisitor::builtinStr(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("str() takes exactly one argument");
    }
    return toStr(args[0]);
}

Value EvalVisitor::builtinBool(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("bool() takes exactly one argument");
    }
    return toBool(args[0]);
}