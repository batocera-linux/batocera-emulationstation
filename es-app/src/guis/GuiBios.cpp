#include "guis/GuiBios.h"

#include "ApiSystem.h"
#include "components/OptionListComponent.h"
#include "guis/GuiSettings.h"
#include "views/ViewController.h"
#include "components/ComponentGrid.h"
#include "SystemData.h"
#include "LocaleES.h"
#include "components/MultiLineMenuEntry.h"
#include "GuiLoading.h"
#include "guis/GuiMsgBox.h"
#include <cstring>
#include "SystemConf.h"

#define WINDOW_WIDTH (float)Math::max((int)Renderer::getScreenHeight(), (int)(Renderer::getScreenWidth() * 0.65f))

void GuiBios::show(Window* window)
{
	window->pushGui(new GuiLoading<std::vector<BiosSystem>>(window, _("PLEASE WAIT"),
		[](auto gui) { return ApiSystem::getInstance()->getBiosInformations(); },
		[window](std::vector<BiosSystem> ra) 
	{ 
		if (ra.size() == 0)
			window->pushGui(new GuiMsgBox(window, _("NO MISSING BIOS FILES"), _("OK")));
		else
			window->pushGui(new GuiBios(window, ra)); 
	}));
}

GuiBios::GuiBios(Window* window, const std::vector<BiosSystem> bioses)
	: GuiComponent(window), mMenu(window, _("MISSING BIOS CHECK").c_str())
{
	mBios = bioses;

	addChild(&mMenu);

	mMenu.addButton(_("REFRESH"), "refresh", [&] { refresh(); });
    mMenu.addButton(_("BACK"), "back", [&] { delete this; });

	loadList();
}

void GuiBios::refresh()
{
	mWindow->pushGui(new GuiLoading<std::vector<BiosSystem>>(mWindow, _("PLEASE WAIT"),
		[](auto gui) { return ApiSystem::getInstance()->getBiosInformations(); },
		[this](std::vector<BiosSystem> ra) { mBios = ra; loadList(); }));
}

void GuiBios::loadList()
{
#define INVALID_ICON _U("\uF071")
#define MISSING_ICON _U("\uF127")
	
	auto theme = ThemeData::getMenuTheme();

	mMenu.clear();

	if (mBios.size() == 0)
	{
		mMenu.addEntry(_("NO MISSING BIOS"));
		centerWindow();
	}

	for (auto systemBiosData : mBios)
	{
		ComponentListRow row;

		std::string name = systemBiosData.name;

		for (auto sys : SystemData::sSystemVector)
			if (sys->getName() == systemBiosData.name)
				name = sys->getFullName();

		mMenu.addGroup(name, true);

		for (auto biosFile : systemBiosData.bios)
		{
			auto theme = ThemeData::getMenuTheme();

			ComponentListRow row;

			auto icon = std::make_shared<TextComponent>(mWindow);
			icon->setText(biosFile.status == "INVALID" ? INVALID_ICON : MISSING_ICON);
			icon->setColor(theme->Text.color);
			icon->setFont(theme->Text.font);
			icon->setSize(theme->Text.font->getLetterHeight() * 1.5f, 0);
			row.addElement(icon, false);

			auto spacer = std::make_shared<GuiComponent>(mWindow);
			spacer->setSize(14, 0);
			row.addElement(spacer, false);

			std::string status = _(biosFile.status.c_str()) + " - MD5: " + biosFile.md5;

			auto line = std::make_shared<MultiLineMenuEntry>(mWindow, biosFile.path, status);
			row.addElement(line, true);

			mMenu.addRow(row);
		}
	}

	centerWindow();	
}

void GuiBios::centerWindow()
{
	if (Renderer::isSmallScreen())
		mMenu.setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
	else
	{
		mMenu.setSize(mMenu.getSize().x(), Renderer::getScreenHeight() * 0.875f);
		mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);
	}
}

bool GuiBios::input(InputConfig* config, Input input)
{
	if (GuiComponent::input(config, input))
		return true;

	if (input.value != 0 && config->isMappedTo(BUTTON_BACK, input))
	{
		delete this;
		return true;
	}

	if (config->isMappedTo("start", input) && input.value != 0)
	{
		refresh();
		return true;
	}

	return false;
}

std::vector<HelpPrompt> GuiBios::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mMenu.getHelpPrompts();
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	prompts.push_back(HelpPrompt("start", _("REFRESH")));
	return prompts;
}
