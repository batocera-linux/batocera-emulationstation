#include "guis/GuiMetaDataEd.h"

#include "components/ButtonComponent.h"
#include "components/ComponentList.h"
#include "components/DateTimeEditComponent.h"
#include "components/MenuComponent.h"
#include "components/RatingComponent.h"
#include "components/SwitchComponent.h"
#include "components/TextComponent.h"
#include "guis/GuiFileBrowser.h"
#include "guis/GuiGameScraper.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiTextEditPopup.h"
#include "resources/Font.h"
#include "utils/StringUtil.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "FileData.h"
#include "FileFilterIndex.h"
#include "SystemData.h"
#include "Window.h"
#include "LocaleES.h"
#include "guis/GuiTextEditPopupKeyboard.h"
#include "Gamelist.h"
#include "components/OptionListComponent.h"
#include "ApiSystem.h"
#include "scrapers/Scraper.h"
#include "Genres.h"
#include <algorithm>
#include <set>

GuiMetaDataEd::GuiMetaDataEd(Window* window, MetaDataList* md, const std::vector<MetaDataDecl>& mdd, ScraperSearchParams scraperParams,
	const std::string& /*header*/, std::function<void()> saveCallback, std::function<void()> deleteFunc, FileData* file) : GuiComponent(window),
	mScraperParams(scraperParams),

	mBackground(window, ":/frame.png"),
	mGrid(window, Vector2i(1, 3)),

	mMetaDataDecl(mdd),
	mMetaData(md),
	mSavedCallback(saveCallback), mDeleteFunc(deleteFunc)
{
	auto theme = ThemeData::getMenuTheme();
	mBackground.setImagePath(theme->Background.path); // ":/frame.png"
	mBackground.setEdgeColor(theme->Background.color);
	mBackground.setCenterColor(theme->Background.centerColor);
	mBackground.setCornerSize(theme->Background.cornerSize);

	addChild(&mBackground);
	addChild(&mGrid);

	mHeaderGrid = std::make_shared<ComponentGrid>(mWindow, Vector2i(1, 5));

	mTitle = std::make_shared<TextComponent>(mWindow, _("EDIT METADATA"), theme->Title.font, theme->Title.color, ALIGN_CENTER); // batocera
	
	auto subTitle = Utils::FileSystem::createRelativePath(scraperParams.game->getPath(), scraperParams.game->getSourceFileData()->getSystem()->getRootFolder()->getPath(), true);
	if (Utils::String::startsWith(subTitle, "./"))
		subTitle = subTitle.substr(2);
	else
		subTitle = Utils::FileSystem::getFileName(scraperParams.game->getPath());

	mSubtitle = std::make_shared<TextComponent>(mWindow, subTitle, theme->TextSmall.font, theme->TextSmall.color, ALIGN_CENTER);

	mHeaderGrid->setEntry(mTitle, Vector2i(0, 1), false, true);
	mHeaderGrid->setEntry(mSubtitle, Vector2i(0, 3), false, true);

	mGrid.setEntry(mHeaderGrid, Vector2i(0, 0), false, true);

	mList = std::make_shared<ComponentList>(mWindow);
	mGrid.setEntry(mList, Vector2i(0, 1), true, true);

	SystemData* system = file->getSystem();

	auto emul_choice = std::make_shared<OptionListComponent<std::string>>(mWindow, _("Emulator"), false);
	auto core_choice = std::make_shared<OptionListComponent<std::string>>(mWindow, _("Core"), false);
	std::string relativePath = mMetaData->getRelativeRootPath();

	// populate list
	for(auto iter = mdd.cbegin(); iter != mdd.cend(); iter++)
	{
		std::shared_ptr<GuiComponent> ed;

		// don't add statistics
		if(iter->isStatistic)
			continue;

		if (file->getType() != GAME && !iter->visibleForFolder)
			continue;

		// create ed and add it (and any related components) to mMenu
		// ed's value will be set below
		ComponentListRow row;

		if (iter->key == "emulator")
		{
			if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::GAMESETTINGS))
				continue;

			std::string defaultEmul = system->getDefaultEmulator();
			std::string currentEmul = file->getEmulator(false);

			if (defaultEmul.length() == 0)
				emul_choice->add(_("AUTO"), "", true);
			else
				emul_choice->add(_("AUTO") + " (" + defaultEmul + ")", "", currentEmul.length() == 0);

			for (auto core : file->getSystem()->getEmulators())
				emul_choice->add(core.name, core.name, core.name == currentEmul);

			row.addElement(std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(_("Emulator")), theme->Text.font, theme->Text.color), true);
			row.addElement(emul_choice, false);

			mList->addRow(row);
			emul_choice->setTag(iter->key);
			mEditors.push_back(emul_choice);

			emul_choice->setSelectedChangedCallback([this, system, core_choice, file](std::string emulatorName)
			{
				std::string currentCore = file->getCore(false);

				std::string defaultCore = system->getDefaultCore(emulatorName);
				if (emulatorName.length() == 0)
					defaultCore = system->getDefaultCore(system->getDefaultEmulator());

				core_choice->clear();
				if (defaultCore.length() == 0)
					core_choice->add(_("AUTO"), "", false);
				else
					core_choice->add(_("AUTO") + " (" + defaultCore + ")", "", false);

				std::vector<std::string> cores = system->getCoreNames(emulatorName);

				bool found = false;

				for (auto it = cores.begin(); it != cores.end(); it++)
				{
					std::string core = *it;
					core_choice->add(core, core, currentCore == core);
					if (currentCore == core)
						found = true;
				}

				if (!found)
					core_choice->selectFirstItem();
				else
					core_choice->invalidate();
			});

			continue;
		}

		if (iter->key == "core")
		{
			if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::GAMESETTINGS))
				continue;

			core_choice->setTag(iter->key);

			row.addElement(std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(_("Core")), theme->Text.font, theme->Text.color), true);
			row.addElement(core_choice, false);

			mList->addRow(row);
			ed = core_choice;

			mEditors.push_back(core_choice);

			// force change event to load core list
			emul_choice->invalidate();
			continue;
		}

		if (iter->key == "genre")
			continue;

		if (iter->key == "genres")
		{
			auto selectedGenres = Utils::String::split(mMetaData->get(MetaDataId::GenreIds), ',');

			auto genres = std::make_shared<OptionListComponent<std::string>>(mWindow, _("GENRES"), true, true);
			genres->setTag("genres");
			ed = genres;

			for (auto genre : Genres::getGameGenres())
			{
				bool selected = std::find(selectedGenres.cbegin(), selectedGenres.cend(), std::to_string(genre->id)) != selectedGenres.cend();
				genres->add(genre->getLocalizedName(), std::to_string(genre->id), selected, false, genre->parentId > 0);
			}

			row.addElement(std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(_("GENRES")), theme->Text.font, theme->Text.color), true);
			row.addElement(genres, false);

			mList->addRow(row);

			mEditors.push_back(genres);

			continue;
		}

		auto lbl = std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(iter->displayName), theme->Text.font, theme->Text.color);
		row.addElement(lbl, true); // label

		switch(iter->type)
		{
		case MD_BOOL:
			{
				ed = std::make_shared<SwitchComponent>(window);
				row.addElement(ed, false, true);
				break;
			}
		case MD_RATING:
			{
				ed = std::make_shared<RatingComponent>(window);
				const float height = lbl->getSize().y() * 0.71f;
				ed->setSize(0, height);
				row.addElement(ed, false, true);

				auto spacer = std::make_shared<GuiComponent>(mWindow);
				spacer->setSize(Renderer::getScreenWidth() * 0.0025f, 0);
				row.addElement(spacer, false);

				// pass input to the actual RatingComponent instead of the spacer
				row.input_handler = std::bind(&GuiComponent::input, ed.get(), std::placeholders::_1, std::placeholders::_2);

				break;
			}
		case MD_DATE:
			{
				ed = std::make_shared<DateTimeEditComponent>(window);
				row.addElement(ed, false);

				auto spacer = std::make_shared<GuiComponent>(mWindow);
				spacer->setSize(Renderer::getScreenWidth() * 0.0025f, 0);
				row.addElement(spacer, false);

				// pass input to the actual DateTimeEditComponent instead of the spacer
				row.input_handler = std::bind(&GuiComponent::input, ed.get(), std::placeholders::_1, std::placeholders::_2);

				break;
			}
		case MD_TIME:
			{
				ed = std::make_shared<DateTimeEditComponent>(window, DateTimeEditComponent::DISP_RELATIVE_TO_NOW);
				row.addElement(ed, false);
				break;
			}

		case MD_PATH:
		{
			ed = std::make_shared<TextComponent>(window, "", theme->Text.font, theme->Text.color, ALIGN_RIGHT);
			row.addElement(ed, true);

			auto spacer = std::make_shared<GuiComponent>(mWindow);
			spacer->setSize(Renderer::getScreenWidth() * 0.005f, 0);
			row.addElement(spacer, false);

			auto bracket = std::make_shared<ImageComponent>(mWindow);
			bracket->setImage(ThemeData::getMenuTheme()->Icons.arrow);
			bracket->setResize(Vector2f(0, lbl->getFont()->getLetterHeight()));
			row.addElement(bracket, false);

			GuiFileBrowser::FileTypes type = GuiFileBrowser::FileTypes::IMAGES;

			if (iter->key == "video")
				type = GuiFileBrowser::FileTypes::VIDEO;
			else if (iter->key == "manual" || iter->key == "magazine" || iter->key == "map")
				type = (GuiFileBrowser::FileTypes) (GuiFileBrowser::FileTypes::IMAGES | GuiFileBrowser::FileTypes::MANUALS);

			auto updateVal = [ed, relativePath](const std::string& newVal)
			{ 
				auto val = Utils::FileSystem::createRelativePath(newVal, relativePath, true);
				ed->setValue(val);
			};

			row.makeAcceptInputHandler([this, type, ed, iter, updateVal, relativePath]
			{			
				std::string filePath = ed->getValue();
				if (!filePath.empty())
					filePath = Utils::FileSystem::resolveRelativePath(filePath, relativePath, true);
				
				std::string dir = Utils::FileSystem::getParent(filePath);
				if (dir.empty())
					dir = relativePath;

				std::string title = iter->displayName + " - " + mMetaData->getName();
				mWindow->pushGui(new GuiFileBrowser(mWindow, dir, filePath, type, updateVal, title));
			});

			break;
		}

		case MD_MULTILINE_STRING:
		default:
			{
				// MD_STRING
				ed = std::make_shared<TextComponent>(window, "", theme->Text.font, theme->Text.color, ALIGN_RIGHT);
				row.addElement(ed, true);

				auto spacer = std::make_shared<GuiComponent>(mWindow);
				spacer->setSize(Renderer::getScreenWidth() * 0.005f, 0);
				row.addElement(spacer, false);

				auto bracket = std::make_shared<ImageComponent>(mWindow);
				bracket->setImage(ThemeData::getMenuTheme()->Icons.arrow);
				bracket->setResize(Vector2f(0, lbl->getFont()->getLetterHeight()));
				row.addElement(bracket, false);

				bool multiLine = iter->type == MD_MULTILINE_STRING;
				const std::string title = iter->displayPrompt;
				auto updateVal = [ed](const std::string& newVal) { ed->setValue(newVal); }; // ok callback (apply new value to ed)
				row.makeAcceptInputHandler([this, title, ed, updateVal, multiLine] 
				{
				    // batocera
					if (!multiLine && Settings::getInstance()->getBool("UseOSK"))
						mWindow->pushGui(new GuiTextEditPopupKeyboard(mWindow, title, ed->getValue(), updateVal, multiLine));
				    else
						mWindow->pushGui(new GuiTextEditPopup(mWindow, title, ed->getValue(), updateVal, multiLine));				    
				});
				break;
			}
		}

		assert(ed);
		mList->addRow(row);

		ed->setTag(iter->key);
		ed->setValue(mMetaData->get(iter->key, false));
		mEditors.push_back(ed);
	}

	std::vector< std::shared_ptr<ButtonComponent> > buttons;

	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("SAVE"), _("SAVE"), [&] { if (save()) { delete this; } }));

	if (!scraperParams.system->hasPlatformId(PlatformIds::PLATFORM_IGNORE))
		buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("SCRAPE"), _("SCRAPE"), std::bind(&GuiMetaDataEd::fetch, this)));

	if(mDeleteFunc)
	{
		auto deleteFileAndSelf = [&] { mDeleteFunc(); delete this; };
		auto deleteBtnFunc = [this, deleteFileAndSelf] { mWindow->pushGui(new GuiMsgBox(mWindow, _("THIS WILL DELETE THE ACTUAL GAME FILE(S)!\nARE YOU SURE?"), _("YES"), deleteFileAndSelf, _("NO"), nullptr)); };
		buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("DELETE"), _("DELETE"), deleteBtnFunc));
	}

	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("CANCEL"), _("CANCEL"), [&] { delete this; }));

	mButtons = makeButtonGrid(mWindow, buttons);
	mGrid.setEntry(mButtons, Vector2i(0, 2), true, false);

	mGrid.setUnhandledInputCallback([this](InputConfig* config, Input input) -> bool {
		if (config->isMappedLike("down", input)) {
			mGrid.setCursorTo(mList);
			mList->setCursorIndex(0);
			return true;
		}
		if (config->isMappedLike("up", input)) {
			mList->setCursorIndex(mList->size() - 1);
			mGrid.moveCursor(Vector2i(0, 1));
			return true;
		}
		return false;
	});

	// resize + center	

	if (Renderer::isSmallScreen())
		setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
	else
	{
		float width = Renderer::getScreenWidth() * 0.72f; // (float)Math::min(Renderer::getScreenHeight(), (int)(Renderer::getScreenWidth() * 0.90f));
		if (width < Renderer::getScreenHeight())
			width = Renderer::getScreenHeight();

		setSize(width, Renderer::getScreenHeight() * 0.82f);
	}

	setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, (Renderer::getScreenHeight() - mSize.y()) / 2);
}

