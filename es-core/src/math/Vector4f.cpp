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
	auto splits = Utils::String::split(_input, ' ');
	if (splits.size() == 4)
		return Vector4f(Utils::String::toFloat(splits[0]), Utils::String::toFloat(splits[1]), Utils::String::toFloat(splits[2]), Utils::String::toFloat(splits[3]));

	Vector4f ret = Vector4f(0, 0, 0, 0);

	if (splits.size() == 1)
		ret.z() = ret.x() = ret.w() = ret.y() = Utils::String::toFloat(splits[0]);
	else if (splits.size() == 2)
	{
		ret.z() = ret.x() = Utils::String::toFloat(splits[0]);
		ret.w() = ret.y() = Utils::String::toFloat(splits[1]);
	}

	return ret;
}

Vector4f& Vector4f::operator*=(const Vector2f& _other)
{ 
	mX * _other.x(), mY * _other.y(), mZ * _other.x(), mW * _other.y(); 
	return *this; 
}