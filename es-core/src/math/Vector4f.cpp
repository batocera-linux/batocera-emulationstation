#include "math/Vector4f.h"
#include "utils/StringUtil.h"
#include "math/Vector2f.h"

Vector4f& Vector4f::round()
{
	vec.x = (float)(int)(vec.x + 0.5f);
	vec.y = (float)(int)(vec.y + 0.5f);
	vec.z = (float)(int)(vec.z + 0.5f);
	vec.w = (float)(int)(vec.w + 0.5f);

	return *this;

} // round

Vector4f& Vector4f::lerp(const Vector4f& _start, const Vector4f& _end, const float _fraction)
{
	vec.x = Math::lerp(_start.x(), _end.x(), _fraction);
	vec.y = Math::lerp(_start.y(), _end.y(), _fraction);
	vec.z = Math::lerp(_start.z(), _end.z(), _fraction);
	vec.w = Math::lerp(_start.w(), _end.w(), _fraction);

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
	vec.x * _other.x(), vec.y * _other.y(), vec.z * _other.x(), vec.w * _other.y(); 
	return *this; 
}
