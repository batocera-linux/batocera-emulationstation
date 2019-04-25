#include "guis/GuiBackupStart.h"

#include "ApiSystem.h"
#include "components/OptionListComponent.h"
#include "guis/GuiBackup.h"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include "views/ViewController.h"

#include "LocaleES.h"

GuiBackupStart::GuiBackupStart(Window* window) : GuiComponent(window),
  mMenu(window, _("BACKUP USER DATA").c_str())
{
	addChild(&mMenu);

	// available backup storage
	std::vector<std::string> availableStorage = ApiSystem::getInstance()->getAvailableBackupDevices();
	moptionsStorage = std::make_shared<OptionListComponent<std::string> >(window, _("TARGET DEVICE"),
										  false);
	for(auto it = availableStorage.begin(); it != availableStorage.end(); it++){
	  if (boost::starts_with((*it), "DEV")){
	    std::vector<std::string> tokens;
	    boost::split( tokens, (*it), boost::is_any_of(" ") );
	    if(tokens.size()>= 3){
	      // concatenat the ending words
	      std::string vname = "";
	      for(unsigned int i=2; i<tokens.size(); i++) {
		if(i > 2) vname += " ";
		vname += tokens.at(i);
	      }
	      moptionsStorage->add(vname, tokens.at(1), false);
	    }
	  } else {
	    moptionsStorage->add((*it), (*it), false);
	  }
	}
	mMenu.addWithLabel(_("TARGET DEVICE"), moptionsStorage);

	mMenu.addButton(_("START"), "start", std::bind(&GuiBackupStart::start, this));
	mMenu.addButton(_("BACK"), "back", [&] { delete this; });

	mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, Renderer::getScreenHeight() * 0.1f);
}

void GuiBackupStart::start()
{
  if(moptionsStorage->getSelected() != "") {
    mWindow->pushGui(new GuiBackup(mWindow, moptionsStorage->getSelected()));
    delete this;
  }
}

bool GuiBackupStart::input(InputConfig* config, Input input)
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

std::vector<HelpPrompt> GuiBackupStart::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mMenu.getHelpPrompts();
	prompts.push_back(HelpPrompt("a", _("BACK")));
	prompts.push_back(HelpPrompt("start", _("CLOSE")));
	return prompts;
}