void GuiMetaDataEd::onSizeChanged()
{
	mBackground.fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));

	mGrid.setSize(mSize);

	const float titleHeight = mTitle->getFont()->getLetterHeight();
	const float subtitleHeight = mSubtitle->getFont()->getLetterHeight();
	const float titleSubtitleSpacing = mSize.y() * 0.03f;

	mGrid.setRowHeightPerc(0, (titleHeight + titleSubtitleSpacing + subtitleHeight + TITLE_VERT_PADDING) / mSize.y());
	mGrid.setRowHeightPerc(2, mButtons->getSize().y() / mSize.y());

	mHeaderGrid->setRowHeightPerc(1, titleHeight / mHeaderGrid->getSize().y());
	mHeaderGrid->setRowHeightPerc(2, titleSubtitleSpacing / mHeaderGrid->getSize().y());
	mHeaderGrid->setRowHeightPerc(3, subtitleHeight / mHeaderGrid->getSize().y());
}

bool GuiMetaDataEd::isStatistic(const std::string name)
{
	for (auto in : mMetaDataDecl)
		if (in.key == name && in.isStatistic)
			return true;

	return false;
}

bool GuiMetaDataEd::save()
{
	std::set<std::string> externalFilesToCopy;

	auto performSave = [this](std::set<std::string> filesToCopy)
	{
		// remove game from index
		mScraperParams.system->removeFromIndex(mScraperParams.game);

		for (unsigned int i = 0; i < mEditors.size(); i++)
		{
			std::shared_ptr<GuiComponent> ed = mEditors.at(i);

			auto val = ed->getValue();
			auto key = ed->getTag();

			if (isStatistic(key))
				continue;

			if (key == "genre")
				continue;

			if (key == "genres")
			{
				std::shared_ptr<OptionListComponent<std::string>> list = std::static_pointer_cast<OptionListComponent<std::string>>(ed);
				val = Utils::String::join(list->getSelectedObjects(), ",");

				if (val != mMetaData->get(MetaDataId::GenreIds))
					mMetaData->set("genre", Genres::genreStringFromIds(list->getSelectedObjects(), false));
			}

			if (key == "core" || key == "emulator")
			{
				std::shared_ptr<OptionListComponent<std::string>> list = std::static_pointer_cast<OptionListComponent<std::string>>(ed);
				val = list->getSelected();
			}

			if (mMetaData->getType(key) == MD_PATH && !val.empty() && filesToCopy.find(val) != filesToCopy.cend())
			{				
				auto rootPath = mMetaData->getRelativeRootPath();
				auto sourceFile = Utils::FileSystem::resolveRelativePath(val, rootPath, true);

				auto destFile = Scraper::getSaveAsPath(mScraperParams.game, mMetaData->getId(key), Utils::FileSystem::getExtension(sourceFile));

				if (Utils::FileSystem::copyFile(sourceFile, destFile))
					val = destFile;
			}

			mMetaData->set(key, val);
		}

		// enter game in index
		mScraperParams.system->addToIndex(mScraperParams.game);
		mScraperParams.game->importP2k(mScrappedPk2);

		if (mSavedCallback)
			mSavedCallback();

		saveToGamelistRecovery(mScraperParams.game);

		// update respective Collection Entries
		CollectionSystemManager::get()->refreshCollectionSystems(mScraperParams.game);
	};

	for (unsigned int i = 0; i < mEditors.size(); i++)
	{
		std::shared_ptr<GuiComponent> ed = mEditors.at(i);
		auto key = ed->getTag();
		if (isStatistic(key))
			continue;

		auto val = ed->getValue();
		if (mMetaData->getType(key) == MD_PATH && !val.empty())
		{
			auto root = mMetaData->getRelativeRootPath();
			auto abs = Utils::FileSystem::resolveRelativePath(val, root, true);

			auto cur = mMetaData->get(key, true);
			if (abs != cur && !Utils::String::startsWith(abs, root))
				externalFilesToCopy.insert(val);
		}
	}

	if (externalFilesToCopy.size())
	{
		mWindow->pushGui(new GuiMsgBox(mWindow,
			_("SOME FILES YOU LINKED ARE LOCATED OUTSIDE THE GAMELIST PATH.\nDO YOU WANT TO USE A COPY OF THESE FILES RELATIVE TO THE GAMELIST ?"),
			_("YES"), [this, performSave, externalFilesToCopy] { performSave(externalFilesToCopy); delete this; },
			_("NO"), [this, performSave] { performSave(std::set<std::string>()); delete this; },
			_("CANCEL"), nullptr, ICON_QUESTION
		));
		
		return false;
	}
	
	performSave(externalFilesToCopy);
	return true;
}

