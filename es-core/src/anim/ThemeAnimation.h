#pragma once

#include "ThemeData.h"
#include "renderers/Renderer.h"
#include <string>

class ThemeAnimation
{
public:
	enum EasingMode
	{
		Linear,
		EaseIn,
		EaseInCubic,
		EaseInQuint,
		EaseOut,
		EaseOutCubic,
		EaseOutQuint,
		EaseInOut,
		Bump
	};

	ThemeAnimation()
	{
		repeat = 1;
		duration = 0;
		begin = 0;
		autoReverse = false;
		easingMode = EasingMode::Linear;

		from.type = ThemeData::ThemeElement::Property::PropertyType::Unknown;
		to.type = ThemeData::ThemeElement::Property::PropertyType::Unknown;
	}

	std::string propertyName;
	int duration;
	int begin;
	bool autoReverse;	
	int repeat; // 0 = forever
	EasingMode easingMode;

	ThemeData::ThemeElement::Property from;
	ThemeData::ThemeElement::Property to;
	
	virtual ThemeData::ThemeElement::Property computeValue(float value) = 0;

	void ensureInitialValue(const ThemeData::ThemeElement::Property& initialValue)
	{
		if (from.type == ThemeData::ThemeElement::Property::PropertyType::Unknown)
			from = initialValue;

		if (to.type == ThemeData::ThemeElement::Property::PropertyType::Unknown)
			to = initialValue;
	}	
};

class ThemeFloatAnimation : public ThemeAnimation
{
	ThemeData::ThemeElement::Property computeValue(float value) override
	{
		return from.f * (1.0f - value) + to.f * value;
	}	
};

class ThemeColorAnimation : public ThemeAnimation
{
	ThemeData::ThemeElement::Property computeValue(float value) override
	{
		return Renderer::mixColors(from.i, to.i, value);
	}
};

class ThemeVector2Animation : public ThemeAnimation
{
	ThemeData::ThemeElement::Property computeValue(float value) override
	{
		auto ret = Vector2f(
			from.v.x() * (1.0f - value) + to.v.x() * value,
			from.v.y() * (1.0f - value) + to.v.y() * value);		

		return ret;
	}
};

class ThemeVector4Animation : public ThemeAnimation
{
	ThemeData::ThemeElement::Property computeValue(float value) override
	{
		return Vector4f(
			from.r.x() * (1.0f - value) + to.r.x() * value,
			from.r.y() * (1.0f - value) + to.r.y() * value,
			from.r.z() * (1.0f - value) + to.r.z() * value,
			from.r.w() * (1.0f - value) + to.r.w() * value);
	}
};

class ThemeStringAnimation : public ThemeAnimation
{
	ThemeData::ThemeElement::Property computeValue(float value) override
	{
		if (value >= 0.9999)
			return to.s;

		return from.s;
	}
};

class ThemePathAnimation : public ThemeAnimation
{
	ThemeData::ThemeElement::Property computeValue(float value) override
	{
		if (value >= 0.9999)
			return to.s;

		return from.s;
	}
};