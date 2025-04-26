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
#include <set>
#include <functional>
#include <algorithm>

#include "MathExpr.h"
#include "StringUtil.h"
#include "FileSystemUtil.h"
#include "TimeUtil.h"
#include "math/Misc.h"
#include "LocaleES.h"
#include "utils/HtmlColor.h"

namespace Utils
{
	static std::string addStringQuotes(const std::string& value) { return value.size() > 1 && value[0] == '"' && value[value.size() - 1] == '"' ? value : "\"" + value + "\""; }
	static std::string removeStringQuotes(const std::string& value) { return value.size() > 1 && value[0] == '"' && value[value.size() - 1] == '"' ? value.substr(1, value.size() - 2) : value; }

	typedef std::vector<std::string> Args;
	typedef std::tuple<std::string, int> MethodSignature; // Name, then argument count

	static std::string firstFile(const Args& args)
	{
		for (auto arg : args)
			if (Utils::FileSystem::exists(removeStringQuotes(arg)))
				return arg;

		return addStringQuotes("");
	}

	static std::map<MethodSignature, std::function<std::string(const Args&)>> _methods
	{
		// File / Path
		{ { "exists", 1 },        [](const Args& args) { return Utils::FileSystem::exists(removeStringQuotes(args[0])) ? "1" : "0"; } },
		{ { "isdirectory", 1 },   [](const Args& args) { return Utils::FileSystem::isDirectory(removeStringQuotes(args[0])) ? "1" : "0"; } },
		{ { "getextension", 1 },  [](const Args& args) { return addStringQuotes(Utils::String::toLower(Utils::FileSystem::getExtension(removeStringQuotes(args[0])))); } },
		{ { "filename", 1 },      [](const Args& args) { return addStringQuotes(Utils::FileSystem::getFileName(removeStringQuotes(args[0]))); } },
		{ { "stem", 1 },          [](const Args& args) { return addStringQuotes(Utils::FileSystem::getStem(removeStringQuotes(args[0]))); } },
		{ { "directory", 1 },     [](const Args& args) { return addStringQuotes(Utils::FileSystem::getParent(removeStringQuotes(args[0]))); } },
		{ { "filesize", 1 },	  [](const Args& args) { return std::to_string(Utils::FileSystem::getFileSize(removeStringQuotes(args[0]))); } },
		{ { "filesizekb", 1 },	  [](const Args& args) { return addStringQuotes(Utils::FileSystem::kiloBytesToString(Utils::FileSystem::getFileSize(removeStringQuotes(args[0])) / 1024)); } },
		{ { "filesizemb", 1 },	  [](const Args& args) { return addStringQuotes(Utils::FileSystem::megaBytesToString(Utils::FileSystem::getFileSize(removeStringQuotes(args[0])) / 1024 / 1024)); } },
		{ { "firstfile", -1 },	  [](const Args& args) { return firstFile(args); } },

		// String
		{ { "empty", 1 },         [](const Args& args) { return removeStringQuotes(args[0]).empty() ? "1" : "0"; } },
		{ { "lower", 1 },         [](const Args& args) { return Utils::String::toLower(args[0]); } },
		{ { "upper", 1 },         [](const Args& args) { return Utils::String::toUpper(args[0]); } },
		{ { "trim", 1 },          [](const Args& args) { return addStringQuotes(Utils::String::trim(removeStringQuotes(args[0]))); } },
		{ { "proper", 1 },        [](const Args& args) { return addStringQuotes(Utils::String::proper(removeStringQuotes(args[0]))); } },
		{ { "tointeger", 1 },     [](const Args& args) { return removeStringQuotes(args[0]); } },
		{ { "toboolean", 1 },     [](const Args& args) { return Utils::String::toBoolean(removeStringQuotes(args[0])) ? "1" : "0"; } },		
		{ { "translate", 1 },     [](const Args& args) { return addStringQuotes(_(removeStringQuotes(args[0]).c_str())); } },
		{ { "contains", 2 },      [](const Args& args) { return removeStringQuotes(args[0]).find(removeStringQuotes(args[1])) != std::string::npos ? "1" : "0"; } },
		{ { "startswith", 2 },    [](const Args& args) { return Utils::String::startsWith(removeStringQuotes(args[0]), removeStringQuotes(args[1])) ? "1" : "0"; } },
		{ { "endswith", 2 },      [](const Args& args) { return Utils::String::endsWith(removeStringQuotes(args[0]), removeStringQuotes(args[1])) ? "1" : "0"; } },

		// Math
		{ { "min", 2 },           [](const Args& args) { return std::to_string(Math::min(Utils::String::toFloat(removeStringQuotes(args[0])), Utils::String::toFloat(removeStringQuotes(args[1])))); } },
		{ { "max", 2 },           [](const Args& args) { return std::to_string(Math::max(Utils::String::toFloat(removeStringQuotes(args[0])), Utils::String::toFloat(removeStringQuotes(args[1])))); } },
		{ { "clamp", 3 },         [](const Args& args) { return std::to_string(Math::clamp( Utils::String::toFloat(removeStringQuotes(args[0])),  Utils::String::toFloat(removeStringQuotes(args[1])), Utils::String::toFloat(removeStringQuotes(args[2])) )); } },
		{ { "tostring", 1 },      [](const Args& args) { return addStringQuotes(args[0]); } },

		// Misc
		{ { "default", 1 },       [](const Args& args) { auto dataAsString = removeStringQuotes(args[0]);
														 return addStringQuotes(dataAsString.empty() ? _("Unknown") : dataAsString == "0" ? _("None") : dataAsString); } },
		{ { "year", 1 },          [](const Args& args) { auto time = Utils::Time::stringToTime(removeStringQuotes(args[0]), Utils::Time::getSystemDateFormat()); 
														 return addStringQuotes(time <= 0 ? "" : Utils::Time::timeToString(time, "%Y")); } },
		{ { "month", 1 },         [](const Args& args) { auto time = Utils::Time::stringToTime(removeStringQuotes(args[0]), Utils::Time::getSystemDateFormat()); 
														 return addStringQuotes(time <= 0 ? "" : Utils::Time::timeToString(time, "%m")); } },
		{ { "day", 1 },           [](const Args& args) { auto time = Utils::Time::stringToTime(removeStringQuotes(args[0]), Utils::Time::getSystemDateFormat()); 
														 return addStringQuotes(time <= 0 ? "" : Utils::Time::timeToString(time, "%d")); } },
		{ { "elapsed", 1 },       [](const Args& args) { auto time = Utils::Time::stringToTime(removeStringQuotes(args[0]), Utils::Time::getSystemDateFormat()); 
														 return addStringQuotes(time <= 0 ? "" : Utils::Time::getElapsedSinceString(time)); } },
		{ { "expandseconds", 1 }, [](const Args& args) { auto seconds = Utils::String::toInteger(removeStringQuotes(args[0])); 
														 return addStringQuotes(seconds == 0 ? "" : Utils::Time::secondsToString(seconds)); } },
		{ { "formatseconds", 1 }, [](const Args& args) { auto seconds = Utils::String::toInteger(removeStringQuotes(args[0]));
														 return addStringQuotes(seconds == 0 ? "" : Utils::Time::secondsToString(seconds, true)); } },
	};

