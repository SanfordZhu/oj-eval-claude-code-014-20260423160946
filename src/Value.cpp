#include "Value.h"
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <iostream>

// BigInt implementation
BigInt::BigInt(long long num) : negative(false) {
    if (num < 0) {
        negative = true;
        num = -num;
    }

    if (num == 0) {
        digits.push_back(0);
    } else {
        while (num > 0) {
            digits.push_back(num % 10);
            num /= 10;
        }
    }
}

BigInt::BigInt(const std::string& str) : negative(false) {
    size_t start = 0;
    if (str[0] == '-') {
        negative = true;
        start = 1;
    }

    for (int i = str.length() - 1; i >= static_cast<int>(start); i--) {
        if (std::isdigit(str[i])) {
            digits.push_back(str[i] - '0');
        }
    }

    normalize();
}

void BigInt::normalize() {
    while (digits.size() > 1 && digits.back() == 0) {
        digits.pop_back();
    }

    if (digits.size() == 1 && digits[0] == 0) {
        negative = false;
    }
}

BigInt BigInt::operator+(const BigInt& other) const {
    if (negative != other.negative) {
        if (negative) {
            BigInt temp = *this;
            temp.negative = false;
            return other - temp;
        } else {
            BigInt temp = other;
            temp.negative = false;
            return *this - temp;
        }
    }

    BigInt result;
    result.negative = negative;

    // Clear the default zero
    if (result.digits.size() == 1 && result.digits[0] == 0) {
        result.digits.clear();
    }

    int carry = 0;
    size_t maxSize = std::max(digits.size(), other.digits.size());

    for (size_t i = 0; i < maxSize; i++) {
        int sum = carry;
        if (i < digits.size()) sum += digits[i];
        if (i < other.digits.size()) sum += other.digits[i];

        result.digits.push_back(sum % 10);
        carry = sum / 10;
    }

    // Handle remaining carry
    if (carry) {
        result.digits.push_back(carry);
    }

    // Normalize the result
    result.normalize();

    return result;
}

BigInt BigInt::operator-(const BigInt& other) const {
    if (negative != other.negative) {
        BigInt temp = other;
        temp.negative = !temp.negative;
        return *this + temp;
    }

    if (negative) {
        BigInt temp1 = *this;
        BigInt temp2 = other;
        temp1.negative = false;
        temp2.negative = false;
        return temp2 - temp1;
    }

    if (*this < other) {
        BigInt result = other - *this;
        result.negative = true;
        return result;
    }

    BigInt result;
    // Clear the default zero
    if (result.digits.size() == 1 && result.digits[0] == 0) {
        result.digits.clear();
    }
    int borrow = 0;

    for (size_t i = 0; i < digits.size(); i++) {
        int diff = digits[i] - borrow;
        if (i < other.digits.size()) diff -= other.digits[i];

        if (diff < 0) {
            diff += 10;
            borrow = 1;
        } else {
            borrow = 0;
        }

        result.digits.push_back(diff);
    }

    result.normalize();
    return result;
}

BigInt BigInt::operator*(const BigInt& other) const {
    BigInt result;
    result.digits.resize(digits.size() + other.digits.size(), 0);

    for (size_t i = 0; i < digits.size(); i++) {
        int carry = 0;
        for (size_t j = 0; j < other.digits.size() || carry; j++) {
            int product = result.digits[i + j] + digits[i] * (j < other.digits.size() ? other.digits[j] : 0) + carry;
            result.digits[i + j] = product % 10;
            carry = product / 10;
        }
    }

    result.negative = (negative != other.negative);
    result.normalize();
    return result;
}

BigInt BigInt::operator/(const BigInt& other) const {
    if (other.isZero()) {
        throw std::runtime_error("Division by zero");
    }

    BigInt dividend = *this;
    BigInt divisor = other;
    dividend.negative = false;
    divisor.negative = false;

    BigInt quotient;
    BigInt remainder;

    for (int i = dividend.digits.size() - 1; i >= 0; i--) {
        remainder.digits.insert(remainder.digits.begin(), dividend.digits[i]);
        remainder.normalize();

        int count = 0;
        while (remainder >= divisor) {
            remainder = remainder - divisor;
            count++;
        }

        quotient.digits.insert(quotient.digits.begin(), count);
    }

    quotient.negative = (negative != other.negative);
    quotient.normalize();
    return quotient;
}

BigInt BigInt::operator%(const BigInt& other) const {
    BigInt quotient = *this / other;
    BigInt product = quotient * other;
    return *this - product;
}

bool BigInt::operator<(const BigInt& other) const {
    if (negative != other.negative) {
        return negative;
    }

    if (digits.size() != other.digits.size()) {
        return negative ? (digits.size() > other.digits.size()) : (digits.size() < other.digits.size());
    }

    for (int i = digits.size() - 1; i >= 0; i--) {
        if (digits[i] != other.digits[i]) {
            return negative ? (digits[i] > other.digits[i]) : (digits[i] < other.digits[i]);
        }
    }

    return false;
}

bool BigInt::operator>(const BigInt& other) const {
    return other < *this;
}

bool BigInt::operator<=(const BigInt& other) const {
    return !(other < *this);
}

bool BigInt::operator>=(const BigInt& other) const {
    return !(*this < other);
}

