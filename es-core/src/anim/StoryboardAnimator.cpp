#include "StoryboardAnimator.h"
#include "PowerSaver.h"

StoryboardAnimator::StoryboardAnimator(GuiComponent* comp, const ThemeStoryboard* storyboard)
{
	mHasInitialProperties = false;
	mPaused = true;
	mComponent = comp;

	mStoryBoard = storyboard->getDefinition();
	mAnimations.reserve(mStoryBoard->animations.size());
	for (std::size_t i = 0; i < mStoryBoard->animations.size(); ++i)
		if (storyboard->isAnimationEnabled(i))
			mAnimations.push_back(mStoryBoard->animations[i].get());
	mRepeatCount = 0;
	mCurrentTime = 0;
}

StoryboardAnimator::~StoryboardAnimator()
{
	pause();
	clearStories();
}

StoryboardAnimator::InitialValues& StoryboardAnimator::initialValues(const ThemeAnimation* animation)
{
	auto it = mInitialValues.find(animation);
	if (it == mInitialValues.end())
		it = mInitialValues.emplace(animation, InitialValues(animation)).first;
	return it->second;
}

const ThemeData::ThemeElement::Property& StoryboardAnimator::fromValue(const ThemeAnimation* animation)
{
	return animation->from.type == ThemeData::ThemeElement::Property::Unknown ?
		initialValues(animation).from : animation->from;
}

const ThemeData::ThemeElement::Property& StoryboardAnimator::toValue(const ThemeAnimation* animation)
{
	return animation->to.type == ThemeData::ThemeElement::Property::Unknown ?
		initialValues(animation).to : animation->to;
}

void StoryboardAnimator::ensureInitialValues(const ThemeAnimation* animation,
	const ThemeData::ThemeElement::Property& value)
{
	if (fromValue(animation).type == ThemeData::ThemeElement::Property::Unknown)
		initialValues(animation).from = value;
	if (toValue(animation).type == ThemeData::ThemeElement::Property::Unknown)
		initialValues(animation).to = value;
}

StoryAnimation* StoryboardAnimator::createStory(const ThemeAnimation* animation)
{
	return new StoryAnimation(animation, fromValue(animation), toValue(animation));
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

void StoryboardAnimator::reset(int atTime, bool resetInitialProperties)
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
		for (auto anim : mAnimations)
			if (anim->begin + anim->duration <= atTime)
				_finishedStories.push_back(createStory(anim));

		addNewAnimations();
	}
	else
	{
		mComponent->setProperty("sound", std::string()); // Make sure sound is always reset

		if (mHasInitialProperties && resetInitialProperties)
		{
			for (auto prop : mInitialProperties)
			{
				if (mDisabledProperties.find(prop.first) != mDisabledProperties.cend())
					continue;

				bool hasAssignationAtZero = false;

				if (atTime == 0)
				{
					for (auto anim : mAnimations)
						if (anim->begin == 0 && anim->propertyName == prop.first)
							hasAssignationAtZero = true;
				}

				if (!hasAssignationAtZero)
					mComponent->setProperty(prop.first, prop.second);
			}
		}

		for (auto anim : mAnimations)
			if (anim->begin == 0)
				_currentStories.push_back(createStory(anim));
	}
}

void StoryboardAnimator::clearInitialProperties()
{
	mInitialProperties.clear();
}

void StoryboardAnimator::stop()
{
	pause();

	for (auto prop : mInitialProperties)
		if (mDisabledProperties.find(prop.first) == mDisabledProperties.cend())
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
	for (auto anim : mAnimations)
	{
		bool exists = false;

		for (auto story : _currentStories)
			if (story->animation == anim) { exists = true; break; }

		if (!exists && _finishedStories.size())
		{
			for (auto story : _finishedStories)
				if (story->animation == anim) { exists = true; break; }
		}

		if (exists)
			continue;

		if (mCurrentTime >= anim->begin)
		{
			ensureInitialValues(anim, mComponent->getProperty(anim->propertyName));
			_currentStories.push_back(createStory(anim));
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

		for (auto anim : mAnimations)
			if (anim->propertyName != "sound") // Sound is always initially empty
				mInitialProperties[anim->propertyName] = mComponent->getProperty(anim->propertyName);

		for (auto anim : mAnimations)
		{
			if (anim->begin == 0)
			{
				if (toValue(anim).type == ThemeData::ThemeElement::Property::Unknown)
					initialValues(anim).to = anim->propertyName == "sound" ? std::string() : mComponent->getProperty(anim->propertyName);
				if (fromValue(anim).type == ThemeData::ThemeElement::Property::Unknown)
					initialValues(anim).from = anim->propertyName == "sound" ? std::string() : mComponent->getProperty(anim->propertyName);
				else if (mDisabledProperties.find(anim->propertyName) == mDisabledProperties.cend())
					mComponent->setProperty(anim->propertyName, fromValue(anim));
			}
		}
	}

	mCurrentTime += elapsed;

	addNewAnimations();

	for (int i = _currentStories.size() - 1; i >= 0; i--)
	{
		auto story = _currentStories[i];
		bool ended = !story->update(elapsed);

		if (mDisabledProperties.find(story->animation->propertyName) == mDisabledProperties.cend())
			mComponent->setProperty(story->animation->propertyName, story->currentValue);

		if (ended)
		{
			if (story->animation->propertyName == "sound")
				mComponent->setProperty("sound", std::string());

			_finishedStories.push_back(story);

			auto it = _currentStories.begin();
			std::advance(it, i);
			_currentStories.erase(it);
			continue;
		}
	}

	if (_finishedStories.size() == mAnimations.size())
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

		reset(mStoryBoard->repeatAt, mStoryBoard->repeatAt == 0); // it's a repeat
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

void StoryboardAnimator::enableProperty(const std::string& name, bool enable)
{
	mDisabledProperties.erase(name);

	if (!enable)
		mDisabledProperties.insert(name);
}
