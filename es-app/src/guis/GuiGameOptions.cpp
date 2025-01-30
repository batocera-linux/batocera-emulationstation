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
#include "guis/GuiGameScraper.h"
#include "SaveStateRepository.h"
#include "guis/GuiSaveState.h"
#include "SystemConf.h"

GuiGameOptions::GuiGameOptions(Window* window, FileData* game) : GuiComponent(window),
	mMenu(window, game->getName()), mReloadAll(false)
{
	mHasAdvancedGameOptions = false;

	mGame = game;
	mSystem = game->getSystem();	

	auto logo = game->getMarqueePath();
	if (Utils::FileSystem::exists(logo))
	{
		auto image = std::make_shared<ImageComponent>(mWindow, true);  // image expire immediately
		image->setIsLinear(true);
		image->setImage(logo);
		mMenu.setSubTitle("fake");
		mMenu.setTitleImage(image, true);		
	}

	addChild(&mMenu);

	bool isImageViewer = game->getSourceFileData()->getSystem()->hasPlatformId(PlatformIds::IMAGEVIEWER);
	bool hasManual = ApiSystem::getInstance()->isScriptingSupported(ApiSystem::ScriptId::PDFEXTRACTION) && Utils::FileSystem::exists(game->getMetadata(MetaDataId::Manual));
	bool hasMagazine = ApiSystem::getInstance()->isScriptingSupported(ApiSystem::ScriptId::PDFEXTRACTION) && Utils::FileSystem::exists(game->getMetadata(MetaDataId::Magazine));
	bool hasMap = Utils::FileSystem::exists(game->getMetadata(MetaDataId::Map));
	bool hasVideo = Utils::FileSystem::exists(game->getMetadata(MetaDataId::Video));
	bool hasAlternateMedias = game->getSourceFileData()->getFileMedias().size() > 0;
	bool hasCheevos = game->hasCheevos();

	if (hasManual || hasMap || hasCheevos || hasMagazine || hasVideo || hasAlternateMedias)
	{
		mMenu.addGroup(_("GAME MEDIA"));

		if (hasManual)
		{
			mMenu.addEntry(_("VIEW GAME MANUAL"), false, [window, game, this]
			{
				GuiImageViewer::showPdf(window, game->getMetadata(MetaDataId::Manual));
				close();
			});
		}

		if (hasMagazine)
		{
			mMenu.addEntry(_("VIEW GAME MAGAZINE"), false, [window, game, this]
			{
				GuiImageViewer::showPdf(window, game->getMetadata(MetaDataId::Magazine));
				close();
			});
		}		

		if (hasMap)
		{
			mMenu.addEntry(_("VIEW GAME MAP"), false, [window, game, this]
			{
				auto imagePath = game->getMetadata(MetaDataId::Map);
				GuiImageViewer::showImage(window, imagePath, Utils::String::toLower(Utils::FileSystem::getExtension(imagePath)) != ".pdf");
				close();
			});
		}

		if (hasVideo)
		{
			mMenu.addEntry(_("VIEW FULLSCREEN VIDEO"), false, [window, game, this]
			{
				auto imagePath = game->getMetadata(MetaDataId::Video);
				GuiVideoViewer::playVideo(mWindow, imagePath);
				close();
			});
		}
		
		if (hasAlternateMedias)
		{
			mMenu.addEntry(_("VIEW GAME MEDIA"), false, [window, game, this]
			{
				auto imageList = game->getSourceFileData()->getFileMedias();
				GuiImageViewer::showImages(mWindow, imageList);
				close();
			});
		}

		if (hasCheevos)
		{
			if (!game->isFeatureSupported(EmulatorFeatures::cheevos))
			{
				std::string coreList = game->getSourceFileData()->getSystem()->getCompatibleCoreNames(EmulatorFeatures::cheevos);
				std::string msg = _U("\uF06A  ");
				msg += _("CURRENT CORE IS NOT COMPATIBLE") + ": " + Utils::String::toUpper(game->getCore(true).empty()? game->getEmulator(true): game->getCore(true));
				if (!coreList.empty())
				{
					msg += _U("\r\n\uF05A  ");
					msg += _("COMPATIBLE CORE(S)") + ": " + Utils::String::toUpper(coreList);
				}

				mMenu.addWithDescription(_("VIEW THIS GAME'S ACHIEVEMENTS"), msg, nullptr, [window, game, this]
				{
					GuiGameAchievements::show(window, Utils::String::toInteger(game->getMetadata(MetaDataId::CheevosId)));
					close();
				}, "", false, true);
			}
			else
			{
				mMenu.addEntry(_("VIEW THIS GAME'S ACHIEVEMENTS"), false, [window, game, this]
				{
					GuiGameAchievements::show(window, Utils::String::toInteger(game->getMetadata(MetaDataId::CheevosId)));
					close();
				});
			}
		}
	}


	if (game->getType() == GAME)
	{
		mMenu.addGroup(_("GAME"));

		if (SaveStateRepository::isEnabled(game))
		{
			mMenu.addEntry(_("SAVE STATES"), false, [window, game, this]
			{
				mWindow->pushGui(new GuiSaveState(mWindow, game, [this, game](SaveState* state)
				{
					LaunchGameOptions options;
					options.saveStateInfo = state;
					ViewController::get()->launch(game, options);
				}));

				this->close();
			});
		}
		else
		{
			mMenu.addEntry(isImageViewer ? _("OPEN") : _("LAUNCH"), false, [window, game, this]
			{
				ViewController::get()->launch(game);
				this->close();
			});
		}

		if (game->isNetplaySupported())
		{
			mMenu.addEntry(_("START A NETPLAY GAME"), false, [window, game, this]
				{
					GuiSettings* msgBox = new GuiSettings(mWindow, _("NETPLAY"));
					msgBox->setSubTitle(game->getName());
					msgBox->addGroup(_("START GAME"));
					msgBox->addEntry(_U("\uF144 ") + _("HOST A NETPLAY GAME"), false, [window, msgBox, game]
						{
							if (ApiSystem::getInstance()->getIpAddress() == "NOT CONNECTED")
							{
								window->pushGui(new GuiMsgBox(window, _("YOU ARE NOT CONNECTED TO A NETWORK"), _("OK"), nullptr));
								return;
							}
							LaunchGameOptions options;
							options.netPlayMode = SERVER;
							ViewController::get()->launch(game, options);
							msgBox->close();
						});
					msgBox->addGroup(_("OPTIONS"));
					// pubic announce
					auto public_announce = std::make_shared<SwitchComponent>(mWindow);
					public_announce->setState(SystemConf::getInstance()->getBool("global.netplay_public_announce"));
					msgBox->addWithLabel(_("PUBLICLY ANNOUNCE GAME"), public_announce);
					msgBox->addSaveFunc([public_announce] { SystemConf::getInstance()->setBool("global.netplay_public_announce", public_announce->getState()); });

					// passwords
					msgBox->addInputTextRow(_("PLAYER PASSWORD"), "global.netplay.password", false);
					msgBox->addInputTextRow(_("VIEWER PASSWORD"), "global.netplay.spectatepassword", false);
					mWindow->pushGui(msgBox);
					close();
				});
		}

		SystemData* all = SystemData::getSystem("all");
		if (all != nullptr && game != nullptr && game->getType() != FOLDER && !isImageViewer)
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

				close();
			});
		}

		if (UIModeController::getInstance()->isUIModeFull())
		{
			mMenu.addEntry(isImageViewer ? _("DELETE ITEM") : _("DELETE GAME"), false, [this, game]
			{
				mWindow->pushGui(new GuiMsgBox(mWindow, _("THIS WILL DELETE THE ACTUAL GAME FILE(S)!\nARE YOU SURE?"), _("YES"),
					[this, game]
				{
					deleteGame(game);
					close();
				},
					_("NO"), nullptr));


			});
		}
	}

	bool isCustomCollection = (mSystem->isCollection() && game->getType() == FOLDER && CollectionSystemManager::get()->isCustomCollection(mSystem->getName()));
	bool isAppendableToCollection = (game->getType() == GAME) && (mSystem->isGameSystem() || mSystem->isGroupSystem());

	if (UIModeController::getInstance()->isUIModeFull())
	{
		if (isCustomCollection || isAppendableToCollection)
			mMenu.addGroup(_("COLLECTIONS"));

		if (isAppendableToCollection)
		{
			char trstring[1024];

			snprintf(trstring, 1024, std::string(game->getFavorite() ? _("REMOVE FROM %s") : _("ADD TO %s")).c_str(), _("FAVORITES").c_str());
			mMenu.addEntry(trstring, false, [this, game]
			{
				CollectionSystemManager::get()->toggleGameInCollection(game, "Favorites");
				close();
			});

			int addToCollectionCount = 0;
			for (auto customCollection : CollectionSystemManager::get()->getCustomCollectionSystems())
				if (customCollection.second.filteredIndex == nullptr && customCollection.second.isEnabled && !CollectionSystemManager::get()->inInCustomCollection(game, customCollection.first))
					addToCollectionCount++;

			if (addToCollectionCount > 1)
			{
				mMenu.addEntry(_("ADD TO CUSTOM COLLECTION..."), false, [this, game]
				{
					auto pThis = this;
					Window* window = mWindow;

					GuiSettings* msgBox = new GuiSettings(mWindow, _("ADD TO CUSTOM COLLECTION..."));
					msgBox->setTag("popup");
					
					for (auto customCollection : CollectionSystemManager::get()->getCustomCollectionSystems())
					{
						if (customCollection.second.filteredIndex != nullptr || !customCollection.second.isEnabled)
							continue;

						std::string collectionName = customCollection.first;
						if (CollectionSystemManager::get()->inInCustomCollection(game, collectionName))
							continue;
						
						msgBox->addEntry(Utils::String::toUpper(collectionName), false, [pThis, window, msgBox, collectionName, game]
						{
							auto parent = pThis;
							CollectionSystemManager::get()->toggleGameInCollection(game, collectionName);
							msgBox->close();
							parent->close();
						});
					}

					mWindow->pushGui(msgBox);
				//	close();
				});
			}

			for (auto customCollection : CollectionSystemManager::get()->getCustomCollectionSystems())
			{
				if (customCollection.second.filteredIndex != nullptr || !customCollection.second.isEnabled)
					continue;

				std::string collectionName = customCollection.first;
				bool exists = CollectionSystemManager::get()->inInCustomCollection(game, collectionName);
				if (!exists && addToCollectionCount > 1)
					continue;

				snprintf(trstring, 1024, std::string(exists ? _("REMOVE FROM %s") : _("ADD TO %s")).c_str(), Utils::String::toUpper(collectionName).c_str());
				mMenu.addEntry(trstring, false, [this, game, collectionName]
				{
					CollectionSystemManager::get()->toggleGameInCollection(game, collectionName);
					close();
				});
			}


		}

		if (isCustomCollection)
			mMenu.addEntry(_("DELETE COLLECTION"), false, std::bind(&GuiGameOptions::deleteCollection, this));
	}

	bool fromPlaceholder = game->isPlaceHolder();
	if (isImageViewer)
		fromPlaceholder = true; 
	else if (game->getType() == FOLDER && ((FolderData*)game)->isVirtualStorage())
		fromPlaceholder = true;
	else if (game->getType() == FOLDER && mSystem->isCollection()) // >getName() == CollectionSystemManager::get()->getCustomCollectionsBundle()->getName())
		fromPlaceholder = true;

	if (!fromPlaceholder && !isCustomCollection && UIModeController::getInstance()->isUIModeFull())
	{
		mMenu.addGroup(_("OPTIONS"));
		
		mMenu.addEntry(_("SCRAPE"), false, [this, game]
		{
			ScraperSearchParams scraperParams;
			scraperParams.game = game;
			scraperParams.system = game->getSourceFileData()->getSystem();

			GuiGameScraper* scr = new GuiGameScraper(mWindow, scraperParams, [game, scraperParams](const ScraperSearchResult& result)
			{
				game->importP2k(result.p2k);
				game->getMetadata().importScrappedMetadata(result.mdl);	
				game->detectLanguageAndRegion(true);
				game->getMetadata().setScrapeDate(result.scraper);

				ViewController::get()->onFileChanged(game, FILE_METADATA_CHANGED);
			});

			mWindow->pushGui(scr);

			close();
		});

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::ScriptId::EVMAPY) && game->getType() != FOLDER)
		{
			if (game->hasKeyboardMapping())
			{
				mMenu.addEntry(_("EDIT PADTOKEY PROFILE"), false, [this, game]
				{ 
					GuiMenu::editKeyboardMappings(mWindow, game, true); 
					close();
				});
			}
			else if (game->isFeatureSupported(EmulatorFeatures::Features::padTokeyboard))
			{
				mMenu.addEntry(_("CREATE PADTOKEY PROFILE"), false, [this, game]
				{ 
					GuiMenu::editKeyboardMappings(mWindow, game, true);
					close();
				});
			}
		}

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::GAMESETTINGS))
		{
			auto srcSystem = game->getSourceFileData()->getSystem();
			auto sysOptions = !mSystem->isGameSystem() ? srcSystem : mSystem;

			if (game->getType() != FOLDER)
			{
				if (srcSystem->hasFeatures() || srcSystem->hasEmulatorSelection())
				{
					mHasAdvancedGameOptions = true;
					mMenu.addEntry(_("ADVANCED GAME OPTIONS"), false, [this, game]
					{
						GuiMenu::popGameConfigurationGui(mWindow, game);
						close();
					});
				}
			}
		}

		if (game->getType() == FOLDER)
			mMenu.addEntry(_("EDIT FOLDER METADATA"), false, std::bind(&GuiGameOptions::openMetaDataEd, this));
		else
			mMenu.addEntry(_("EDIT THIS GAME'S METADATA"), false, std::bind(&GuiGameOptions::openMetaDataEd, this));
	}
	else if (game->hasKeyboardMapping())
	{
		mMenu.addEntry(_("VIEW PAD TO KEYBOARD INFORMATION"), false, [this, game]
		{ 
			GuiMenu::editKeyboardMappings(mWindow, game, false);
			close();
		});
	}

	if (Renderer::ScreenSettings::fullScreenMenus())
	{	
		mMenu.addButton(_("BACK"), _("go back"), [this] { close(); });

		mMenu.setMaxHeight(Renderer::getScreenHeight() * 0.85f);
		setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
		mMenu.animateTo(Vector2f((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2));
	}
	else
	{
		float w = Math::min(Renderer::getScreenWidth() * 0.5, ThemeData::getMenuTheme()->Text.font->sizeText("S").x() * 33.0f);
		w = Math::max(w, Renderer::getScreenWidth() / 3.0f);

		mMenu.setSize(w, Renderer::getScreenHeight() + 2);
		/*mMenu.animateTo(
			Vector2f(-w, -1),
			Vector2f(-1, -1), AnimateFlags::OPACITY | AnimateFlags::POSITION);
			*/
		mMenu.animateTo(
			Vector2f(Renderer::getScreenWidth(), -1),
			Vector2f(Renderer::getScreenWidth() - w -1, -1), AnimateFlags::OPACITY | AnimateFlags::POSITION);
	}
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

void GuiGameOptions::deleteGame(FileData* file)
{
	if (file->getType() != GAME)
		return;

	auto sourceFile = file->getSourceFileData();

	auto sys = sourceFile->getSystem();
	if (sys->isGroupChildSystem())
		sys = sys->getParentGroupSystem();

	CollectionSystemManager::get()->deleteCollectionFiles(sourceFile);
	sourceFile->deleteGameFiles();

	auto view = ViewController::get()->getGameListView(sys, false);
	if (view != nullptr)
		view.get()->remove(sourceFile);
	else
	{
		sys->getRootFolder()->removeFromVirtualFolders(sourceFile);
		delete sourceFile;
	}
}

void GuiGameOptions::openMetaDataEd()
{
	if (ThreadedScraper::isRunning() || ThreadedHasher::isRunning())
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("THIS FUNCTION IS DISABLED WHILE THE SCRAPER IS RUNNING")));
		return;
	}

	// open metadata editor
	// get the FileData that hosts the original metadata
	FileData* file = mGame;
	ScraperSearchParams p;
	p.game = file;
	p.system = file->getSystem();

	std::function<void()> deleteBtnFunc = nullptr;

	if (file->getType() == GAME)
	{
		auto sourceFile = file->getSourceFileData();
		deleteBtnFunc = [sourceFile] { GuiGameOptions::deleteGame(sourceFile); };
	}

	SystemData* system = file->getSystem();
	if (system->isGroupChildSystem())
		system = system->getParentGroupSystem();

	mWindow->pushGui(new GuiMetaDataEd(mWindow, &file->getMetadata(), file->getMetadata().getMDD(), p, Utils::FileSystem::getFileName(file->getPath()),
		std::bind(&ViewController::onFileChanged, ViewController::get(), file, FILE_METADATA_CHANGED), deleteBtnFunc, file));

	close();
}

