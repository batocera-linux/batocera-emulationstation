#include "math/Vector4f.h"
#include "utils/StringUtil.h"
#include "math/Vector2f.h"

Vector4f& Vector4f::round()
{
	vec = hlslpp::round(vec);

	return *this;

} // round

Vector4f& Vector4f::lerp(const Vector4f& _start, const Vector4f& _end, const float _fraction)
{
	vec = smoothstep(_start.vec, _end.vec, _fraction);

	return *this;

} // lerp

const Vector4f Vector4f::parseString(const std::string& _input)
{
	Vector4f ret = Vector4f(0, 0, 0, 0);

	auto splits = Utils::String::split(_input, ' ');

	if (splits.size() == 1)
	{
		ret = Vector4f(
			(float)atof(splits.at(0).c_str()), (float)atof(splits.at(0).c_str()),
			(float)atof(splits.at(0).c_str()), (float)atof(splits.at(0).c_str()));
	}
	else if (splits.size() == 2)
	{
		ret = Vector4f(
			(float)atof(splits.at(0).c_str()), (float)atof(splits.at(1).c_str()),
			(float)atof(splits.at(0).c_str()), (float)atof(splits.at(1).c_str()));
	}
	else if (splits.size() == 4)
	{
		ret = Vector4f(
			(float)atof(splits.at(0).c_str()), (float)atof(splits.at(1).c_str()),
			(float)atof(splits.at(2).c_str()), (float)atof(splits.at(3).c_str()));
	}

	return ret;
}

Vector4f& Vector4f::operator*=(const Vector2f& _other)
{ 
	vec = vec.xyzw * _other.vec2().xyxy;
	return *this; 
}
