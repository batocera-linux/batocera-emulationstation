#include "math/Vector4f.h"
#include "utils/StringUtil.h"
#include "math/Vector2f.h"

Vector4f& Vector4f::round()
{
	mX = (float)(int)(mX + 0.5f);
	mY = (float)(int)(mY + 0.5f);
	mZ = (float)(int)(mZ + 0.5f);
	mW = (float)(int)(mW + 0.5f);

	return *this;

} // round

Vector4f& Vector4f::lerp(const Vector4f& _start, const Vector4f& _end, const float _fraction)
{
	mX = Math::lerp(_start.x(), _end.x(), _fraction);
	mY = Math::lerp(_start.y(), _end.y(), _fraction);
	mZ = Math::lerp(_start.z(), _end.z(), _fraction);
	mW = Math::lerp(_start.w(), _end.w(), _fraction);

	return *this;

} // lerp

static float parseNextFloat(const char*& str)
{
	while (*str == ' ')
		str++;

	bool neg = false;
	if (*str == '-')
	{
		neg = true;
		++str;
	}
	else if (*str == '+')
		++str;

	int64_t value = 0;
	for (; *str && *str != '.' && *str != ' '; str++)
	{
		if (*str < '0' || *str > '9')
			return 0;

		value *= 10;
		value += *str - '0';
	}

	if (*str == '.')
	{
		str++;

		int64_t decimal = 0, weight = 1;

		for (; *str && *str != ' '; str++)
		{
			if (*str < '0' || *str > '9')
				return 0;

			decimal *= 10;
			decimal += *str - '0';
			weight *= 10;
		}

		float ret = value + (decimal / (float)weight);
		return neg ? -ret : ret;
	}

	return neg ? -value : value;
}

const Vector4f Vector4f::parseString(const std::string& _input)
{
	const char* str = _input.c_str();

	int count = 0;

	float arr[4];
	for (int i = 0; i < 4; i++) 
	{
		while (*str == ' ') str++;
		if (*str == '\0') 
			break;
		
		arr[i] = parseNextFloat(str);
		count++;
	}

	if (count == 4)
		return Vector4f(arr[0], arr[1], arr[2], arr[3]);
	
	if (count == 1)
		return Vector4f(arr[0], arr[0], arr[0], arr[0]);
	
	if (count == 2)
		return Vector4f(arr[0], arr[1], arr[0], arr[1]);

	return Vector4f(0, 0, 0, 0);
}

Vector4f& Vector4f::operator*=(const Vector2f& _other)
{ 
	mX * _other.x(), mY * _other.y(), mZ * _other.x(), mW * _other.y(); 
	return *this; 
}