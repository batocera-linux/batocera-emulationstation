#pragma once
#ifndef ES_CORE_MATH_VECTOR4F_H
#define ES_CORE_MATH_VECTOR4F_H

#include "math/Misc.h"
#include <assert.h>
#include <string>

#include "hlslpp/hlsl++.h"
using namespace hlslpp;

class Vector2f;
class Vector3f;

class Vector4f
{
public:
	Vector4f()
	{
		vec = (0,0,0,0);
	}

	         Vector4f(const float _f)                                                 : vec(    _f,     _f,     _f, _f) { }
	         Vector4f(const float _x, const float _y, const float _z, const float _w) : vec(    _x,     _y,     _z, _w) { }
	explicit Vector4f(const Vector2f& _v)                                             : vec( ((Vector4f&)_v).x(), ((Vector4f&)_v).y(),                   0,  0) { }
	explicit Vector4f(const Vector2f& _v, const float _z)                             : vec( ((Vector4f&)_v).x(), ((Vector4f&)_v).y(),                  _z,  0) { }
	explicit Vector4f(const Vector2f& _v, const float _z, const float _w)             : vec( ((Vector4f&)_v).x(), ((Vector4f&)_v).y(),                  _z, _w) { }
	explicit Vector4f(const Vector3f& _v)                                             : vec( ((Vector4f&)_v).x(), ((Vector4f&)_v).y(), ((Vector4f&)_v).z(),  0) { }
	explicit Vector4f(const Vector3f& _v, const float _w)                             : vec( ((Vector4f&)_v).x(), ((Vector4f&)_v).y(), ((Vector4f&)_v).z(), _w) { }
	explicit Vector4f(float4 _v)                                                      : vec(_v) { }

	const bool     operator==(const Vector4f& _other) const { return all(vec - _other.vec); }
	const bool     operator!=(const Vector4f& _other) const { return any(vec - _other.vec); }

	const Vector4f operator+ (const Vector4f& _other) const { return Vector4f(vec + float4(_other.x(), _other.y(), _other.z(), _other.w())); }
	const Vector4f operator- (const Vector4f& _other) const { return Vector4f(vec - float4(_other.x(), _other.y(), _other.z(), _other.w())); }
	const Vector4f operator* (const Vector4f& _other) const { return Vector4f(vec * float4(_other.x(), _other.y(), _other.z(), _other.w())); }
	const Vector4f operator/ (const Vector4f& _other) const { return Vector4f(vec / float4(_other.x(), _other.y(), _other.z(), _other.w())); }

	const Vector4f operator+ (const float& _other) const    { return Vector4f(vec + _other); }
	const Vector4f operator- (const float& _other) const    { return Vector4f(vec - _other); }
	const Vector4f operator* (const float& _other) const    { return Vector4f(vec * _other); }
	const Vector4f operator/ (const float& _other) const    { return Vector4f(vec / _other); }

	const Vector4f operator- () const                       { return Vector4f(-vec); }

	Vector4f&      operator+=(const Vector4f& _other)       { *this = *this + _other; return *this; }
	Vector4f&      operator-=(const Vector4f& _other)       { *this = *this - _other; return *this; }
	Vector4f&      operator*=(const Vector4f& _other)       { *this = *this * _other; return *this; }
	Vector4f&      operator/=(const Vector4f& _other)       { *this = *this / _other; return *this; }

	Vector4f&      operator+=(const float& _other)          { *this = *this + _other; return *this; }
	Vector4f&      operator-=(const float& _other)          { *this = *this - _other; return *this; }
	Vector4f&      operator*=(const float& _other)          { *this = *this * _other; return *this; }
	Vector4f&      operator/=(const float& _other)          { *this = *this / _other; return *this; }

	Vector4f&      operator*=(const Vector2f& _other);

	      float&   operator[](const int _index)             { assert(_index < 4 && "index out of range"); return (vec.f32[_index]); }
	const float&   operator[](const int _index) const       { assert(_index < 4 && "index out of range"); return (vec.f32[_index]); }

	inline       float& x()       { return vec.f32[0]; }
	inline       float& y()       { return vec.f32[1]; }
	inline       float& z()       { return vec.f32[2]; }
	inline       float& w()       { return vec.f32[3]; }
	inline const float& x() const { return vec.f32[0]; }
	inline const float& y() const { return vec.f32[1]; }
	inline const float& z() const { return vec.f32[2]; }
	inline const float& w() const { return vec.f32[3]; }

	inline       Vector2f& v2()       { return *(Vector2f*)this; }
	inline const Vector2f& v2() const { return *(Vector2f*)this; }

	inline       Vector3f& v3()       { return *(Vector3f*)this; }
	inline const Vector3f& v3() const { return *(Vector3f*)this; }

	Vector4f& round();
	Vector4f& lerp (const Vector4f& _start, const Vector4f& _end, const float _fraction);

	static const Vector4f Zero () { return Vector4f(0,0,0,0); }
	static const Vector4f UnitX() { return Vector4f(1,0,0,0); }
	static const Vector4f UnitY() { return Vector4f(0,1,0,0); }
	static const Vector4f UnitZ() { return Vector4f(0,0,1,0); }
	static const Vector4f UnitW() { return Vector4f(0,0,0,1); }

	static const Vector4f parseString(const std::string& _input);

private:

	float4 vec;

}; // Vector4f

#endif // ES_CORE_MATH_VECTOR4F_H
