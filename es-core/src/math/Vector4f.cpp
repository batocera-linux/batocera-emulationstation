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
	mX * _other.x(), mY * _other.y(), mZ * _other.x(), mW * _other.y(); 
	return *this; 
}