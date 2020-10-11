#pragma once

#include "ThemeAnimation.h"
#include "ThemeStoryboard.h"
#include "GuiComponent.h"
#include <vector>


class StoryAnimation
{
public:
	StoryAnimation(ThemeAnimation* anim)
	{
		animation = anim;

		_repeatCount = 0;
		_currentTime = 0;
		_isReversed = false;		
	}

	bool update(int elapsed)
	{
		if (animation == nullptr)
			return false;

		if (_isReversed)
			_currentTime -= elapsed;
		else
			_currentTime += elapsed;

		bool ended = false;
		bool pseudoEnd = false;

		if (!_isReversed && _currentTime > animation->duration)
		{
			if (animation->autoReverse)
			{
				_isReversed = true;
				_currentTime = animation->duration;
			}
			else
			{
				pseudoEnd = true;
				_currentTime = 0;
			}

		}
		else if (_isReversed && _currentTime < 0)
		{
			_currentTime = 0;
			_isReversed = false;
			pseudoEnd = true;
		}

		if (pseudoEnd)
		{
			if (animation->repeat == 1)
				ended = true;
			else if (animation->repeat > 1)
			{
				_repeatCount++;
				if (_repeatCount >= animation->repeat)
					ended = true;
				else
					_currentTime = 0;
			}
		}

		if (ended || animation->duration == 0)
		{
			currentValue = animation->computeValue(animation->autoReverse ? 0.0f : 1.0f);
			return false;
		}

		float value = 0;

		if (_currentTime >= animation->duration)
			value = 1;
		else if (_currentTime < animation->duration)
		{
			float b = 0;
			float c = 1;
			float d = animation->duration;

			float t = _currentTime / (float)animation->duration;
			if (t > 1)
				t = 1;

			switch (animation->easingMode)
			{
			case ThemeAnimation::EasingMode::EaseIn:
				value = (t * t + b);
				break;

			case ThemeAnimation::EasingMode::EaseInCubic:
				value = t * t * t;
				break;

			case ThemeAnimation::EasingMode::EaseInQuint:
				value = t * t * t * t * t;
				break;

			case ThemeAnimation::EasingMode::EaseOut:
				value = (-c * t * (t - 2.0f));
				break;

			case ThemeAnimation::EasingMode::EaseOutCubic:
				t = t - 1.0f;
				value = t * t * t + 1.0f;
				break;

			case ThemeAnimation::EasingMode::EaseOutQuint:
				t = t - 1.0f;
				value = t * t * t * t * t + 1.0f;
				break;

			case ThemeAnimation::EasingMode::EaseInOut:
				t = _currentTime / ((float)animation->duration / 2.0f);
				if (t < 1)
					value = t * t / 2.0f;
				else
				{
					t--;
					value = (-c / 2.0f * (t * (t - 2.0f) - 1.0f));
				}
				break;

			case ThemeAnimation::EasingMode::Bump:
				#define PIVAL 3.141592653589793238462643383279502884L
				value = sin((PIVAL / 2.0) * t) + sin(PIVAL * t) / 2.0;
				break;

			default:
				value = t;
				break;
			}
		}

		currentValue = animation->computeValue(value);

		return !ended;
	}


	ThemeData::ThemeElement::Property currentValue;

	ThemeAnimation* animation;

private:
	int _repeatCount;
	int _currentTime;
	bool _isReversed;
};

class StoryboardAnimator
{
public:
	StoryboardAnimator(GuiComponent* comp, ThemeStoryboard* storyboard);
	~StoryboardAnimator();

	void reset(int atTime = 0);
	void stop();
	bool update(int elapsed);
	void pause();

	bool isRunning() { return !mPaused; }

	const std::string getName();

private:
	void addNewAnimations();
	void clearStories();

	GuiComponent* mComponent;
	ThemeStoryboard* mStoryBoard;

	int mRepeatCount;
	int mCurrentTime;

	bool mPaused;

	std::vector<StoryAnimation*> _currentStories;
	std::vector<StoryAnimation*> _finishedStories;
	std::map<std::string, ThemeData::ThemeElement::Property> mInitialProperties;
	bool mHasInitialProperties;
};
