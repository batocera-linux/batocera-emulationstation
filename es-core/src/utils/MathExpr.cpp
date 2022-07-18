// Source: http://www.daniweb.com/software-development/cpp/code/427500/calculator-using-shunting-yard-algorithm#
// Source2: https://github.com/bamos/cpp-expression-parser
// Author: Jesse Brown
// Modifications: Brandon Amos
// Modifications: www.K3A.me (changed token class, float numbers, added support for boolean operators, added support for strings)
// Licence: MIT

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <math.h>
#include <stdio.h>

#include "MathExpr.h"

namespace Utils
{
	float MathExpr::Value::toNumber()
	{
		if (isToken()) return 0;
		if (isNumber()) return number;

		number = atof(string.c_str());
		type |= NUMBER;

		return number;
	}

	std::string MathExpr::Value::toString()
	{
		if (isToken()) return string;
		if (isString()) return string;

		char str[16];
		sprintf(str, "%f", number);
		string = str;
		type |= STRING;

		return string;
	}

	MathExpr::MathExpr()
	{
		// 1. Create the operator precedence map.
		opPrecedence["("] = -10;
		opPrecedence["&&"] = -2; opPrecedence["||"] = -3;
		opPrecedence[">"] = -1; opPrecedence[">="] = -1;
		opPrecedence["<"] = -1; opPrecedence["<="] = -1;
		opPrecedence["=="] = -1; opPrecedence["!="] = -1;
		opPrecedence["<<"] = 1; opPrecedence[">>"] = 1;
		opPrecedence["+"] = 2; opPrecedence["-"] = 2;
		opPrecedence["*"] = 3; opPrecedence["/"] = 3;
		opPrecedence["^"] = 4;
		opPrecedence["!"] = 5;
	}

#define isvariablechar(c) (isalpha(c) || c == '_')

	MathExpr::ValuePtrQueue MathExpr::toRPN(const char* expr, ValueMap* vars, IntMap opPrecedence)
	{
		ValuePtrQueue rpnQueue; std::stack<std::string> operatorStack;
		bool lastTokenWasOp = true;

		// In one pass, ignore whitespace and parse the expression into RPN
		// using Dijkstra's Shunting-yard algorithm.
		while (*expr && isspace(*expr)) ++expr;

		while (*expr)
		{
			if (isdigit(*expr))
			{
				// If the token is a number, add it to the output queue.
				char* nextChar = 0;
				float digit = strtod(expr, &nextChar);

				rpnQueue.push(new Value(digit));
				expr = nextChar;
				lastTokenWasOp = false;
			}
			else if (isvariablechar(*expr) || *expr == '{' || *expr == '$')
			{
				// If the function is a variable, resolve it and
				// add the parsed number to the output queue.
				if (!vars)
					throw std::domain_error("Detected variable, but the variable map is null.");

				std::stringstream ss;

				if (*expr == '$' && *(expr+1) == '{')
					expr++;

				if (*expr == '{')
				{
					++expr;
					while (*expr != '}' && *expr != 0)
					{
						ss << *expr;
						++expr;
					}

					if (*expr == '}')
						expr++;
				}
				else
				{
					ss << *expr;
					++expr;
					while (isvariablechar(*expr))
					{
						ss << *expr;
						++expr;
					}
				}

				std::string key = ss.str();
				if (key == "true")
					rpnQueue.push(new Value(1));
				else if (key == "false")
					rpnQueue.push(new Value(0));
				else {
					ValueMap::iterator it = vars->find(key);
					if (it == vars->end())
						throw std::domain_error("Unable to find the variable '" + key + "'.");

					rpnQueue.push(new Value(it->second));
				}

				lastTokenWasOp = false;
			}
			else if (*expr == '\'' || *expr == '"')
			{
				// It's a string value

				char startChr = *expr;

				std::stringstream ss;
				++expr;
				while (*expr && *expr != startChr)
				{
					ss << *expr;
					++expr;
				}
				if (*expr) expr++;

				rpnQueue.push(new Value(ss.str()));
				lastTokenWasOp = false;
			}
			else
			{
				// Otherwise, the variable is an operator or paranthesis.
				switch (*expr)
				{
				case '(':
					operatorStack.push("(");
					++expr;
					break;
				case ')':
					while (operatorStack.top().compare("("))
					{
						rpnQueue.push(new Value(operatorStack.top(), TOKEN));
						operatorStack.pop();
					}
					operatorStack.pop();
					++expr;
					break;
				default:
				{
					// The token is an operator.
					//
					// Let p(o) denote the precedence of an operator o.
					//
					// If the token is an operator, o1, then
					//   While there is an operator token, o2, at the top
					//       and p(o1) <= p(o2), then
					//     pop o2 off the stack onto the output queue.
					//   Push o1 on the stack.
					std::stringstream ss;
					ss << *expr;
					++expr;
					while (*expr && !isspace(*expr) && !isdigit(*expr) && !isvariablechar(*expr) && *expr != '(' && *expr != ')')
					{
						ss << *expr;
						++expr;
					}

					ss.clear();
					std::string str;
					ss >> str;

					if (lastTokenWasOp)
					{
						// Convert unary operators to binary in the RPN.
						if (!str.compare("-") || !str.compare("+") || !str.compare("!"))
							rpnQueue.push(new Value(0));
						else
							throw std::domain_error("Unrecognized unary operator: '" + str + "'");

					}

					while (!operatorStack.empty() && opPrecedence[str] <= opPrecedence[operatorStack.top()])
					{
						rpnQueue.push(new Value(operatorStack.top(), TOKEN));
						operatorStack.pop();
					}
					operatorStack.push(str);
					lastTokenWasOp = true;
				}
				}
			}
			while (*expr && isspace(*expr)) ++expr;
		}
		while (!operatorStack.empty())
		{
			rpnQueue.push(new Value(operatorStack.top(), TOKEN));
			operatorStack.pop();
		}
		return rpnQueue;
	}

