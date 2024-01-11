// Source: http://www.daniweb.com/software-development/cpp/code/427500/calculator-using-shunting-yard-algorithm#
// Source2: https://github.com/bamos/cpp-expression-parser
// Author: Jesse Brown
// Modifications: Brandon Amos
// Modifications: www.K3A.me (changed token class, float numbers, added support for boolean operators, added support for strings)

#ifndef _SHUNTING_YARD_H
#define _SHUNTING_YARD_H

#include <map>
#include <string>
#include <queue>
#include <stack>

namespace Utils
{

	class MathExpr
	{
	public:
		enum ValueType
		{
			UNDEFINED = 0,
			TOKEN = 1,
			NUMBER = 2,
			STRING = 4,

		};
		struct Value
		{
			Value() :type(UNDEFINED), number(0) {};
			Value(std::string str, ValueType t) :type(t), string(str), number(0) {};
			Value(std::string str) :type(STRING), string(str), number(0) {};
			Value(float n) :type(NUMBER), number(n) {};
			Value& operator=(std::string str) {
				type = STRING;
				string = str;
				return *this;
			}
			Value& operator=(float n) {
				type = NUMBER;
				number = n;
				return *this;
			}

			unsigned type;
			float number;
			std::string string;

			bool isToken()const { return type == TOKEN; };
			bool isString()const { return !isToken() && (type & STRING); };
			bool isNumber()const { return !isToken() && (type & NUMBER); };

			float toNumber();
			std::string toString();
		};

		typedef std::map<std::string, Value> ValueMap;
		typedef std::queue<Value*> ValuePtrQueue;
		typedef std::stack<Value> ValueStack;
		typedef std::map<std::string, int> IntMap;

	public:
		static MathExpr::Value evaluate(const char* expr, ValueMap* vars = 0);
		static void performUnitTests();

	private:
		MathExpr() { };

		static ValuePtrQueue toRPN(const char* expr, ValueMap* vars);
		static std::string	 evaluateMethods(const std::string& expr, ValueMap* vars);		
	};
}

#endif // _SHUNTING_YARD_H