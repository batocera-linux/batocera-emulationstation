#pragma once
#ifndef ES_CORE_MATH_VECTOR3F_H
#define ES_CORE_MATH_VECTOR3F_H

#include "math/Misc.h"
#include <assert.h>
#include <math.h>

#include "hlslpp/hlsl++.h"
using namespace hlslpp;

class Vector2f;
class Vector4f;

class Vector3f
{
public:	   
	Vector3f()
	{ 
		vec = (0,0,0);
	}

	         Vector3f(const float _f)                                 : vec( _f, _f,  _f) { }
	         Vector3f(const float _x, const float _y, const float _z) : vec( _x, _y,  _z) { }
	explicit Vector3f(const Vector2f& _v)                             : vec(((Vector3f&)_v).vec2().xy,  0) { }
	explicit Vector3f(const Vector2f& _v, const float _z)             : vec(((Vector3f&)_v).vec2().xy, _z) { }
	explicit Vector3f(const Vector4f& _v)                             : vec(((Vector3f&)_v).vec3().xyz   ) { }

	// Explicit constructors for fast hlslpp
	explicit Vector3f(float3 _v)                             		  : vec(_v)     { }
	explicit Vector3f(float2 _v)                                      : vec(_v,  0) { }
	explicit Vector3f(float2 _v, const float _z)                      : vec(_v, _z) { }

	// Fast inline accessors to downcast vec4 to vec2/vec3
	inline float2 		 vec2()       { return float2(vec.xy);  }
	inline const float2  vec2() const { return float2(vec.xy);  }
	inline float3 		 vec3()       { return float3(vec.xyz); }
	inline const float3  vec3() const { return float3(vec.xyz); }

	const bool     operator==(const Vector3f& _other) const { return all(vec - _other.vec); }
	const bool     operator!=(const Vector3f& _other) const { return any(vec - _other.vec); }

	const Vector3f operator+ (const Vector3f& _other) const { return Vector3f(vec + _other.vec); }
	const Vector3f operator- (const Vector3f& _other) const { return Vector3f(vec - _other.vec); }
	const Vector3f operator* (const Vector3f& _other) const { return Vector3f(vec * _other.vec); }
	const Vector3f operator/ (const Vector3f& _other) const { return Vector3f(vec / _other.vec); }

	const Vector3f operator+ (const float& _other) const    { return Vector3f(vec + _other); }
	const Vector3f operator- (const float& _other) const    { return Vector3f(vec - _other); }
	const Vector3f operator* (const float& _other) const    { return Vector3f(vec * _other); }
	const Vector3f operator/ (const float& _other) const    { return Vector3f(vec / _other); }

	const Vector3f operator- () const                       { return Vector3f(-vec); }

	Vector3f&      operator+=(const Vector3f& _other)       { *this = *this + _other; return *this; }
	Vector3f&      operator-=(const Vector3f& _other)       { *this = *this - _other; return *this; }
	Vector3f&      operator*=(const Vector3f& _other)       { *this = *this * _other; return *this; }
	Vector3f&      operator/=(const Vector3f& _other)       { *this = *this / _other; return *this; }

	Vector3f&      operator+=(const float& _other)          { *this = *this + _other; return *this; }
	Vector3f&      operator-=(const float& _other)          { *this = *this - _other; return *this; }
	Vector3f&      operator*=(const float& _other)          { *this = *this * _other; return *this; }
	Vector3f&      operator/=(const float& _other)          { *this = *this / _other; return *this; }

	      float&   operator[](const int _index)             { assert(_index < 3 && "index out of range"); return (vec.f32[_index]); }
	const float&   operator[](const int _index) const       { assert(_index < 3 && "index out of range"); return (vec.f32[_index]); }

	inline       float& x()       { return vec.f32[0]; }
	inline       float& y()       { return vec.f32[1]; }
	inline       float& z()       { return vec.f32[2]; }
	inline const float& x() const { return vec.f32[0]; }
	inline const float& y() const { return vec.f32[1]; }
	inline const float& z() const { return vec.f32[2]; }

	inline       Vector2f& v2()       { return *(Vector2f*)this; }
	inline const Vector2f& v2() const { return *(Vector2f*)this; }

	Vector3f& round();
	Vector3f& lerp (const Vector3f& _start, const Vector3f& _end, const float _fraction);

	static const Vector3f Zero () { return Vector3f(0,0,0); }
	static const Vector3f UnitX() { return Vector3f(1,0,0); }
	static const Vector3f UnitY() { return Vector3f(0,1,0); }
	static const Vector3f UnitZ() { return Vector3f(0,0,1); }

	static float distanceSquared(const Vector3f& value1, const Vector3f& value2)
	{
		return abs(dot(value1.vec , value2.vec)).f32[0];
	}

	static float distance(const Vector3f& value1, const Vector3f& value2)
	{
		float1 l = length(float3(value1.vec - value2.vec));
		return abs(l).f32[0];
	}

	float _length()
	{
		return length(vec);
	}


private:

	float3 vec;

}; // Vector3f

#endif // ES_CORE_MATH_VECTOR3F_H