	static std::map<std::string, int> opPrecedence =
	{
		{ "(", -10 }, { "&&", -2 }, { "||", -3 }, { ">", -1 }, { ">=", -1 }, { "<", -1 }, { "<=", -1 }, { "==", -1 },
		{ "!=", -1 }, { "<<", 1 }, { ">>", 1 }, { "+", 2 }, { "-", 2 }, { "*", 3 }, { "/", 3 }, { "^", 4 }, { "!", 5 }
	};

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
				delete falsePart;				
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

		bool inQuote = false;
		bool inChar = false;
		int parenth = 0;

		size_t conditionEnd = 0;
		while (true)
		{
			char c = input[conditionEnd];
			if (c == 0)
			{
				expression->expression = input;
				return expression;
			}

			if (c == '\"' && !inChar)
				inQuote = !inQuote;
			else if (c == '\'' && !inQuote)
				inChar = !inChar;
			else if (c == '?' && !inQuote && !inChar)
				break;

			conditionEnd++;
		}

		//size_t conditionEnd = input.find('?');
		if (conditionEnd != std::string::npos)
		{
			size_t start = conditionEnd > 0 ? conditionEnd - 1 : conditionEnd;

			inQuote = false;
			inChar = false;
			parenth = 0;

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

				start--;
				if (start >= 0 && input[start] == '(' && parenth == 0)
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

			int condCount = 0;

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
				else if (c == '?' && !inQuote && !inChar && parenth == 0)
					condCount++;
				else if (c == ':' && !inQuote && !inChar && parenth == 0)
				{
					if (condCount <= 0)
					{
						truePartEnd = idx;
						break;
					}

					condCount--;
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
			condCount = 0;

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
				else if (c == '?' && !inQuote && !inChar && parenth == 0)
					condCount++;
				else if (c == ':' && !inQuote && !inChar && parenth == 0)
				{
					if (condCount <= 0)
					{
						falsePartEnd = idx;
						break;
					}
					condCount--;
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

	static std::string reverseMethodToken(const std::string& input, int start)
	{
		static std::set<char> _operators = { ' ', '&', '|', '>', '<', '=', '!', '+', '-', '*', '^', '!', '/', '?', ':' };

		int src = start + 1;

		bool inQuote = false;
		bool inChar = false;
		int parenth = 0;
		int varcode = 0;

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
				parenth--;
			else if (c == ')' && !inQuote && !inChar)
				parenth++;
			else if (c == '{' && !inQuote && !inChar)
				varcode--;
			else if (c == '}' && !inQuote && !inChar)
				varcode++;
			else if (parenth == 0 && varcode == 0 && !inQuote && !inChar && _operators.find(c) != _operators.cend())
			{
				start++;
				return input.substr(start, src - start);				
			}

			start--;
			if (start >= 0 && input[start] == '(' && parenth == 0)
			{
				start++;
				return input.substr(start, src - start);
			}
		}

		return input.substr(0, src);
	}

	class Method
	{
	public:
		std::string name;
		Args arguments;

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
			else if (currentChar == '(' && !insideString && !insideChar && i != nameStart)
			{
				Method currentMethod;
				currentMethod.name = Utils::String::toLower(expression.substr(nameStart, i - nameStart));

				// Method is an 'extension' method
				if (nameStart > 0 && expression[nameStart - 1] == '.')
				{					
					auto token = reverseMethodToken(expression, nameStart - 2);
					if (!token.empty())
					{
						auto it = std::find_if(methods.begin(), methods.end(), [token](const Method& method) { return method.fullExpression == token; });
						if (it != methods.end())
						{
							currentMethod.arguments.push_back(token);							
							methods.erase(it);
							nameStart -= token.size() + 1;
						}
						else
						{
							currentMethod.arguments.push_back(token);
							nameStart -= token.size() + 1;							
						}
					}
				}

				bool foundComma = false;

				int parenth = 0;
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
						foundComma = true;
						auto arg = Utils::String::trim(expression.substr(argStart, a - argStart));
						currentMethod.arguments.push_back(arg);
						argStart = a + 1;
					}
					else if (currentChar == '(' && !insideString && !insideChar)
						parenth++;
					else if (currentChar == ')' && !insideString && !insideChar)
					{
						if (parenth > 0)
							parenth--;
						else
						{
							if (a != argStart || foundComma)
							{
								auto arg = Utils::String::trim(expression.substr(argStart, a - argStart));
								currentMethod.arguments.push_back(arg);
							}
						
							currentMethod.fullExpression = expression.substr(nameStart, a - nameStart + 1);							

							// Remove child methods							
							auto it = methods.begin();
							while (it != methods.end())
							{
								if (currentMethod.fullExpression.find(it->fullExpression) != std::string::npos)
									it = methods.erase(it);
								else
									it++;
							}

							i = a;
							break;
						}
					}
				}

				if (currentMethod.fullExpression.empty())
					break;

				methods.push_back(currentMethod);
			}
			else if (!isvariablechar(currentChar) || currentChar == '.')
				nameStart = i + 1;
		}

