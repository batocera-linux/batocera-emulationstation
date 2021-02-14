#include "GuiSaveState.h"
#include "SystemData.h"
#include "FileData.h"
#include "utils/StringUtil.h"
#include "ApiSystem.h"
#include "HelpStyle.h"
#include "SystemConf.h"
#include "guis/GuiMsgBox.h"
#include "SaveStateRepository.h"

#define WINDOW_HEIGHT Renderer::getScreenHeight() * 0.40f

static int slots = 6; // 5;

GuiSaveState::GuiSaveState(Window* window, FileData* game, const std::function<void(const SaveState& state)>& callback) :
	GuiComponent(window), mGrid(nullptr), mBackground(window, ":/frame.png"),
	mLayout(window, Vector2i(3, 5)), mTitle(nullptr)
{
	mGame = game;
	mRepository = game->getSourceFileData()->getSystem()->getSaveStateRepository();
	mRunCallback = callback;

	// Form background
	auto theme = ThemeData::getMenuTheme();
	mBackground.setImagePath(theme->Background.path); // ":/frame.png"
	mBackground.setEdgeColor(theme->Background.color);
	mBackground.setCenterColor(theme->Background.centerColor);
	mBackground.setCornerSize(theme->Background.cornerSize);

	mTitle = std::make_shared<TextComponent>(mWindow, _("SAVE SNAPSHOTS"), theme->Title.font, theme->Title.color, ALIGN_CENTER);
	mLayout.setEntry(mTitle, Vector2i(1, 1), false, true);

	mGrid = std::make_shared<ImageGridComponent<SaveState>>(mWindow);
	mLayout.setEntry(mGrid, Vector2i(1, 3), true, true);

	addChild(&mBackground);
	addChild(&mLayout);
	
	float cellProportion = 1.77;
	float screenProportion = (float)Renderer::getScreenWidth() / (float)Renderer::getScreenHeight();

	float sh = (float)Math::min(Renderer::getScreenHeight(), Renderer::getScreenWidth());
	sh = (float) theme->TextSmall.font->getSize() / sh;

	std::string xml =
		"<theme defaultView=\"Tiles\">"
		"<formatVersion>7</formatVersion>"
		"<view name = \"grid\">"
		"<imagegrid name=\"gamegrid\">"
		"  <margin>0.01 0.02</margin>"
		"  <padding>0 0</padding>"
		"  <scrollDirection>horizontal</scrollDirection>"
		"  <autoLayout>" + std::to_string(slots * screenProportion / cellProportion) +" 1</autoLayout>"
		"  <autoLayoutSelectedZoom>1</autoLayoutSelectedZoom>"
		"  <animateSelection>false</animateSelection>"
		"  <centerSelection>false</centerSelection>"		
		"</imagegrid>"		
		"<gridtile name=\"default\">"
		"  <backgroundColor>FFFFFF00</backgroundColor>"
		"  <padding>8 8</padding>"
		"  <imageColor>FFFFFFFF</imageColor>"
		"</gridtile>"		
		"<gridtile name=\"selected\">"
		"  <backgroundColor>" + Utils::String::toHexString(theme->Text.selectorColor) + "</backgroundColor>"
		"</gridtile>"
		"<text name=\"gridtile\">"
		"  <color>" + Utils::String::toHexString(theme->Text.color) + "</color>"
		"  <backgroundColor>00000000</backgroundColor>"
		"  <fontPath>" + theme->Text.font->getPath() +"</fontPath>"
		"  <fontSize>" + std::to_string(sh) + "</fontSize>"
		"  <alignment>center</alignment>"
		"  <singleLineScroll>false</singleLineScroll>"
		"  <size>1 0.30</size>"
		"</text>"
		"<text name=\"gridtile:selected\">"
		"  <color>" + Utils::String::toHexString(theme->Text.selectedColor) + "</color>"
		"</text>"
		"<image name=\"gridtile.image\">"
		"  <linearSmooth>true</linearSmooth>"		
		"  <color>D0D0D0D0</color>"
		"  <roundCorners>0.02</roundCorners>"		
		"</image>"
		"<image name=\"gridtile.image:selected\">"
		"  <color>FFFFFFFF</color>"
		"</image>"
		"</view>"
		"</theme>";

	mTheme = std::shared_ptr<ThemeData>(new ThemeData());
	std::map<std::string, std::string> emptyMap;
	mTheme->loadFile("imageviewer", emptyMap, xml, false);

	//mGrid->setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
	mGrid->applyTheme(mTheme, "grid", "gamegrid", 0);
	mGrid->setCursorChangedCallback([&](const CursorState& /*state*/) { updateHelpPrompts(); });

	loadGrid();
	centerWindow();
}

