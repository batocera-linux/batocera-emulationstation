#pragma once

#include "GuiComponent.h"
#include "components/ImageComponent.h"
#include <set>

class GuiKeyboardLayout : public GuiComponent
{
public:
	GuiKeyboardLayout(Window* window, const std::function<void(const std::set<std::string>&)>& okCallback = nullptr, std::set<std::string>* activeKeys = nullptr);
	~GuiKeyboardLayout();

	static bool isEnabled();

	bool input(InputConfig* config, Input input) override;
	std::vector<HelpPrompt> getHelpPrompts() override;

private:
	void selectKey(const std::string& keyName);

	bool		   mImageToggle;
	std::string    mKeyboardSvg;
	ImageComponent mKeyboard;

	std::set<std::string>    mActiveKeys;	

	std::set<std::string>    mSelectedKeys;

	int			   mX;
	int			   mY;

	std::function<void(const std::set<std::string>&)> mOkCallback;
};
