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

	         Vector4f(const float _f)                                                 : vec(        _f, _f,  _f, _f) { }
	         Vector4f(const float _x, const float _y, const float _z, const float _w) : vec(        _x, _y,  _z, _w) { }
	explicit Vector4f(const Vector2f& _v)                                             : vec( ((Vector4f&)_v).vec2(),       0,  0) { }
	explicit Vector4f(const Vector2f& _v, const float _z)                             : vec( ((Vector4f&)_v).vec2(),      _z,  0) { }
	explicit Vector4f(const Vector2f& _v, const float _z, const float _w)             : vec( ((Vector4f&)_v).vec2(),      _z, _w) { }
	explicit Vector4f(const Vector3f& _v)                                             : vec( ((Vector4f&)_v).vec3(),           0) { }
	explicit Vector4f(const Vector3f& _v, const float _w)                             : vec( ((Vector4f&)_v).vec )                { }

	// Explicit constructors for fast hlslpp
	explicit Vector4f(float2 _v)                                      				  : vec(_v,  0,  0) { }
	explicit Vector4f(float2 _v, const float _z)                                      : vec(_v, _z,  0) { }
	explicit Vector4f(float2 _v, const float _z, const float _w)                      : vec(_v, _z, _w) { }
	explicit Vector4f(float3 _v, const float _w)                                      : vec(_v,     _w) { }
	explicit Vector4f(float4 _v)                                                      : vec(_v)         { }

	// Fast inline accessors to downcast vec4 to vec2/vec3
	inline float2 		 vec2()        { return float2(vec.xy);  }
	inline const float2  vec2() const  { return float2(vec.xy);  }
	inline float3 		 vec3()        { return float3(vec.xyz); }
	inline const float3  vec3() const  { return float3(vec.xyz); }
	inline float4 		 vec4()        { return vec; }
	inline const float4  vec4() const  { return vec; }

	const bool     operator==(const Vector4f& _other) const { return all(vec - _other.vec); }
	const bool     operator!=(const Vector4f& _other) const { return any(vec - _other.vec); }

	const Vector4f operator+ (const Vector4f& _other) const { return Vector4f(vec + _other.vec); }
	const Vector4f operator- (const Vector4f& _other) const { return Vector4f(vec - _other.vec); }
	const Vector4f operator* (const Vector4f& _other) const { return Vector4f(vec * _other.vec); }
	const Vector4f operator/ (const Vector4f& _other) const { return Vector4f(vec / _other.vec); }

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
