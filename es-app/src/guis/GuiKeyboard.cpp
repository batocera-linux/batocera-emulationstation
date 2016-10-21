#include "EmulationStation.h"
#include "guis/GuiKeyboard.h"
#include "Window.h"
#include "Sound.h"
#include "Log.h"
#include "Settings.h"
#include "Locale.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiSettings.h"
#include "guis/GuiScraperStart.h"
#include "guis/GuiDetectDevice.h"
#include "views/ViewController.h"

#include "components/ButtonComponent.h"
#include "components/SwitchComponent.h"
#include "components/SliderComponent.h"
#include "components/TextComponent.h"
#include "components/OptionListComponent.h"
#include "components/MenuComponent.h"
#include "components/ComponentGrid.h"
#include "VolumeControl.h"
#include "scrapers/GamesDBScraper.h"
#include "scrapers/MamedbScraper.h"

GuiKeyboard::GuiKeyboard(Window* window) : GuiComponent(window) , mMenu(window, "KEYBOARD"), mVersion(window), cGrid(window, Eigen::Vector2i(256,256))
{

	cGrid.setEntry(std::make_shared<ButtonComponent>(mWindow, "Q"), 
		Eigen::Vector2i(32, 32), true, true, Eigen::Vector2i(32, 32));
	cGrid.setEntry(std::make_shared<ButtonComponent>(mWindow, "W"),
		Eigen::Vector2i(64, 32), true, true, Eigen::Vector2i(32, 32));
	cGrid.setEntry(std::make_shared<ButtonComponent>(mWindow, "E"),
		Eigen::Vector2i(96, 32), true, true, Eigen::Vector2i(32, 32));
	cGrid.setEntry(std::make_shared<ButtonComponent>(mWindow, "R"),
		Eigen::Vector2i(128, 32), true, true, Eigen::Vector2i(32, 32));
	cGrid.setEntry(std::make_shared<ButtonComponent>(mWindow, "A"),
		Eigen::Vector2i(32, 64), true, true, Eigen::Vector2i(32, 32));

	addChild(&cGrid);


	mVersion.setFont(Font::get(FONT_SIZE_SMALL));
	mVersion.setColor(0xB6B6C4FF);
	mVersion.setText("KEYBOARD V.0.0.1");
	mVersion.setAlignment(ALIGN_CENTER);

	addChild(&mMenu);
	addChild(&mVersion);

	setSize(mMenu.getSize());
	setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, Renderer::getScreenHeight() * 0.15f);
}

void GuiKeyboard::onSizeChanged()
{
	mVersion.setSize(mSize.x(), 0);
	mVersion.setPosition(0, mSize.y() - mVersion.getSize().y());
}




bool GuiKeyboard::input(InputConfig* config, Input input)
{
	if(GuiComponent::input(config, input))
		return true;

	if((config->isMappedTo("b", input) || config->isMappedTo("start", input)) && input.value != 0)
	{
		delete this;
		return true;
	}

	return false;
}

std::vector<HelpPrompt> GuiKeyboard::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt("up/down", _("choose")));
	prompts.push_back(HelpPrompt("a", _("select")));
	prompts.push_back(HelpPrompt("start", _("close")));
	return prompts;
}
