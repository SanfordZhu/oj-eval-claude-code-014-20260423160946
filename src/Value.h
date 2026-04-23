#pragma once
#ifndef PYTHON_INTERPRETER_VALUE_H
#define PYTHON_INTERPRETER_VALUE_H

#include <string>
#include <memory>
#include <vector>
#include <variant>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <any>

// Forward declaration
class BigInt;

// Python value types
using IntType = std::shared_ptr<BigInt>;
using FloatType = double;
using BoolType = bool;
using StrType = std::string;
using NoneType = std::nullptr_t;

// Tuple type (for multiple return values)
using TupleType = std::vector<std::any>;

// Value variant
using Value = std::variant<IntType, FloatType, BoolType, StrType, NoneType, TupleType>;

// BigInt class for arbitrary precision integers
class BigInt {
private:
    std::vector<int> digits; // Store digits in reverse order
    bool negative;

    void normalize();

public:
    BigInt() : negative(false) { digits.push_back(0); }
    BigInt(long long num);
    BigInt(const std::string& str);

    BigInt operator+(const BigInt& other) const;
    BigInt operator-(const BigInt& other) const;
    BigInt operator*(const BigInt& other) const;
    BigInt operator/(const BigInt& other) const;
    BigInt operator%(const BigInt& other) const;

    bool operator<(const BigInt& other) const;
    bool operator>(const BigInt& other) const;
    bool operator<=(const BigInt& other) const;
    bool operator>=(const BigInt& other) const;
    bool operator==(const BigInt& other) const;
    bool operator!=(const BigInt& other) const;

    BigInt operator-() const;

    std::string toString() const;
    double toDouble() const;
    bool isZero() const;
    bool isNegative() const;

    static BigInt fromString(const std::string& str);
};

// Helper functions for type conversions
std::string valueToString(const Value& val);
Value intToValue(long long val);
Value intToValue(const BigInt& val);
Value floatToValue(double val);
Value boolToValue(bool val);
Value strToValue(const std::string& val);
Value noneValue();

// Type checking functions
bool isInt(const Value& val);
bool isFloat(const Value& val);
bool isBool(const Value& val);
bool isStr(const Value& val);
bool isNone(const Value& val);
bool isTuple(const Value& val);

// Type extraction functions
IntType asInt(const Value& val);
FloatType asFloat(const Value& val);
BoolType asBool(const Value& val);
StrType asStr(const Value& val);
TupleType asTuple(const Value& val);

// Type conversion functions
Value toInt(const Value& val);
Value toFloat(const Value& val);
Value toBool(const Value& val);
Value toStr(const Value& val);

#endif // PYTHON_INTERPRETER_VALUE_H