bool GuiGameOptions::input(InputConfig* config, Input input)
{
	if ((config->isMappedTo(BUTTON_BACK, input) || config->isMappedTo("select", input)) && input.value)
	{
		close();
		return true;
	}

	if (mHasAdvancedGameOptions && config->isMappedTo("x", input) && input.value)
	{
		GuiMenu::popGameConfigurationGui(mWindow, mGame);
		close();
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
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("CLOSE"), [&] { close(); }));

	if (mHasAdvancedGameOptions)
	{
		prompts.push_back(HelpPrompt("x", _("ADVANCED GAME OPTIONS"), [&] 
		{
			GuiMenu::popGameConfigurationGui(mWindow, mGame);
			close();
		}));
	}

	return prompts;
}

IGameListView* GuiGameOptions::getGamelist()
{
	return ViewController::get()->getGameListView(mSystem).get();
}

void GuiGameOptions::deleteCollection()
{
	if (getCustomCollectionName() == CollectionSystemManager::get()->getCustomCollectionsBundle()->getName())
		return;

	mWindow->pushGui(new GuiMsgBox(mWindow, _("ARE YOU SURE YOU WANT TO DELETE THIS ITEM?"), _("YES"),
		[this]
		{
			std::map<std::string, CollectionSystemData> customCollections = CollectionSystemManager::get()->getCustomCollectionSystems();
			auto customCollection = customCollections.find(getCustomCollectionName());
			if (customCollection == customCollections.cend())
				return;

			if (CollectionSystemManager::get()->deleteCustomCollection(&customCollection->second))
			{
				mWindow->renderSplashScreen("");

				CollectionSystemManager::get()->loadEnabledListFromSettings();
				CollectionSystemManager::get()->updateSystemsList();
				ViewController::get()->goToStart();
				ViewController::get()->reloadAll(mWindow);

				mWindow->closeSplashScreen();			
			}
			delete this;
		}, 
		_("NO"), [this] 
		{
			delete this;
		}));

	
}

void GuiGameOptions::close()
{
	delete this;
}

bool GuiGameOptions::hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult)
{
	if (pResult)
		pResult->push_back(this);

	GuiComponent::hitTest(x, y, parentTransform, pResult);
	return true;
}

bool GuiGameOptions::onMouseClick(int button, bool pressed, int x, int y)
{
	if (pressed && button == 1 && !mMenu.isMouseOver())
	{
		close();
		return true;
	}

	return (button == 1);
}
