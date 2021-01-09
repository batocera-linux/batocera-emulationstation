#pragma once
#ifndef ES_CORE_MATH_VECTOR2F_H
#define ES_CORE_MATH_VECTOR2F_H

#include "math/Misc.h"
#include <assert.h>
#include <string>

#include "hlslpp/hlsl++.h"
using namespace hlslpp;

class Vector3f;
class Vector4f;

class Vector2f
{
public:
	Vector2f() { vec.xy=0; }

	         Vector2f(const float _f)                 : vec(_f, _f)    { }
	         Vector2f(const float _x, const float _y) : vec(_x, _y)    { }
	explicit Vector2f(const Vector3f& _v)             : vec(((Vector2f&)_v).vec2()) { }
	explicit Vector2f(const Vector4f& _v)             : vec(((Vector2f&)_v).vec2()) { }

	// Explicit constructors for fast hlslpp
	explicit Vector2f(float2 _v)                                      : vec(_v) { }

	// Fast inline accessors to downcast vec4 to vec2/vec3
	inline float2& 		 vec2()       { return vec; }
	inline const float2& vec2() const { return vec; }

	const bool     operator==(const Vector2f& _other) const { return all(vec - _other.vec); }
	const bool     operator!=(const Vector2f& _other) const { return any(vec - _other.vec); }

	const Vector2f operator+ (const Vector2f& _other) const { return Vector2f(vec + _other.vec); }
	const Vector2f operator- (const Vector2f& _other) const { return Vector2f(vec - _other.vec); }
	const Vector2f operator* (const Vector2f& _other) const { return Vector2f(vec * _other.vec); }
	const Vector2f operator/ (const Vector2f& _other) const { return Vector2f(vec / _other.vec); }

	const Vector2f operator+ (const float& _other) const    { return Vector2f(vec + _other); }
	const Vector2f operator- (const float& _other) const    { return Vector2f(vec - _other); }
	const Vector2f operator* (const float& _other) const    { return Vector2f(vec * _other); }
	const Vector2f operator/ (const float& _other) const    { return Vector2f(vec / _other); }

	const Vector2f operator- () const                       { return Vector2f(-vec); }

	Vector2f&      operator+=(const Vector2f& _other)       { *this = *this + _other; return *this; }
	Vector2f&      operator-=(const Vector2f& _other)       { *this = *this - _other; return *this; }
	Vector2f&      operator*=(const Vector2f& _other)       { *this = *this * _other; return *this; }
	Vector2f&      operator/=(const Vector2f& _other)       { *this = *this / _other; return *this; }

	Vector2f&      operator+=(const float& _other)          { *this = *this + _other; return *this; }
	Vector2f&      operator-=(const float& _other)          { *this = *this - _other; return *this; }
	Vector2f&      operator*=(const float& _other)          { *this = *this * _other; return *this; }
	Vector2f&      operator/=(const float& _other)          { *this = *this / _other; return *this; }

	      float&   operator[](const int _index)             { assert(_index < 2 && "index out of range"); return (vec.f32[_index]); }
	const float&   operator[](const int _index) const       { assert(_index < 2 && "index out of range"); return (vec.f32[_index]); }

	inline       float& x()       { return vec.f32[0]; }
	inline       float& y()       { return vec.f32[1]; }
	inline const float& x() const { return vec.f32[0]; }
	inline const float& y() const { return vec.f32[1]; }

	Vector2f& round();
	Vector2f& lerp (const Vector2f& _start, const Vector2f& _end, const float _fraction);

	static const Vector2f Zero () { return Vector2f(0,0); }
	static const Vector2f UnitX() { return Vector2f(1,0); }
	static const Vector2f UnitY() { return Vector2f(0,1); }

	static const Vector2f parseString(const std::string& _input);
	const std::string toString();

	inline bool empty() { return vec.x == 0.0 && vec.y == 0.0; }
private:

	float2 vec;

}; // Vector2f

#endif // ES_CORE_MATH_VECTOR2F_H
