#pragma once
#ifndef PYTHON_INTERPRETER_EVALVISITOR_H
#define PYTHON_INTERPRETER_EVALVISITOR_H

#include "Python3ParserBaseVisitor.h"
#include "Value.h"
#include "Scope.h"
#include "Function.h"
#include <any>
#include <vector>
#include <string>
#include <memory>
#include <iostream>

class EvalVisitor : public Python3ParserBaseVisitor {
private:
    std::shared_ptr<Scope> currentScope;
    std::unordered_map<std::string, std::shared_ptr<Function>> functions;
    Value lastValue;
    bool shouldReturn = false;
    bool shouldBreak = false;
    bool shouldContinue = false;

public:
    EvalVisitor();

    // File input
    virtual std::any visitFile_input(Python3Parser::File_inputContext *ctx) override;

    // Statements
    virtual std::any visitStmt(Python3Parser::StmtContext *ctx) override;
    virtual std::any visitSimple_stmt(Python3Parser::Simple_stmtContext *ctx) override;
    virtual std::any visitSmall_stmt(Python3Parser::Small_stmtContext *ctx) override;
    virtual std::any visitExpr_stmt(Python3Parser::Expr_stmtContext *ctx) override;
    virtual std::any visitAugassign(Python3Parser::AugassignContext *ctx) override;
    virtual std::any visitFlow_stmt(Python3Parser::Flow_stmtContext *ctx) override;
    virtual std::any visitBreak_stmt(Python3Parser::Break_stmtContext *ctx) override;
    virtual std::any visitContinue_stmt(Python3Parser::Continue_stmtContext *ctx) override;
    virtual std::any visitReturn_stmt(Python3Parser::Return_stmtContext *ctx) override;
    virtual std::any visitCompound_stmt(Python3Parser::Compound_stmtContext *ctx) override;
    virtual std::any visitIf_stmt(Python3Parser::If_stmtContext *ctx) override;
    virtual std::any visitWhile_stmt(Python3Parser::While_stmtContext *ctx) override;
    virtual std::any visitSuite(Python3Parser::SuiteContext *ctx) override;

    // Function definition
    virtual std::any visitFuncdef(Python3Parser::FuncdefContext *ctx) override;
    virtual std::any visitParameters(Python3Parser::ParametersContext *ctx) override;
    virtual std::any visitTypedargslist(Python3Parser::TypedargslistContext *ctx) override;
    virtual std::any visitTfpdef(Python3Parser::TfpdefContext *ctx) override;

    // Expressions
    virtual std::any visitTest(Python3Parser::TestContext *ctx) override;
    virtual std::any visitOr_test(Python3Parser::Or_testContext *ctx) override;
    virtual std::any visitAnd_test(Python3Parser::And_testContext *ctx) override;
    virtual std::any visitNot_test(Python3Parser::Not_testContext *ctx) override;
    virtual std::any visitComparison(Python3Parser::ComparisonContext *ctx) override;
    virtual std::any visitComp_op(Python3Parser::Comp_opContext *ctx) override;
    virtual std::any visitArith_expr(Python3Parser::Arith_exprContext *ctx) override;
    virtual std::any visitAddorsub_op(Python3Parser::Addorsub_opContext *ctx) override;
    virtual std::any visitTerm(Python3Parser::TermContext *ctx) override;
    virtual std::any visitMuldivmod_op(Python3Parser::Muldivmod_opContext *ctx) override;
    virtual std::any visitFactor(Python3Parser::FactorContext *ctx) override;
    virtual std::any visitAtom_expr(Python3Parser::Atom_exprContext *ctx) override;
    virtual std::any visitTrailer(Python3Parser::TrailerContext *ctx) override;
    virtual std::any visitAtom(Python3Parser::AtomContext *ctx) override;
    virtual std::any visitFormat_string(Python3Parser::Format_stringContext *ctx) override;
    virtual std::any visitTestlist(Python3Parser::TestlistContext *ctx) override;
    virtual std::any visitArglist(Python3Parser::ArglistContext *ctx) override;
    virtual std::any visitArgument(Python3Parser::ArgumentContext *ctx) override;

    // Terminal nodes
    virtual std::any visitTerminal(antlr4::tree::TerminalNode *node) override;

    // Helper methods
    Value evaluateExpression(Python3Parser::TestContext *ctx);
    Value applyBinaryOp(const Value& left, const std::string& op, const Value& right);
    Value applyUnaryOp(const std::string& op, const Value& val);
    Value applyComparison(const std::vector<Value>& values, const std::vector<std::string>& ops);
    void executeAssignment(const std::vector<std::string>& names, const Value& value);
    Value executeFunctionCall(const std::string& name, const std::vector<Value>& args, const std::unordered_map<std::string, Value>& kwargs);
    std::string formatString(const std::string& formatStr, const std::vector<Value>& values);
    std::string extractVariableName(Python3Parser::TestContext *ctx);

    // Built-in functions
    Value builtinPrint(const std::vector<Value>& args);
    Value builtinInt(const std::vector<Value>& args);
    Value builtinFloat(const std::vector<Value>& args);
    Value builtinStr(const std::vector<Value>& args);
    Value builtinBool(const std::vector<Value>& args);
};

#endif // PYTHON_INTERPRETER_EVALVISITOR_H