	MathExpr::Value MathExpr::eval(const char* expr, ValueMap* vars) 
	{
		// Convert to RPN with Dijkstra's Shunting-yard algorithm.
		ValuePtrQueue rpn = toRPN(expr, vars, opPrecedence);

		// Evaluate the expression in RPN form.
		ValueStack evaluation;

		while (!rpn.empty()) 
		{
			Value* tok = rpn.front();
			rpn.pop();

			if (tok->isToken())
			{
				std::string str = tok->string;
				if (evaluation.size() < 2) {
					throw std::domain_error("Invalid equation.");
				}
				Value right = evaluation.top(); evaluation.pop();
				Value left = evaluation.top(); evaluation.pop();
				if (!str.compare("+") && left.isNumber())
					evaluation.push(left.number + right.toNumber());
				if (!str.compare("+") && left.isString())
					evaluation.push(left.string + right.toString());
				else if (!str.compare("*"))
					evaluation.push(left.toNumber() * right.toNumber());
				else if (!str.compare("-"))
					evaluation.push(left.toNumber() - right.toNumber());
				else if (!str.compare("/"))
				{
					float r = right.toNumber();
					if (r == 0)
						evaluation.push(0);
					else
						evaluation.push(left.toNumber() / r);
				}
				else if (!str.compare("<<"))
					evaluation.push((int)left.toNumber() << (int)right.toNumber());
				else if (!str.compare("^"))
					evaluation.push(pow(left.toNumber(), right.toNumber()));
				else if (!str.compare(">>"))
					evaluation.push((int)left.toNumber() >> (int)right.toNumber());
				else if (!str.compare(">"))
					evaluation.push(left.toNumber() > right.toNumber());
				else if (!str.compare(">="))
					evaluation.push(left.toNumber() >= right.toNumber());
				else if (!str.compare("<"))
					evaluation.push(left.toNumber() < right.toNumber());
				else if (!str.compare("<="))
					evaluation.push(left.toNumber() <= right.toNumber());
				else if (!str.compare("&&"))
					evaluation.push(left.toNumber() && right.toNumber());
				else if (!str.compare("||"))
					evaluation.push(left.toNumber() || right.toNumber());
				else if (!str.compare("=="))
				{
					if (left.isNumber() && right.isNumber())
						evaluation.push(left.number == right.number);
					else if (left.isString() && right.isString())
						evaluation.push(left.string == right.string);
					else if (left.isString())
						evaluation.push(left.string == right.toString());
					else
						evaluation.push(left.toNumber() == right.toNumber());
				}
				else if (!str.compare("!="))
				{
					if (left.isNumber() && right.isNumber())
						evaluation.push(left.number != right.number);
					else if (left.isString() && right.isString())
						evaluation.push(left.string != right.string);
					else if (left.isString())
						evaluation.push(left.string != right.toString());
					else
						evaluation.push(left.toNumber() != right.toNumber());
				}
				else if (!str.compare("!"))
					evaluation.push(!right.toNumber());
				else
					throw std::domain_error("Unknown operator: " + left.toString() + " " + str + " " + right.toString() + ".");
			}
			else if (tok->isNumber() || tok->isString())
			{
				evaluation.push(*tok);
			}
			else
			{
				throw std::domain_error("Invalid token '" + tok->toString() + "'.");
			}

			delete tok;
		}
		return evaluation.top();
	}
}