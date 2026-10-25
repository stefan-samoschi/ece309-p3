// src/tools/calculator_eval.cpp — YOU IMPLEMENT (spec §4.6).
//
// calc_evaluate: evaluate a well-formed token list with two stacks
// (one for numbers, one for operators and '('). See spec §4.6 and the
// worked trace in Appendix C. std::stack or std::vector are both fine.

#include "tools/calculator.h"

#include <vector>

namespace {
int precedence(CalcToken::Kind kind) {
    switch (kind) {
        case CalcToken::Kind::Plus:
        case CalcToken::Kind::Minus:
            return 1;
        case CalcToken::Kind::Times:
        case CalcToken::Kind::Divide:
            return 2;
        default:
            return -1;
    }
}

bool apply_op(std::vector<double>& values, std::vector<CalcToken::Kind>& ops,
              std::string* error) {
    if (values.size() < 2 || ops.empty()) {
        if (error) *error = "malformed expression";
        return false;
    }

    const CalcToken::Kind op = ops.back();
    ops.pop_back();
    const double right = values.back();
    values.pop_back();
    const double left = values.back();
    values.pop_back();

    double result = 0.0;
    switch (op) {
        case CalcToken::Kind::Plus:
            result = left + right;
            break;
        case CalcToken::Kind::Minus:
            result = left - right;
            break;
        case CalcToken::Kind::Times:
            result = left * right;
            break;
        case CalcToken::Kind::Divide:
            if (right == 0.0) {
                if (error) *error = "division by zero";
                return false;
            }
            result = left / right;
            break;
        default:
            if (error) *error = "malformed expression";
            return false;
    }

    values.push_back(result);
    return true;
}
}  // namespace

bool calc_evaluate(const std::vector<CalcToken>& tokens, double* out,
                   std::string* error) {
    std::vector<double> values;
    std::vector<CalcToken::Kind> ops;

    for (const CalcToken& token : tokens) {
        switch (token.kind) {
            case CalcToken::Kind::Number:
                values.push_back(token.value);
                break;

            case CalcToken::Kind::LParen:
                ops.push_back(CalcToken::Kind::LParen);
                break;

            case CalcToken::Kind::RParen:
                while (!ops.empty() && ops.back() != CalcToken::Kind::LParen) {
                    if (!apply_op(values, ops, error)) return false;
                }
                if (ops.empty()) {
                    if (error) *error = "mismatched parentheses";
                    return false;
                }
                ops.pop_back();
                break;

            case CalcToken::Kind::Plus:
            case CalcToken::Kind::Minus:
            case CalcToken::Kind::Times:
            case CalcToken::Kind::Divide:
                while (!ops.empty() && ops.back() != CalcToken::Kind::LParen &&
                       precedence(ops.back()) >= precedence(token.kind)) {
                    if (!apply_op(values, ops, error)) return false;
                }
                ops.push_back(token.kind);
                break;

            default:
                if (error) *error = "malformed expression";
                return false;
        }
    }

    while (!ops.empty()) {
        if (ops.back() == CalcToken::Kind::LParen) {
            if (error) *error = "mismatched parentheses";
            return false;
        }
        if (!apply_op(values, ops, error)) return false;
    }

    if (values.size() != 1) {
        if (error) *error = "malformed expression";
        return false;
    }

    *out = values.back();
    return true;
}
