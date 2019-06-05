#include "guis/GuiThemeInstallStart.h"

#include "ApiSystem.h"
#include "components/OptionListComponent.h"
#include "guis/GuiThemeInstall.h"
#include "guis/GuiSettings.h"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include "views/ViewController.h"

#include "LocaleES.h"

GuiThemeInstallStart::GuiThemeInstallStart(Window* window)
	:GuiComponent(window), mMenu(window, _("SELECT THEME").c_str())
{
	addChild(&mMenu);
	ComponentListRow row;
	std::vector<std::string> availableThemes = ApiSystem::getInstance()->getBatoceraThemesList();

	for(auto it = availableThemes.begin(); it != availableThemes.end(); it++){
	    	//moptionsTheme->add((*it), (*it), false);
		auto itstring = std::make_shared<TextComponent>(mWindow,
				(*it).c_str(), Font::get(FONT_SIZE_SMALL), 0x777777FF);
		char *tmp=new char [(*it).length()+1];
		mSelectedTheme=new char [(*it).length()+1];
		std::strcpy (tmp, (*it).c_str());
		// Get theme name (from string '[A] Theme_name http://url_of_this_theme')
		char *thname = strtok (tmp, " \t");
		thname = strtok (NULL, " \t");
		// Names longer than this will crash GuiMsgBox downstream
		// "48" found by trials and errors. Ideally should be fixed
		// in es-core MsgBox -- FIXME
		if (strlen(thname) > 48)
			thname[47]='\0';
		row.makeAcceptInputHandler([this, thname] {
				strcpy (mSelectedTheme,thname);
				this->start();
				});
		row.addElement(itstring, true);
		mMenu.addRow(row);
		row.elements.clear();
	}
	mMenu.addButton(_("BACK"), "back", [&] { delete this; });
	mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, Renderer::getScreenHeight() * 0.15f);
}

void GuiThemeInstallStart::start()
{
  if(strcmp(mSelectedTheme,"")) {
    mWindow->pushGui(new GuiThemeInstall(mWindow, mSelectedTheme));
    delete this;
  }
}

bool GuiThemeInstallStart::input(InputConfig* config, Input input)
{
	bool consumed = GuiComponent::input(config, input);
	if(consumed)
		return true;
	
	if(input.value != 0 && config->isMappedTo("a", input))
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

std::vector<HelpPrompt> GuiThemeInstallStart::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mMenu.getHelpPrompts();
	prompts.push_back(HelpPrompt("a", _("BACK")));
	prompts.push_back(HelpPrompt("start", _("CLOSE")));
	return prompts;
}