void GuiSaveState::loadGrid()
{
	mGrid->clear();
		
	auto states = mRepository->getSaveStates(mGame);
	std::sort(states.begin(), states.end(), [](const SaveState* file1, const SaveState* file2) { return file1->creationDate >= file2->creationDate; });

	for (auto item : states)
	{
		if (item->slot == -1)
			mGrid->add(item->creationDate.toLocalTimeString() + std::string("\r\n") + _("AUTO SAVE"), item->getScreenShot(), "", "", false, false, false, false, *item);
		else
			mGrid->add(item->creationDate.toLocalTimeString() + std::string("\r\n") + _("SLOT") + std::string(" ") + std::to_string(item->slot), item->getScreenShot(), "", "", false, false, false, false, *item);
	}

	int slot = SaveStateRepository::getNextFreeSlot(mGame);
	mGrid->add(_("START NEW GAME"), ":/freeslot.svg", "", "", false, false, false, false, SaveState(-2));

	std::string autoSaveMode = mGame->getCurrentGameSetting("autosave");
	if (autoSaveMode.empty() || autoSaveMode == "auto" || autoSaveMode == "2")
	{
		auto autoSave = std::find_if(states.cbegin(), states.cend(), [](SaveState* x) { return x->slot == -1; });
		if (autoSave == states.cend())
			mGrid->add(_("START AUTO SAVE"), ":/freeslot.svg", "", "", false, false, false, false, SaveState(-1));
	}
}

void GuiSaveState::onSizeChanged()
{	
	float helpSize = 0.02;

	if (Settings::getInstance()->getBool("ShowHelpPrompts"))
	{
		HelpStyle help;
		help.applyTheme(mTheme, "system");

		const float height = Math::round(help.font->getLetterHeight() * 1.25f);

		float helpBottom = help.position.y() + (height * mOrigin.y());
		float helpBottomSpace = 0; // Renderer::getScreenHeight() - helpBottom;

		float helpTop = help.position.y() - (height * mOrigin.y()); // +height / 2;
		
		helpSize = helpTop;
		helpSize = Renderer::getScreenHeight() - helpSize + helpBottomSpace;
		helpSize = helpSize / mSize.y() + 0.06;
	}

	mBackground.fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));

	mLayout.setColWidthPerc(0, 0.01);
	mLayout.setColWidthPerc(2, 0.01);

	mLayout.setRowHeightPerc(0, 0.02);

	if (mTitle != nullptr &&  mTitle->getFont() != nullptr)
		mLayout.setRowHeightPerc(1, mTitle->getFont()->getHeight(2.0f) / mSize.y());

	mLayout.setRowHeightPerc(2, 0.02);
	mLayout.setRowHeightPerc(4, helpSize );

	mLayout.setSize(mSize);
}

void GuiSaveState::centerWindow()
{
	setSize(Renderer::getScreenWidth(), WINDOW_HEIGHT);
	animateTo(
		Vector2f((Renderer::getScreenWidth() - getSize().x()) / 2, Renderer::getScreenHeight()),
		Vector2f((Renderer::getScreenWidth() - getSize().x()) / 2, Renderer::getScreenHeight() - WINDOW_HEIGHT),
		AnimateFlags::OPACITY | AnimateFlags::POSITION);
}

bool GuiSaveState::input(InputConfig* config, Input input)
{
	if (input.value != 0 && (config->isMappedTo(BUTTON_BACK, input) || config->isMappedTo("l3", input)))
	{
		delete this;
		return true;
	}

	if (input.value != 0 && config->isMappedTo(BUTTON_OK, input))
	{
		if (mGrid->size())
		{
			const SaveState& item = mGrid->getSelected();
			mRunCallback(item);
		}

		delete this;
		return true;
	}

	if (input.value != 0 && config->isMappedTo("y", input))
	{
		if (mGrid->size())
		{
			mWindow->pushGui(new GuiMsgBox(mWindow, _("ARE YOU SURE YOU WANT TO DELETE THIS ITEM ?"), _("YES"), 
				[this]
				{
					const SaveState& toDelete = mGrid->getSelected();

					mRepository->deleteSaveState(toDelete);
					mRepository->refresh();

					loadGrid();
				}, 
				_("NO"), nullptr));
		}

		return true;
	}
	
	if (input.value != 0 && config->isMappedTo("x", input))
	{
		if (mGrid->size())
		{
			int slot = mRepository->getNextFreeSlot(mGame);
			if (slot >= 0)
			{
				const SaveState& toCopy = mGrid->getSelected();

				mRepository->copyToSlot(toCopy, slot);
				mRepository->refresh();

				loadGrid();
			}
		}

		return true;
	}
	
	return GuiComponent::input(config, input);
}

std::vector<HelpPrompt> GuiSaveState::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));	
	prompts.push_back(HelpPrompt(BUTTON_OK, _("LAUNCH")));

	if (mGrid->size() && !mGrid->getSelected().fileName.empty())
	{
		prompts.push_back(HelpPrompt("y", _("DELETE")));
		prompts.push_back(HelpPrompt("x", _("COPY TO FREE SLOT")));
	}

	return prompts;
}
