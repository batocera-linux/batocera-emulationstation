#pragma once

#include <mutex>
#include "GuiComponent.h"

class ComponentGrid;
class NinePatchComponent;
class TextComponent;
class Window;

class AsyncNotificationComponent : public GuiComponent
{
public:
	AsyncNotificationComponent(Window* window, bool actionLine = true);
	~AsyncNotificationComponent();

	void updateTitle(const std::string text);
	void updateText(const std::string text, const std::string action = "");
	void updatePercent(int percent);

	void render(const Transform4x4f& parentTrans) override;

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
};