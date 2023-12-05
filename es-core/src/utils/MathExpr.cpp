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
#include "StringUtil.h"
#include "FileSystemUtil.h"

namespace Utils
{
	#define isvariablechar(c) (isalpha(c) || c == '_')

	class Expression
	{
	public:
		Expression() 
		{
			truePart = nullptr;
			falsePart = nullptr;
		}

		~Expression()
		{
			if (truePart != nullptr)
				delete truePart;

			if (falsePart != nullptr)
				falsePart = nullptr;
		}

		std::string expression;
		Expression* truePart;
		Expression* falsePart;

		std::string fullExpression;

		std::string toString()
		{
			if (truePart != nullptr && falsePart != nullptr)
				return expression + " ? " + truePart->expression + " : " + falsePart->expression;

			return expression;
		}
	};
	
	static Expression* parseConditionalExpression(const std::string& input) 
	{
		Expression* expression = new Expression;

		// Find the position of the first '?'
		size_t conditionEnd = input.find('?');
		if (conditionEnd != std::string::npos)
		{
			size_t start = conditionEnd > 0 ? conditionEnd - 1 : conditionEnd;

			bool inQuote = false;
			bool inChar = false;
			int parenth = 0;

			bool foundVar = false;
			bool exitOnCloseP = false;

			while (start > 0 && isspace(input[start])) start--;

			while (start > 0)
			{
				char c = input[start];

				if (c == '\"' && !inChar)
					inQuote = !inQuote;
				else if (c == '\'' && !inQuote)
					inChar = !inChar;
				else if (c == '(' && !inQuote && !inChar)
				{
					parenth--;
					if (parenth < 0)
						break;
					else if (parenth == 0 && exitOnCloseP)
						break;
				}
				else if (c == ')' && !inQuote && !inChar)
				{
					if (parenth == 0 && !foundVar)
						exitOnCloseP = true;

					parenth++;
				}
				else if (!inQuote && !inChar && parenth == 0)
				{
					if (isdigit(c) || isvariablechar(c) || c == '{' || c == '$')
						foundVar = true;
					else if (foundVar)
						break;
				}

				start--;
				if (start == 0 && input[0] == '(')
				{
					start++;
					break;
				}
			}			

			// Extract the condition part
			expression->expression = Utils::String::trim(input.substr(start, conditionEnd - start));

			size_t truePartEnd = std::string::npos;

			inQuote = false;
			inChar = false;
			parenth = 0;

			const char* chars = input.c_str();
			size_t idx = conditionEnd + 1;
			while (true)
			{
				char c = *(chars + idx);
				if (c == 0)
					break;

				if (c == '\"' && !inChar)
					inQuote = !inQuote;
				else if (c == '\'' && !inQuote)
					inChar = !inChar;
				else if (c == '(' && !inQuote && !inChar)
					parenth++;
				else if (c == ')' && !inQuote && !inChar)
					parenth--;
				else if (c == ':' && !inQuote && !inChar && parenth == 0)
				{
					truePartEnd = idx;
					break;
				}

				idx++;
			}

			if (truePartEnd == std::string::npos)
				return expression;

			std::string truePart = Utils::String::trim(input.substr(conditionEnd + 1, truePartEnd - conditionEnd - 1));
			expression->truePart = parseConditionalExpression(truePart);


			size_t falsePartStart = truePartEnd + 1;
			size_t falsePartEnd = falsePartStart;

			while (falsePartEnd < input.size() && isspace(input[falsePartEnd])) falsePartEnd++;

			inQuote = false;
			inChar = false;
			parenth = 0;

			while (falsePartEnd < input.size())
			{
				char c = input[falsePartEnd];
				if (c == 0)
					break;

				if (c == '\"' && !inChar)
					inQuote = !inQuote;
				else if (c == '\'' && !inQuote)
					inChar = !inChar;
				else if (c == '(' && !inQuote && !inChar)
					parenth++;
				else if (c == ')' && !inQuote && !inChar)
				{
					parenth--;
					if (parenth < 0)
						break;
				}
				else if (c == ':' && !inQuote && !inChar && parenth == 0)
				{
					falsePartEnd = idx;
					break;
				}

				falsePartEnd++;
			}

			std::string falsePart = Utils::String::trim(input.substr(falsePartStart, falsePartEnd - falsePartStart));
			expression->falsePart = parseConditionalExpression(falsePart);

			// Store the full original expression
			expression->fullExpression = Utils::String::trim(input.substr(start, falsePartEnd - start));
		}
		else
			expression->expression = input;

		return expression;
	}

	class Method 
	{
	public:
		std::string name;
		std::vector<std::string> arguments;

		std::string fullExpression;
	};

	static std::vector<Method> extractMethods(const std::string& expression) 
	{
		std::vector<Method> methods;
	
		bool insideString = false;
		bool insideChar = false;

		int nameStart = 0;

		for (int i = 0; i < expression.size(); i++) 
		{
			char currentChar = expression[i];

			if (currentChar == '"' && !insideChar)
				insideString = !insideString;
			else if (currentChar == '\'' && !insideString)
				insideChar = !insideChar;			
			else if (currentChar == '(' && !insideString && !insideChar)
			{
				Method currentMethod;
				currentMethod.name = expression.substr(nameStart, i - nameStart);

				int argStart = i + 1;
				for (int a = i + 1; a < expression.size(); a++)
				{
					currentChar = expression[a];

					if (currentChar == '"' && !insideChar)
						insideString = !insideString;
					else if (currentChar == '\'' && !insideString)
						insideChar = !insideChar;
					else if (currentChar == ',' && !insideString && !insideChar)
					{
						auto arg = Utils::String::trim(expression.substr(argStart, a - argStart));
						currentMethod.arguments.push_back(arg);
						argStart = a + 1;
					}
					else if (currentChar == ')' && !insideString && !insideChar)
					{
						auto arg = Utils::String::trim(expression.substr(argStart, a - argStart));
						currentMethod.arguments.push_back(arg);

						currentMethod.fullExpression = expression.substr(nameStart, a - nameStart + 1);
						break;
					}
				}

				if (currentMethod.fullExpression.empty())
					break;

				methods.push_back(currentMethod);
			}
			else if (!isvariablechar(currentChar))
				nameStart = i + 1;
		}

		return methods;
	}

