#include "StoryboardAnimator.h"
#include "PowerSaver.h"

StoryboardAnimator::StoryboardAnimator(GuiComponent* comp, ThemeStoryboard* storyboard)
{
	mHasInitialProperties = false;
	mPaused = true;
	mComponent = comp;
	mStoryBoard = storyboard;
	mRepeatCount = 0;
	mCurrentTime = 0;
}

StoryboardAnimator::~StoryboardAnimator()
{
	pause();

	for (auto story : _currentStories)
		delete story;

	_currentStories.clear();
}

void StoryboardAnimator::clearStories()
{
	for (auto story : _finishedStories)
		delete story;

	for (auto story : _currentStories)
		delete story;

	_finishedStories.clear();
	_currentStories.clear();
}

void StoryboardAnimator::reset(int atTime)
{
	if (mPaused)
	{
		PowerSaver::pause();
		mPaused = false;
	}

	mCurrentTime = atTime;

	clearStories();

	if (atTime > 0)
	{
		for (auto anim : mStoryBoard->animations)
			if (anim->begin + anim->duration <= atTime)
				_finishedStories.push_back(new StoryAnimation(anim));

		addNewAnimations();
	}
	else
	{
		if (mHasInitialProperties)
		{
			for (auto prop : mInitialProperties)
				mComponent->setProperty(prop.first, prop.second);
		}

		for (auto anim : mStoryBoard->animations)
			if (anim->begin == 0)
				_currentStories.push_back(new StoryAnimation(anim));
	}
}

void StoryboardAnimator::stop()
{
	pause();

	for (auto prop : mInitialProperties)
		mComponent->setProperty(prop.first, prop.second);

	clearStories();
}

void StoryboardAnimator::pause()
{ 
	if (!mPaused)
	{
		PowerSaver::resume();
		mPaused = true;
	}
}

void StoryboardAnimator::addNewAnimations()
{
	for (auto anim : mStoryBoard->animations)
	{
		bool exists = false;

		for (auto story : _currentStories)
			if (story->animation == anim) { exists = true; break; }

		if (!exists)
		{
			for (auto story : _finishedStories)
				if (story->animation == anim) { exists = true; break; }
		}

		if (exists)
			continue;

		if (mCurrentTime >= anim->begin)
		{
			anim->ensureInitialValue(mComponent->getProperty(anim->propertyName));
			_currentStories.push_back(new StoryAnimation(anim));
		}
	}
}

bool StoryboardAnimator::update(int elapsed)
{
	if (mPaused || elapsed > 500)
		return true;

	if (!mHasInitialProperties)
	{
		mHasInitialProperties = true;

		for (auto anim : mStoryBoard->animations)
		{
			if (anim->begin == 0)
			{
				if (anim->to.type == ThemeData::ThemeElement::Property::Unknown)
					anim->to = mComponent->getProperty(anim->propertyName);

				if (anim->from.type == ThemeData::ThemeElement::Property::Unknown)
					anim->from = mComponent->getProperty(anim->propertyName);
				else
					mComponent->setProperty(anim->propertyName, anim->from);
			}
		}

		for (auto anim : mStoryBoard->animations)
			mInitialProperties[anim->propertyName] = mComponent->getProperty(anim->propertyName);
	}

	mCurrentTime += elapsed;

	addNewAnimations();

	for (int i = _currentStories.size() - 1; i >= 0; i--)
	{
		auto story = _currentStories[i];
		bool ended = !story->update(elapsed);

		mComponent->setProperty(story->animation->propertyName, story->currentValue);

		if (ended)
		{
			_finishedStories.push_back(story);

			auto it = _currentStories.begin();
			std::advance(it, i);
			_currentStories.erase(it);
			continue;
		}
	}

	if (_finishedStories.size() == mStoryBoard->animations.size())
	{
		if (mStoryBoard->repeat == 1)
		{
			clearStories();
			pause();
			return false;
		}
		else if (mStoryBoard->repeat > 0)
		{
			mRepeatCount++;
			if (mRepeatCount >= mStoryBoard->repeat)
			{
				clearStories();
				pause();
				return false;
			}
		}

		reset(mStoryBoard->repeatAt);
		return true;
	}

	return true;
}

const std::string StoryboardAnimator::getName()
{
	if (mStoryBoard != nullptr)
		return mStoryBoard->eventName;

	return "";
}