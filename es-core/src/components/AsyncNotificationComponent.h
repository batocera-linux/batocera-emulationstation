#pragma once

#include <mutex>
#include "GuiComponent.h"

class ComponentGrid;
class NinePatchComponent;
class TextComponent;
class Window;

class AsyncNotificationComponent : public GuiComponent
{
	friend class Window;

protected:
	AsyncNotificationComponent(Window* window, bool actionLine = true);
	~AsyncNotificationComponent();

public:
	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;

	void updateTitle(const std::string text);
	void updateText(const std::string text, const std::string action = "");
	void updatePercent(int percent);

	float getFading() { return mFadeOut; }
	bool isClosing() { return mClosing; };
	bool isRunning() { return mRunning; };
	Vector2f getFullSize() { return mFullSize; };

	void close();
	
private:
	std::shared_ptr<TextComponent> mTitle;
	std::shared_ptr<TextComponent> mGameName;
	std::shared_ptr<TextComponent> mAction;

	std::string mNextGameName;
	std::string mNextTitle;
	std::string mNextAction;

	ComponentGrid* mGrid;
	NinePatchComponent* mFrame;

	std::mutex					mMutex;

	int mPercent;

	bool                mRunning;
	bool				mClosing;
	float				mFadeOut;
	int                 mFadeTime;
	int                 mFadeOutOpacity;

	Vector2f			mFullSize;
};