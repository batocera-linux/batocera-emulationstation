#pragma once

#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "ApiSystem.h"

template<typename T>
class OptionListComponent;

class GuiFileBrowser : public GuiComponent
{
public:
	enum FileTypes
	{
		IMAGES = 1,
		MANUALS = 2,
		VIDEO = 3,
		DIRECTORY = 4,

		ALL = 255
	};

	GuiFileBrowser(Window* window, const std::string startPath, const std::string selectedFile, FileTypes types = FileTypes::IMAGES, const std::function<void(const std::string&)>& okCallback = nullptr, const std::string& title = "");

	bool input(InputConfig* config, Input input) override;
	virtual std::vector<HelpPrompt> getHelpPrompts() override;

private:
	void navigateTo(const std::string path);
	void centerWindow();

	MenuComponent mMenu;	

	std::string mCurrentPath;
	std::string mSelectedFile;
	FileTypes   mTypes;

	std::function<void(const std::string&)> mOkCallback;
};
