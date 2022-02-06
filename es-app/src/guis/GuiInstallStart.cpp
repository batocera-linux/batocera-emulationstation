#include "guis/GuiInstallStart.h"

#include "ApiSystem.h"
#include "components/OptionListComponent.h"
#include "guis/GuiInstall.h"
#include "views/ViewController.h"
#include "utils/StringUtil.h"
#include "LocaleES.h"
#include "guis/GuiMsgBox.h"
#include "components/SwitchComponent.h"

GuiInstallStart::GuiInstallStart(Window* window) : GuiComponent(window),
mMenu(window, _("INSTALL ON A NEW DISK").c_str())
{
	addChild(&mMenu);

	std::vector<std::string> availableStorage = ApiSystem::getInstance()->getAvailableInstallDevices();
	std::vector<std::string> availableArchitecture = ApiSystem::getInstance()->getAvailableInstallArchitectures();

	bool installationPossible = (availableArchitecture.size() != 0);

	// available install storage
	if (installationPossible) 
	{
		moptionsStorage = std::make_shared<OptionListComponent<std::string> >(window, _("TARGET DEVICE"), false);
		moptionsStorage->add(_("SELECT"), "", true);

		for (auto it = availableStorage.begin(); it != availableStorage.end(); it++) 
		{
			std::vector<std::string> tokens = Utils::String::split(*it, ' ');

			if (tokens.size() >= 2) {
				// concatenat the ending words
				std::string vname = "";
				for (unsigned int i = 1; i < tokens.size(); i++) {
					if (i > 1) vname += " ";
					vname += tokens.at(i);
				}
				moptionsStorage->add(vname, tokens.at(0), false);
			}
		}
		
		mMenu.addWithLabel(_("TARGET DEVICE"), moptionsStorage);
	
		// available install architecture
		moptionsArchitecture = std::make_shared<OptionListComponent<std::string> >(window, _("TARGET ARCHITECTURE"), false);
		moptionsArchitecture->add(_("SELECT"), "", true);

		for (auto it = availableArchitecture.begin(); it != availableArchitecture.end(); it++)
			moptionsArchitecture->add((*it), (*it), false);
		
		mMenu.addWithLabel(_("TARGET ARCHITECTURE"), moptionsArchitecture);

		moptionsValidation = std::make_shared<SwitchComponent>(mWindow);
		mMenu.addWithLabel(_("ARE YOU SURE?"), moptionsValidation);
		
		mMenu.addButton(_("INSTALL"), "install", std::bind(&GuiInstallStart::start, this));
		mMenu.addButton(_("BACK"), "back", [&] { delete this; });
	}
	else
		mMenu.addButton(_("NETWORK REQUIRED"), "back", [&] { delete this; });	

	if (Renderer::isSmallScreen())
		mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);
	else
		mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, Renderer::getScreenHeight() * 0.1f);
}

void GuiInstallStart::start()
{
	if (moptionsStorage->hasSelection() && moptionsStorage->getSelected() != "" &&
		moptionsArchitecture->hasSelection() && moptionsArchitecture->getSelected() != "" &&
		moptionsValidation->getState())
	{
		mWindow->pushGui(new GuiInstall(mWindow, moptionsStorage->getSelected(), moptionsArchitecture->getSelected()));
		delete this;
	}
	else
		mWindow->pushGui(new GuiMsgBox(mWindow, _("INVALID PARAMETERS")));
}

bool GuiInstallStart::input(InputConfig* config, Input input)
{
	bool consumed = GuiComponent::input(config, input);
	if(consumed)
		return true;
	
	if(input.value != 0 && config->isMappedTo(BUTTON_BACK, input))
	{
		delete this;
		return true;
	}

	if(config->isMappedTo("start", input) && input.value != 0)
	{
		// close everything
		Window* window = mWindow;
		while(window->peekGui() && window->peekGui() != ViewController::get())
			delete window->peekGui();
	}


	return false;
}

std::vector<HelpPrompt> GuiInstallStart::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mMenu.getHelpPrompts();
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	prompts.push_back(HelpPrompt("start", _("CLOSE")));
	return prompts;
}