void GuiMetaDataEd::fetch()
{
//	mScraperParams.nameOverride = mScraperParams.game->getName();

	GuiGameScraper* scr = new GuiGameScraper(mWindow, mScraperParams, std::bind(&GuiMetaDataEd::fetchDone, this, std::placeholders::_1));
	mWindow->pushGui(scr);
}

void GuiMetaDataEd::fetchDone(const ScraperSearchResult& result)
{
	for (unsigned int i = 0; i < mEditors.size(); i++)
	{
		auto key = mEditors.at(i)->getTag();
		if (isStatistic(key))
			continue;

		// Don't override favorite & hidden values, as they are not statistics
		if (key == "favorite" || key == "hidden")
			continue;

		if (key == "rating" && result.mdl.getFloat(MetaDataId::Rating) < 0)
			continue;
		
		// Don't override medias when scrap result has nothing
		if ((key == "image" || key == "thumbnail" || key == "marquee" || key == "fanart" || key == "titleshot" || key == "manual" || key == "map" || key == "video" || key == "magazine") && result.mdl.get(key).empty())
			continue;

		mEditors.at(i)->setValue(result.mdl.get(key));
	}

	mScrappedPk2 = result.p2k;
}

void GuiMetaDataEd::close(bool closeAllWindows)
{
	// find out if the user made any changes
	bool dirty = false;
	for(unsigned int i = 0; i < mEditors.size(); i++)
	{
		auto ed = mEditors.at(i);
		auto key = ed->getTag();
		auto value = ed->getValue();

		if (key == "core" || key == "emulator")
		{
			std::shared_ptr<OptionListComponent<std::string>> list = std::static_pointer_cast<OptionListComponent<std::string>>(ed);
			value = list->getSelected();
		}

		if (key == "genre")
			continue;

		if (key == "genres")
		{
			std::shared_ptr<OptionListComponent<std::string>> list = std::static_pointer_cast<OptionListComponent<std::string>>(ed);
			value = Utils::String::join(list->getSelectedObjects(), ",");
		}

		std::string mdv = mMetaData->get(key, false);

		if (key == "rating")
		{
			mdv = std::to_string(atof(mdv.c_str()));
			value = std::to_string(atof(value.c_str()));
		}
		else
			mdv = Utils::String::replace(mdv, "not-a-date-time", "19700101T010000");
		
		if (mdv != value)
		{
			dirty = true;
			break;
		}
	}

	std::function<void()> closeFunc;
	if(!closeAllWindows)
	{
		closeFunc = [this] { delete this; };
	}else{
		Window* window = mWindow;
		closeFunc = [window, this] {
			while(window->peekGui() != ViewController::get())
				delete window->peekGui();
		};
	}


	if(dirty)
	{
		// changes were made, ask if the user wants to save them
		mWindow->pushGui(new GuiMsgBox(mWindow,
			_("SAVE CHANGES?"), // batocera
			_("YES"), [this, closeFunc] { if (save()) closeFunc(); }, // batocera
			_("NO"), closeFunc // batocera
		));
	}else{
		closeFunc();
	}
}

bool GuiMetaDataEd::input(InputConfig* config, Input input)
{
	if(GuiComponent::input(config, input))
		return true;

	const bool isStart = config->isMappedTo("start", input);
	if(input.value != 0 && (config->isMappedTo(BUTTON_BACK, input) || isStart))
	{
		close(isStart);
		return true;
	}

	return false;
}

std::vector<HelpPrompt> GuiMetaDataEd::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mGrid.getHelpPrompts();
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	prompts.push_back(HelpPrompt("start", _("CLOSE"))); // batocera
	return prompts;
}
