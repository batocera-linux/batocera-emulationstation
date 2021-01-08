#include "math/Vector3f.h"

Vector3f& Vector3f::round()
{
	vec = hlslpp::round(vec);

	return *this;

} // round

Vector3f& Vector3f::lerp(const Vector3f& _start, const Vector3f& _end, const float _fraction)
{
	vec = smoothstep(_start.vec, _end.vec, _fraction);

	return *this;

} // lerp