	static std::string removeStringQuotes(const std::string& value)
	{
		if (value.size() > 1 && value[0] == '"' && value[value.size() - 1] == '"')
			return value.substr(1, value.size() - 2);

		return value;
	}

	std::string MathExpr::evaluateMethods(const std::string& expr, ValueMap* vars)
	{
		std::string evalxp = expr;

		for (auto method : extractMethods(expr))
		{
			if (method.name == "exists" && method.arguments.size() == 1)
				evalxp = Utils::String::replace(evalxp, method.fullExpression, Utils::FileSystem::exists(removeStringQuotes(method.arguments[0])) ? "1" : "0");
			else if (method.name == "empty" && method.arguments.size() == 1)
				evalxp = Utils::String::replace(evalxp, method.fullExpression, removeStringQuotes(method.arguments[0]).empty() ? "1" : "0");
			else if (method.name == "lower" && method.arguments.size() == 1)
				evalxp = Utils::String::replace(evalxp, method.fullExpression, Utils::String::toLower(method.arguments[0]));
			else if (method.name == "upper" && method.arguments.size() == 1)
				evalxp = Utils::String::replace(evalxp, method.fullExpression, Utils::String::toUpper(method.arguments[0]));
		}

		// conditionnal expression
		if (evalxp.find('?') != std::string::npos)
		{
			auto xp = parseConditionalExpression(evalxp);
			if (xp != nullptr)
			{
				static Utils::MathExpr evaluator;

				Expression* selected = xp;

				std::stack<Expression*> stack;
				stack.push(xp);

				while (stack.size())
				{
					Expression* current = stack.top();
					stack.pop();

					if (current->truePart == nullptr || current->falsePart == nullptr)
						continue;

					bool isTruePart = false;
					if (current->expression == "1")
						isTruePart = true;
					else if (current->expression == "0")
						isTruePart = false;
					else
					{
						try
						{
							auto ret = evaluate(current->expression.c_str(), vars);
							isTruePart = ret.type == Utils::MathExpr::NUMBER && ret.number != 0;
						}
						catch (...) {}
					}

					if (isTruePart)
					{
						selected = current->truePart;
						stack.push(current->truePart);
					}
					else
					{
						selected = current->falsePart;
						stack.push(current->falsePart);
					}
				}

				evalxp = Utils::String::replace(evalxp, xp->fullExpression, selected->toString());
				delete xp;
			}
		}

		return evalxp;
	}

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

	static std::map<std::string, int> opPrecedence =
	{
		{ "(", -10 },
		{ "&&", -2 },
		{ "||", -3 },
		{ ">", -1 },
		{ ">=", -1 },
		{ "<", -1 },
		{ "<=", -1 },
		{ "==", -1 },
		{ "!=", -1 },
		{ "<<", 1 },
		{ ">>", 1 },
		{ "+", 2 },
		{ "-", 2 },
		{ "*", 3 },
		{ "/", 3 },
		{ "^", 4 },
		{ "!", 5 }
	};


	MathExpr::ValuePtrQueue MathExpr::toRPN(const char* expr, ValueMap* vars)
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
					while (operatorStack.size() && operatorStack.top().compare("("))
					{
						rpnQueue.push(new Value(operatorStack.top(), TOKEN));
						operatorStack.pop();
					}
					if (operatorStack.size())
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
					while (*expr && !isspace(*expr) && !isdigit(*expr) && !isvariablechar(*expr) && *expr != '(' && *expr != ')' && *expr != '\"' && *expr != '\'' && *expr != '$' && *expr != '{' && *expr != '}')
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

	MathExpr::Value MathExpr::evaluate(const char* expr, ValueMap* vars)
	{
		std::string evalxp = evaluateMethods(expr, vars);
		
		if (evalxp.empty())
			return MathExpr::Value();
		else if (evalxp == "0" || evalxp == "!1")
			return MathExpr::Value(0);
		else if (evalxp == "1" || evalxp == "!0")
			return MathExpr::Value(1);
		else if (evalxp == "\"\"")
			return MathExpr::Value("");

		// Convert to RPN with Dijkstra's Shunting-yard algorithm.
		ValuePtrQueue rpn = toRPN(evalxp.c_str(), vars);

		// Evaluate the expression in RPN form.
		ValueStack evaluation;

		while (!rpn.empty()) 
		{
			Value* tok = rpn.front();
			rpn.pop();

			if (tok->isToken())
			{
				std::string str = tok->string;

				if (evaluation.size() < 2)
					throw std::domain_error("Invalid equation.");
				
				Value right = evaluation.top(); evaluation.pop();
				Value left = evaluation.top(); evaluation.pop();

				if (!str.compare("+") && left.isNumber())
					evaluation.push(left.number + right.toNumber());
				else if (!str.compare("+") && left.isString())
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

		if (evaluation.size() != 1)
			throw std::domain_error("Invalid evaluation.");

		return evaluation.top();
	}
}