#include "guis/GuiHashStart.h"

#include "components/OptionListComponent.h"
#include "components/SwitchComponent.h"
#include "guis/GuiMsgBox.h"
#include "views/ViewController.h"
#include "FileData.h"
#include "SystemData.h"
#include "scrapers/ThreadedScraper.h"
#include "LocaleES.h"
#include "GuiLoading.h"

GuiHashStart::GuiHashStart(Window* window, ThreadedHasher::HasherType type) : GuiComponent(window),
  mMenu(window, _("INDEX GAMES").c_str())
{
	mOverwriteMedias = true;
	mType = type;

	addChild(&mMenu);

	auto scraper = Scraper::getScraper();

	// add filters (with first one selected)
	mFilters = std::make_shared< OptionListComponent<bool> >(mWindow, _("GAMES TO INDEX"), false);
	mFilters->add(_("ALL"), true, false);
	mFilters->add(_("ONLY MISSING"), false, true);

	mMenu.addWithLabel(_("GAMES TO INDEX"), mFilters);
	
	std::unordered_set<std::string> checkedSystems;	

	if (ViewController::get()->getState().viewing == ViewController::GAME_LIST)
	{
		auto system = ViewController::get()->getState().getSystem();
		if (system != nullptr)
		{
			std::string systemName = system->getName();

			if (system->isGroupSystem())
			{
				for (auto child : system->getGroupChildSystemNames(systemName))
					checkedSystems.insert(child);
			}
			else
				checkedSystems.insert(systemName);
		}
	}

	mSystems = std::make_shared< OptionListComponent<SystemData*> >(mWindow, _("SYSTEMS INCLUDED"), true);
	for (auto sys : SystemData::sSystemVector)
	{
		if (!sys->isGameSystem())
			continue;

		bool takeNetplay = ((type & ThreadedHasher::HASH_NETPLAY_CRC) == ThreadedHasher::HASH_NETPLAY_CRC) && sys->isNetplaySupported();
		bool takeCheevos = ((type & ThreadedHasher::HASH_CHEEVOS_MD5) == ThreadedHasher::HASH_CHEEVOS_MD5) && sys->isCheevosSupported();
		if (!takeNetplay && !takeCheevos)
			continue;
		
		mSystems->add(sys->getFullName(), sys, checkedSystems.size() == 0 ? !sys->getPlatformIds().empty() : checkedSystems.find(sys->getName()) != checkedSystems.cend() && !sys->getPlatformIds().empty());
	}

	mMenu.addWithLabel(_("SYSTEMS INCLUDED"), mSystems);

	mMenu.addButton(_("START"), _("START"), std::bind(&GuiHashStart::start, this));
	mMenu.addButton(_("BACK"), _("BACK"), [&] { delete this; });

	if (Renderer::ScreenSettings::fullScreenMenus())
		mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);
	else
		mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, Renderer::getScreenHeight() * 0.15f);
}

void GuiHashStart::start()
{
	if (!ThreadedHasher::checkCloseIfRunning(mWindow))
		return;

	std::set<std::string> systems;
	for (auto sys : mSystems->getSelectedObjects())
		systems.insert(sys->getName());

	bool allGames = mFilters->getSelected();

	ThreadedHasher::start(mWindow, mType, allGames, false, &systems);
}

bool GuiHashStart::input(InputConfig* config, Input input)
{
	bool consumed = GuiComponent::input(config, input);
	if (consumed)
		return true;

	if (input.value != 0 && config->isMappedTo(BUTTON_BACK, input))
	{
		delete this;
		return true;
	}

	if (config->isMappedTo("start", input) && input.value != 0)
	{
		// close everything
		Window* window = mWindow;
		while (window->peekGui() && window->peekGui() != ViewController::get())
			delete window->peekGui();
	}


	return false;
}

std::vector<HelpPrompt> GuiHashStart::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mMenu.getHelpPrompts();
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	prompts.push_back(HelpPrompt("start", _("CLOSE")));
	return prompts;
}