		std::reverse(methods.begin(), methods.end());
		return methods;
	}

	std::string MathExpr::evaluateMethods(const std::string& expr, ValueMap* vars)
	{
		std::string evalxp = expr;

		// Evaluate functions
		for (auto method : extractMethods(evalxp))
		{			
			// Evaluate arguments
			for (int arg = 0; arg < method.arguments.size(); arg++)
			{				
				const MathExpr::Value& val = evaluate(method.arguments[arg].c_str(), vars);
				if (val.type == STRING)
					method.arguments[arg] = addStringQuotes(val.string);
				else if (val.type == NUMBER)
					method.arguments[arg] = val.number == (int)val.number ? std::to_string((int) val.number) : std::to_string(val.number);
			}

			MethodSignature key(method.name, method.arguments.size());
			auto it = _methods.find(key);
			if (it == _methods.cend())
			{
				key = MethodSignature(method.name, -1);
				it = _methods.find(key);
			}

			if (it != _methods.cend())
			{
				auto result = it->second(method.arguments);
				evalxp = Utils::String::replace(evalxp, method.fullExpression, result);
			}
			else 
				throw std::domain_error("Unknown method name" + method.name + "()");			
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

					selected = isTruePart ? current->truePart : current->falsePart;
					stack.push(selected);
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

		if (number == (int)number)
			string = std::to_string((int)number);
		else
		{
			char str[16];
			sprintf(str, "%f", number);
			string = str;
		}

		type |= STRING;

		return string;
	}

	int iscolor(char const* _String, char** _EndPtr) 
	{
		static std::set<char> xplimiter = { '\0', ' ', (char) 160, '&', '|', '>', '<', '=', '!', '+', '-', '*', '^', '!', '/', '?', ':', '{', '}', '(', ')', '$', '.', '\'', '\"' };

		int endDelimiter = 0;
		while (true)
		{			
			if (xplimiter.find(_String[endDelimiter]) != xplimiter.cend())
				break;

			endDelimiter++;
		}

		if (endDelimiter == 0)
			return false;

		*_EndPtr = (char*) (_String + endDelimiter);
		const char* colorEndPos = strstr(_String, *_EndPtr);
		if (colorEndPos == nullptr)
			return false;

		return Utils::HtmlColor::isHtmlColor(colorEndPos);		
	}

	MathExpr::ValuePtrQueue MathExpr::toRPN(const char* expr, ValueMap* vars)
	{
		ValuePtrQueue rpnQueue; std::stack<std::string> operatorStack;
		bool lastTokenWasOp = true;

		// In one pass, ignore whitespace and parse the expression into RPN
		// using Dijkstra's Shunting-yard algorithm.
		while (*expr && isspace(*expr)) ++expr;

		while (*expr)
		{
			char* colorEnd;
			if (iscolor(expr, &colorEnd))
			{
				const char* colorEndPos = strstr(expr, colorEnd);
				int color = (int) Utils::HtmlColor::parse(colorEndPos);
				rpnQueue.push(new Value(color));
				expr = colorEnd;
				lastTokenWasOp = false;
			}
			else if (isdigit(*expr))
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
					while (isvariablechar(*expr) || (*expr) == '.')
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

	static void assert_throw(bool test) { if (!test) throw std::domain_error("assert"); }

	void MathExpr::performUnitTests()
	{
		Utils::MathExpr::ValueMap map;
		map["game:gametime"] = 10;
		map["game.duration"] = 5;

		// Variables without { } blocks 
		auto test = Utils::MathExpr::evaluate("{game:gametime} == 0 ? \"never\".upper() : ({game:gametime}.tostring() + 5.tostring() + \" sec\").upper()", &map);;

		auto val = Utils::MathExpr::evaluate("game.duration.tostring()", &map);
		assert_throw(val.type == 4 && val.string == "5");

		val = Utils::MathExpr::evaluate("\"toto\".startswith(\"to\")", &map);
		assert_throw(val.type == 2 && val.number == 1);

		val = Utils::MathExpr::evaluate("\"c:/testpath/toto.zip\".filename()", &map);
		assert_throw(val.type == 4 && val.string == "toto.zip");

		val = Utils::MathExpr::evaluate("\"c:/testpath/toto.zip\".directory().filename()", &map);
		assert_throw(val.type == 4 && val.string == "testpath");

		val = Utils::MathExpr::evaluate("tostring(game.duration).upper().lower().tointeger().tostring()", &map);
		assert_throw(val.type == 4 && val.string == "5");

		val = Utils::MathExpr::evaluate("upper(lower(tostring(game.duration + 5))).tointeger().tostring()", &map);
		assert_throw(val.type == 4 && val.string == "10");

		val = Utils::MathExpr::evaluate("tostring(0.5)", &map);
		assert_throw(val.type == 4 && val.string == "0.500000");

		val = Utils::MathExpr::evaluate("{game:gametime} == 0 ? \"never\".upper() : ({game:gametime}.tostring() + \" sec\").upper()", &map);
		assert_throw(val.type == 4 && val.string == "10 SEC");

		// Variables with $
		val = Utils::MathExpr::evaluate("${game:gametime} == 0 ? \"never\" : (${game:gametime}.tostring() + \" sec\")", &map);
		assert_throw(val.type == 4 && val.string == "10 sec");

		// Cascaded methods
		val = Utils::MathExpr::evaluate("upper({game:gametime} == 0 ? \"never\" : ({game:gametime}.tostring() + \" sec\"))", &map);
		assert_throw(val.type == 4 && val.string == "10 SEC");

		// Cascaded condition
		val = Utils::MathExpr::evaluate("(5 + (6 < 10 ? 3 + 2 == 5 ? 1 : 0 : 10)).tostring()", &map);
		assert_throw(val.type == 4 && val.string == "6");

		val = Utils::MathExpr::evaluate("lower(tostring({game:gametime}))", &map);
		assert_throw(val.type == 4 && val.string == "10");

		val = Utils::MathExpr::evaluate("!empty(\"Alien Syndrome\") ? upper(\"test\") : \"\"");
		assert_throw(val.type == 4 && val.string == "TEST");
	}
}