#include "GuiGameOptions.h"
#include "guis/GuiGamelistFilter.h"
#include "scrapers/Scraper.h"
#include "views/gamelist/IGameListView.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "components/SwitchComponent.h"
#include "CollectionSystemManager.h"
#include "FileFilterIndex.h"
#include "FileSorts.h"
#include "GuiMetaDataEd.h"
#include "SystemData.h"
#include "LocaleES.h"
#include "guis/GuiMenu.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiTextEditPopup.h"
#include "guis/GuiTextEditPopupKeyboard.h"
#include "scrapers/ThreadedScraper.h"
#include "ThreadedHasher.h"
#include "guis/GuiMenu.h"
#include "ApiSystem.h"
#include "guis/GuiImageViewer.h"
#include "views/SystemView.h"
#include "GuiGameAchievements.h"

GuiGameOptions::GuiGameOptions(Window* window, FileData* game) : GuiComponent(window),
	mMenu(window, game->getName()), mReloadAll(false)
{
	mSystem = game->getSystem();	
	
	// mMenu.setSubTitle(game->getName());

	addChild(&mMenu);

//	mMenu.addButton(_("BACK"), _("go back"), [this] { delete this; });
	

	bool hasManual = ApiSystem::getInstance()->isScriptingSupported(ApiSystem::ScriptId::PDFEXTRACTION) && Utils::FileSystem::exists(game->getMetadata(MetaDataId::Manual));
	bool hasMap = Utils::FileSystem::exists(game->getMetadata(MetaDataId::Map));
	bool hasCheevos = game->hasCheevos();

	if (hasManual || hasMap || hasCheevos)
	{
		mMenu.addGroup(_("GAME MEDIAS"));

		if (hasManual)
		{
			mMenu.addEntry(_("VIEW GAME MANUAL"), false, [window, game, this]
			{
				GuiImageViewer::showPdf(window, game->getMetadata(MetaDataId::Manual));
				delete this;
			});
		}

		if (hasMap)
		{
			mMenu.addEntry(_("VIEW GAME MAP"), false, [window, game, this]
			{
				auto imagePath = game->getMetadata(MetaDataId::Map);
				GuiImageViewer::showImage(window, imagePath, Utils::String::toLower(Utils::FileSystem::getExtension(imagePath)) != ".pdf");
				delete this;
			});
		}

		if (hasCheevos)
		{
			if (!game->isFeatureSupported(EmulatorFeatures::cheevos))
			{
				std::string coreList = game->getSourceFileData()->getSystem()->getCompatibleCoreNames(EmulatorFeatures::cheevos);
				std::string msg = _U("\uF06A  ");
				msg += _("CURRENT CORE IS NOT COMPATIBLE") + " : " + Utils::String::toUpper(game->getCore(true));
				if (!coreList.empty())
				{
					msg += _U("\r\n\uF05A  ");
					msg += _("REQUIRED CORE") + " : " + Utils::String::toUpper(coreList);
				}

				mMenu.addWithDescription(_("VIEW GAME ACHIEVEMENTS"), msg, nullptr, [window, game, this]
				{
					GuiGameAchievements::show(window, Utils::String::toInteger(game->getMetadata(MetaDataId::CheevosId)));
				});
			}
			else
			{
				mMenu.addEntry(_("VIEW GAME ACHIEVEMENTS"), false, [window, game, this]
				{
					GuiGameAchievements::show(window, Utils::String::toInteger(game->getMetadata(MetaDataId::CheevosId)));
				});
			}
		}
	}

	if (game != nullptr && game->isNetplaySupported())
	{
		mMenu.addGroup(_("GAME"));

		mMenu.addEntry(_("START NETPLAY HOST"), false, [window, game, this]
		{
			GuiSettings* msgBox = new GuiSettings(mWindow, _("NETPLAY"));
			msgBox->setSubTitle(game->getName());			
			msgBox->addGroup(_("START GAME"));

			msgBox->addEntry(_U("\uF144 ") + _("START NETPLAY HOST"), false, [this, msgBox, game]
			{
				if (ApiSystem::getInstance()->getIpAdress() == "NOT CONNECTED")
				{
					mWindow->pushGui(new GuiMsgBox(mWindow, _("YOU ARE NOT CONNECTED TO A NETWORK"), _("OK"), nullptr));
					return;
				}

				LaunchGameOptions options;
				options.netPlayMode = SERVER;
				ViewController::get()->launch(game, options);
				msgBox->close();
				delete this;
			});

			msgBox->addGroup(_("OPTIONS"));
			msgBox->addInputTextRow(_("SET PLAYER PASSWORD"), "global.netplay.password", false);
			msgBox->addInputTextRow(_("SET VIEWER PASSWORD"), "global.netplay.spectatepassword", false);

			mWindow->pushGui(msgBox);
		});


		SystemData* all = SystemData::getSystem("all");
		if (all != nullptr && game != nullptr && game->getType() != FOLDER)
		{
			mMenu.addEntry(_("FIND SIMILAR GAMES..."), false, [this, game, all]
			{
				auto index = all->getIndex(true);

				FileFilterIndex* copyOfIndex = new FileFilterIndex();
				copyOfIndex->copyFrom(index);

				index->resetFilters();
				index->setTextFilter(game->getName(), true);

				// Create As Popup And Set Exit Function
				// We need to restore index when we are finished as we are using all games collection
				ViewController::get()->getGameListView(all, true, [index, copyOfIndex]()
				{
					index->copyFrom((FileFilterIndex*)copyOfIndex);
					delete copyOfIndex;
				});

				delete this;
			});
		}

	}


	if (mSystem->isGameSystem() || mSystem->isGroupSystem())
	{
		mMenu.addGroup(_("COLLECTIONS"));
		
		mMenu.addEntry(game->getFavorite() ? _("REMOVE FROM FAVORITES") : _("ADD TO FAVORITES"), false, [this, game]
		{
			CollectionSystemManager::get()->toggleGameInCollection(game, "Favorites");
		});
		
		for (auto customCollection : CollectionSystemManager::get()->getCustomCollectionSystems())
		{
			if (customCollection.second.filteredIndex != nullptr)
				continue;

			std::string collectionName = customCollection.first;
			bool exists = CollectionSystemManager::get()->inInCustomCollection(game, collectionName);
			mMenu.addEntry((exists ? _("REMOVE FROM ") : _("ADD TO ")) + Utils::String::toUpper(collectionName), false, [this, game, collectionName]
			{
				CollectionSystemManager::get()->toggleGameInCollection(game, collectionName);
			});
		}
	}




	bool fromPlaceholder = game->isPlaceHolder();
	if (game->getType() == FOLDER && ((FolderData*)game)->isVirtualStorage())
		fromPlaceholder = true;
	else if (game->getType() == FOLDER && mSystem->getName() == CollectionSystemManager::get()->getCustomCollectionsBundle()->getName())
		fromPlaceholder = true;

	if (!fromPlaceholder)
	{
		mMenu.addGroup(_("GAME OPTIONS"));

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::ScriptId::EVMAPY) && game->getType() != FOLDER)
		{
			if (game->hasKeyboardMapping())
				mMenu.addEntry(_("EDIT PAD TO KEYBOARD CONFIGURATION"), true, [this, game] { GuiMenu::editKeyboardMappings(mWindow, game); });
			else if (game->isFeatureSupported(EmulatorFeatures::Features::padTokeyboard))
				mMenu.addEntry(_("CREATE PAD TO KEYBOARD CONFIGURATION"), true, [this, game] { GuiMenu::editKeyboardMappings(mWindow, game); });
		}

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::GAMESETTINGS))
		{
			auto srcSystem = game->getSourceFileData()->getSystem();
			auto sysOptions = mSystem->isGroupSystem() ? srcSystem : mSystem;

			if (game->getType() != FOLDER)
			{
				if (srcSystem->hasFeatures() || srcSystem->hasEmulatorSelection())
					mMenu.addEntry(_("ADVANCED GAME OPTIONS"), true, [this, game] { GuiMenu::popGameConfigurationGui(mWindow, game); });
			}
		}

		if (game->getType() == FOLDER)
			mMenu.addEntry(_("EDIT FOLDER METADATA"), true, std::bind(&GuiGameOptions::openMetaDataEd, this));
		else
			mMenu.addEntry(_("EDIT THIS GAME'S METADATA"), true, std::bind(&GuiGameOptions::openMetaDataEd, this));
	}
	else if (game->hasKeyboardMapping())
		mMenu.addEntry(_("VIEW PAD TO KEYBOARD INFORMATION"), true, [this, game] { GuiMenu::editKeyboardMappings(mWindow, game); });





	float w = Math::min(Renderer::getScreenWidth() * 0.5, ThemeData::getMenuTheme()->Text.font->sizeText("S").x() * 31.0f);
	w = Math::max(w, Renderer::getScreenWidth() / 3.0f);

	mMenu.setSize(w, Renderer::getScreenHeight() + 2);
	mMenu.animateTo(
		Vector2f(-w, -1),
		Vector2f(-1, -1), AnimateFlags::OPACITY | AnimateFlags::POSITION);
	/*
	mMenu.setMaxHeight(Renderer::getScreenHeight() * 0.85f);
	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	mMenu.animateTo(Vector2f((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2));*/
}


