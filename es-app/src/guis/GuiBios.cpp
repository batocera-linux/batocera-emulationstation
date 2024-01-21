#include "guis/GuiBios.h"

#include "ApiSystem.h"
#include "components/OptionListComponent.h"
#include "guis/GuiSettings.h"
#include "views/ViewController.h"
#include "SystemData.h"
#include "LocaleES.h"
#include "components/ComponentGrid.h"
#include "components/MultiLineMenuEntry.h"
#include "components/ComponentTab.h"
#include "components/ButtonComponent.h"
#include "GuiLoading.h"
#include "guis/GuiMsgBox.h"
#include "SystemConf.h"

#include <cstring>

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
	: GuiComponent(window), mGrid(window, Vector2i(1, 4)), mBackground(window, ":/frame.png"), mTabFilter(0)
{
	mBios = bioses;

	addChild(&mBackground);
	addChild(&mGrid);

	// Form background
	auto theme = ThemeData::getMenuTheme();
	mBackground.setImagePath(theme->Background.path);
	mBackground.setEdgeColor(theme->Background.color);
	mBackground.setCenterColor(theme->Background.centerColor);
	mBackground.setCornerSize(theme->Background.cornerSize);
	mBackground.setPostProcessShader(theme->Background.menuShader);

	// Title
	mTitle = std::make_shared<TextComponent>(mWindow, _("MISSING BIOS CHECK"), theme->Title.font, theme->Title.color, ALIGN_CENTER);
	mGrid.setEntry(mTitle, Vector2i(0, 0), false, true);

	// Tabs
	mTabs = std::make_shared<ComponentTab>(mWindow);
	mTabs->addTab(_("Installed systems"));
	mTabs->addTab(_("All"));	

	mTabs->setCursorChangedCallback([&](const CursorState& /*state*/)
		{			
			if (mTabFilter != mTabs->getCursorIndex())
			{
				mTabFilter = mTabs->getCursorIndex();
				loadList();
			}
		});

	mGrid.setEntry(mTabs, Vector2i(0, 1), false, true);

	// Entries
	mList = std::make_shared<ComponentList>(mWindow);
	mList->setUpdateType(ComponentListFlags::UpdateType::UPDATE_ALWAYS);

	mGrid.setEntry(mList, Vector2i(0, 2), true, true);

	// Buttons
	std::vector<std::shared_ptr<ButtonComponent>> buttons;
	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("REFRESH"), _("refresh"), [&] { refresh(); }));
	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("BACK"), _("BACK"), [this] { delete this; }));

	mButtonGrid = makeButtonGrid(mWindow, buttons);
	mGrid.setEntry(mButtonGrid, Vector2i(0, 3), true, false);

	mGrid.setUnhandledInputCallback([this](InputConfig* config, Input input) -> bool
		{
			if (config->isMappedLike("down", input)) { mGrid.setCursorTo(mList); mList->setCursorIndex(0); return true; }
			if (config->isMappedLike("up", input)) { mList->setCursorIndex(mList->size() - 1); mGrid.moveCursor(Vector2i(0, 1)); return true; }
			return false;
		});

	centerWindow();
	loadList();
}

void GuiBios::onSizeChanged()
{
	GuiComponent::onSizeChanged();

	mBackground.fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));

	mGrid.setSize(mSize);

	const float titleHeight = mTitle->getFont()->getLetterHeight();
	const float titleSubtitleSpacing = mSize.y() * 0.03f;

	mGrid.setRowHeight(0, titleHeight + titleSubtitleSpacing  + (Renderer::getScreenHeight() * 0.05f));

	if (mTabs->size() == 0)
		mGrid.setRowHeight(1, 0.00001f);
	else
		mGrid.setRowHeight(1, titleHeight * 2);

	mGrid.setRowHeight(3, mButtonGrid->getSize().y());	
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

	int idx = mList->getCursorIndex();
	mList->clear();

	if (mBios.size() == 0)
	{
		auto theme = ThemeData::getMenuTheme();
		std::shared_ptr<Font> font = theme->Text.font;
		unsigned int color = theme->Text.color;

		ComponentListRow row;
		auto text = std::make_shared<TextComponent>(mWindow, _("NO MISSING BIOS"), font, color);
		if (EsLocale::isRTL())
			text->setHorizontalAlignment(Alignment::ALIGN_RIGHT);

		row.addElement(text, true);

		mList->addRow(row);
		centerWindow();
	}

	for (auto systemBiosData : mBios)
	{
		ComponentListRow row;

		std::string name = Utils::String::proper(systemBiosData.name); // systemBiosData.name;

		bool isKnownSystem = false;

		for (auto sys : SystemData::sSystemVector)
		{
			if (sys->getName() == systemBiosData.name)
			{
				isKnownSystem = true;
				name = sys->getFullName();
			}
		}

		if (!isKnownSystem && mTabFilter == 0)
			continue;
		
		mList->addGroup(name, true);

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

			std::string status = _(biosFile.status.c_str()) + std::string(biosFile.md5.empty() || biosFile.md5 == "-" ? "" : " - MD5: " + biosFile.md5);

			auto line = std::make_shared<MultiLineMenuEntry>(mWindow, biosFile.path, status);
			row.addElement(line, true);

			mList->addRow(row);
		}
	}

	centerWindow();	
}

void GuiBios::centerWindow()
{
	if (Renderer::ScreenSettings::fullScreenMenus())
		setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
	else
		setSize(WINDOW_WIDTH, Renderer::getScreenHeight() * 0.875f);

	setPosition((Renderer::getScreenWidth() - getSize().x()) / 2, (Renderer::getScreenHeight() - getSize().y()) / 2);
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

	if (mTabs->input(config, input))
		return true;

	return false;
}

std::vector<HelpPrompt> GuiBios::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts; // = mMenu.getHelpPrompts();
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	prompts.push_back(HelpPrompt("start", _("REFRESH")));
	return prompts;
}