bool BigInt::operator==(const BigInt& other) const {
    return negative == other.negative && digits == other.digits;
}

bool BigInt::operator!=(const BigInt& other) const {
    return !(*this == other);
}

BigInt BigInt::operator-() const {
    BigInt result = *this;
    if (!isZero()) {
        result.negative = !negative;
    }
    return result;
}

std::string BigInt::toString() const {
    std::string result;
    if (negative && !isZero()) {
        result += "-";
    }

    // digits are stored in reverse order, so iterate backwards
    for (int i = digits.size() - 1; i >= 0; i--) {
        result += ('0' + digits[i]);
    }

    return result;
}

double BigInt::toDouble() const {
    double result = 0.0;
    double power = 1.0;

    for (size_t i = 0; i < digits.size(); i++) {
        result += digits[i] * power;
        power *= 10.0;
    }

    return negative ? -result : result;
}

bool BigInt::isZero() const {
    return digits.size() == 1 && digits[0] == 0;
}

bool BigInt::isNegative() const {
    return negative;
}

BigInt BigInt::fromString(const std::string& str) {
    return BigInt(str);
}

// Helper functions implementation
std::string valueToString(const Value& val) {
    if (isInt(val)) {
        return asInt(val)->toString();
    } else if (isFloat(val)) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << asFloat(val);
        std::string result = oss.str();
        // Remove trailing zeros
        result.erase(result.find_last_not_of('0') + 1, std::string::npos);
        if (result.back() == '.') {
            result += "0";
        }
        return result;
    } else if (isBool(val)) {
        return asBool(val) ? "True" : "False";
    } else if (isStr(val)) {
        return asStr(val);
    } else if (isNone(val)) {
        return "None";
    } else if (isTuple(val)) {
        std::string result = "(";
        const auto& tuple = asTuple(val);
        for (size_t i = 0; i < tuple.size(); i++) {
            if (i > 0) result += ", ";
            result += valueToString(std::any_cast<Value>(tuple[i]));
        }
        if (tuple.size() == 1) result += ",";
        result += ")";
        return result;
    }
    return "";
}

Value intToValue(long long val) {
    return std::make_shared<BigInt>(val);
}

Value intToValue(const BigInt& val) {
    return std::make_shared<BigInt>(val);
}

Value floatToValue(double val) {
    return val;
}

Value boolToValue(bool val) {
    return val;
}

Value strToValue(const std::string& val) {
    return val;
}

Value noneValue() {
    return nullptr;
}

bool isInt(const Value& val) {
    return std::holds_alternative<IntType>(val);
}

bool isFloat(const Value& val) {
    return std::holds_alternative<FloatType>(val);
}

bool isBool(const Value& val) {
    return std::holds_alternative<BoolType>(val);
}

bool isStr(const Value& val) {
    return std::holds_alternative<StrType>(val);
}

bool isNone(const Value& val) {
    return std::holds_alternative<NoneType>(val);
}

bool isTuple(const Value& val) {
    return std::holds_alternative<TupleType>(val);
}

IntType asInt(const Value& val) {
    return std::get<IntType>(val);
}

FloatType asFloat(const Value& val) {
    return std::get<FloatType>(val);
}

BoolType asBool(const Value& val) {
    return std::get<BoolType>(val);
}

StrType asStr(const Value& val) {
    return std::get<StrType>(val);
}

TupleType asTuple(const Value& val) {
    return std::get<TupleType>(val);
}

Value toInt(const Value& val) {
    if (isInt(val)) {
        return val;
    } else if (isFloat(val)) {
        return intToValue(static_cast<long long>(asFloat(val)));
    } else if (isBool(val)) {
        return intToValue(asBool(val) ? 1 : 0);
    } else if (isStr(val)) {
        const std::string& str = asStr(val);
        try {
            // Try to parse as integer first
            return intToValue(std::stoll(str));
        } catch (...) {
            // If fails, parse as float and convert to int
            return intToValue(static_cast<long long>(std::stod(str)));
        }
    } else if (isNone(val)) {
        return intToValue(0);
    }
    throw std::runtime_error("Cannot convert to int");
}

Value toFloat(const Value& val) {
    if (isFloat(val)) {
        return val;
    } else if (isInt(val)) {
        return floatToValue(asInt(val)->toDouble());
    } else if (isBool(val)) {
        return floatToValue(asBool(val) ? 1.0 : 0.0);
    } else if (isStr(val)) {
        const std::string& str = asStr(val);
        return floatToValue(std::stod(str));
    } else if (isNone(val)) {
        return floatToValue(0.0);
    }
    throw std::runtime_error("Cannot convert to float");
}

Value toBool(const Value& val) {
    if (isBool(val)) {
        return val;
    } else if (isInt(val)) {
        return boolToValue(!asInt(val)->isZero());
    } else if (isFloat(val)) {
        return boolToValue(asFloat(val) != 0.0);
    } else if (isStr(val)) {
        return boolToValue(!asStr(val).empty());
    } else if (isNone(val)) {
        return boolToValue(false);
    } else if (isTuple(val)) {
        return boolToValue(!asTuple(val).empty());
    }
    throw std::runtime_error("Cannot convert to bool");
}

Value toStr(const Value& val) {
    if (isStr(val)) {
        return val;
    } else {
        return strToValue(valueToString(val));
    }
}