GuiGameOptions::~GuiGameOptions()
{
	if (mSystem == nullptr)
		return;

	for (auto it = mSaveFuncs.cbegin(); it != mSaveFuncs.cend(); it++)
		(*it)();
}

std::string GuiGameOptions::getCustomCollectionName()
{
	std::string editingSystem = mSystem->getName();

	// need to check if we're editing the collections bundle, as we will want to edit the selected collection within
	if (editingSystem == CollectionSystemManager::get()->getCustomCollectionsBundle()->getName())
	{
		FileData* file = getGamelist()->getCursor();
		// do we have the cursor on a specific collection?
		if (file->getType() == FOLDER)
			return file->getName();

		return file->getSystem()->getName();
	}

	return editingSystem;
}

void GuiGameOptions::openMetaDataEd()
{
	if (ThreadedScraper::isRunning() || ThreadedHasher::isRunning())
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("THIS FUNCTION IS DISABLED WHEN SCRAPING IS RUNNING")));
		return;
	}

	// open metadata editor
	// get the FileData that hosts the original metadata
	FileData* file = getGamelist()->getCursor()->getSourceFileData();
	ScraperSearchParams p;
	p.game = file;
	p.system = file->getSystem();

	std::function<void()> deleteBtnFunc = nullptr;

	SystemData* system = file->getSystem();
	if (system->isGroupChildSystem())
		system = system->getParentGroupSystem();

	if (file->getType() == GAME)
	{
		deleteBtnFunc = [this, file, system]
		{
			auto pThis = this;

			CollectionSystemManager::get()->deleteCollectionFiles(file);
			file->deleteGameFiles();

			auto view = ViewController::get()->getGameListView(system, false);
			if (view != nullptr)
				view.get()->remove(file);
			else
			{
				system->getRootFolder()->removeFromVirtualFolders(file);
				delete file;
			}

			delete pThis;
		};
	}

	mWindow->pushGui(new GuiMetaDataEd(mWindow, &file->getMetadata(), file->getMetadata().getMDD(), p, Utils::FileSystem::getFileName(file->getPath()),
		std::bind(&IGameListView::onFileChanged, ViewController::get()->getGameListView(system).get(), file, FILE_METADATA_CHANGED), deleteBtnFunc, file));
}

bool GuiGameOptions::input(InputConfig* config, Input input)
{
	if ((config->isMappedTo(BUTTON_BACK, input) || config->isMappedTo("select", input)) && input.value)
	{
		delete this;
		return true;
	}

	return mMenu.input(config, input);
}

HelpStyle GuiGameOptions::getHelpStyle()
{
	HelpStyle style = HelpStyle();
	style.applyTheme(mSystem->getTheme(), "system");
	return style;
}

std::vector<HelpPrompt> GuiGameOptions::getHelpPrompts()
{
	auto prompts = mMenu.getHelpPrompts();
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("CLOSE")));
	return prompts;
}

IGameListView* GuiGameOptions::getGamelist()
{
	return ViewController::get()->getGameListView(mSystem).get();
}

