#include "views/ViewController.h"

#include "animations/Animation.h"
#include "animations/LambdaAnimation.h"
#include "animations/LaunchAnimation.h"
#include "animations/MoveCameraAnimation.h"
#include "guis/GuiMenu.h"
#include "views/gamelist/DetailedGameListView.h"
#include "views/gamelist/IGameListView.h"
#include "views/gamelist/GridGameListView.h"
#include "views/gamelist/VideoGameListView.h"
#include "views/gamelist/CarouselGameListView.h"
#include "views/SystemView.h"
#include "views/UIModeController.h"
#include "FileFilterIndex.h"
#include "Log.h"
#include "Scripting.h"
#include "Settings.h"
#include "SystemData.h"
#include "Window.h"
#include "guis/GuiDetectDevice.h"
#include "SystemConf.h"
#include "AudioManager.h"
#include "FileSorts.h"
#include "CollectionSystemManager.h"
#include "guis/GuiImageViewer.h"
#include "ApiSystem.h"
#include "guis/GuiMsgBox.h"
#include "utils/ThreadPool.h"
#include <SDL_timer.h>
#include "TextToSpeech.h"
#include "VolumeControl.h"
#include "guis/GuiNetPlay.h"

ViewController* ViewController::sInstance = nullptr;

ViewController* ViewController::get()
{
	return sInstance;
}

void ViewController::deinit()
{
	if (sInstance != nullptr)
		delete sInstance;

	sInstance = nullptr;
}

void ViewController::init(Window* window)
{
	if (sInstance != nullptr)
		delete sInstance;

	sInstance = new ViewController(window);	
}

void ViewController::saveState()
{
	if (sInstance == nullptr)
		return;

	if (Settings::getInstance()->getString("StartupSystem") != "lastsystem")
		return;
	
	SystemData* activeSystem = nullptr;

	if (sInstance->mState.viewing == GAME_LIST)
		activeSystem = sInstance->mState.getSystem();
	else if (sInstance->mState.viewing == SYSTEM_SELECT)
		activeSystem = sInstance->mSystemListView->getActiveSystem();

	if (activeSystem != nullptr)
	{
		if (Settings::getInstance()->setString("LastSystem", activeSystem->getName()))
			Settings::getInstance()->saveFile();
	}	
}

ViewController::ViewController(Window* window)
	: GuiComponent(window), mCurrentView(nullptr), mCamera(Transform4x4f::Identity()), mFadeOpacity(0), mLockInput(false)
{
	mSystemListView = nullptr;
	mState.viewing = NOTHING;	
	mState.system = nullptr;
}

ViewController::~ViewController()
{	
	ISimpleGameListView* simpleView = dynamic_cast<ISimpleGameListView*>(mCurrentView.get());
	if (simpleView != nullptr)
		simpleView->closePopupContext();

	sInstance = nullptr;
}

void ViewController::goToStart(bool forceImmediate)
{
	bool startOnGamelist = Settings::getInstance()->getBool("StartupOnGameList");

	// If specific system is requested, go directly to the game list
	auto requestedSystem = Settings::getInstance()->getString("StartupSystem");
	if (requestedSystem == "lastsystem")
		requestedSystem = Settings::getInstance()->getString("LastSystem");

	if("" != requestedSystem && "retropie" != requestedSystem)
	{
		auto system = SystemData::getSystem(requestedSystem);
		if (system != nullptr && !system->isGroupChildSystem())
		{
			if (startOnGamelist)
				goToGameList(system, forceImmediate);
			else
				goToSystemView(system, forceImmediate);

			return;
		}
	}

	if (startOnGamelist)
		goToGameList(SystemData::getFirstVisibleSystem(), forceImmediate);
	else
		goToSystemView(SystemData::getFirstVisibleSystem());
}

void ViewController::ReloadAndGoToStart()
{
	mWindow->renderSplashScreen(_("Loading..."));
	ViewController::get()->reloadAll();
	ViewController::get()->goToStart(true);
	mWindow->closeSplashScreen();
}

int ViewController::getSystemId(SystemData* system)
{
	int id = 0;
	for (auto sys : SystemData::sSystemVector)
	{
		if (!sys->isVisible())
			continue;

		if (system == sys)
			return id;

		id++;
	}

	return -1;
}

void ViewController::goToSystemView(std::string& systemName, bool forceImmediate, ViewController::ViewMode mode)
{
	auto system = SystemData::getSystem(systemName);
	if (system != nullptr)
	{
		if (mode == ViewController::ViewMode::GAME_LIST)
			goToGameList(system, forceImmediate);
		else
			goToSystemView(system, forceImmediate);
	}
}

void ViewController::goToSystemView(SystemData* system, bool forceImmediate)
{
	SystemData* dest = system;

	int systemId = getSystemId(dest);

	if (system->isCollection())
	{
		SystemData* bundle = CollectionSystemManager::get()->getCustomCollectionsBundle();
		if (bundle != nullptr && systemId < 0)
			dest = bundle;
	}

	// Tell any current view it's about to be hidden
	if (mCurrentView)
		mCurrentView->onHide();

	// Realign system view
	auto systemList = getSystemListView();
	systemList->setPosition(systemId * (float)Renderer::getScreenWidth(), systemList->getPosition().y());
	
	// Realign every gamelist views
	for (auto gameList : mGameListViews)
	{
		int id = getSystemId(gameList.first);
		gameList.second->setPosition(id * (float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight() * 2);
	}

	// Realign translation
	mCamera.translation().x() = -(systemId * (float)Renderer::getScreenWidth());

	mState.viewing = SYSTEM_SELECT;
	mState.system = dest;

	Scripting::fireEvent("system-selected", dest->getName());

	systemList->goToSystem(dest, false);

	mCurrentView = systemList;

	playViewTransition(forceImmediate);
}

void ViewController::goToNextGameList()
{
	assert(mState.viewing == GAME_LIST);
	SystemData* system = getState().getSystem();
	assert(system);
	goToGameList(system->getNext());
}

void ViewController::goToPrevGameList()
{
	assert(mState.viewing == GAME_LIST);
	SystemData* system = getState().getSystem();
	assert(system);
	goToGameList(system->getPrev());
}

bool ViewController::goToGameList(const std::string& systemName, bool forceImmediate)
{
	auto system = SystemData::getSystem(systemName);
	if (system != nullptr && !system->isGroupChildSystem())
	{
		goToGameList(system, forceImmediate);
		return true;
	}

	return false;
}

void ViewController::goToGameList(SystemData* system, bool forceImmediate)
{
	if (system == nullptr)
		return;

	SystemData* destinationSystem = system;
	FolderData* collectionFolder = nullptr;

	if (system->isCollection())
	{
		SystemData* bundle = CollectionSystemManager::get()->getCustomCollectionsBundle();
		if (bundle != nullptr)
		{
			for (auto child : bundle->getRootFolder()->getChildren())
			{
				if (child->getType() == FOLDER && child->getName() == system->getName())
				{
					collectionFolder = (FolderData*)child;
					destinationSystem = bundle;
					break;
				}
			}
		}
	}

	std::shared_ptr<IGameListView> view = getGameListView(destinationSystem);


	if (mState.viewing == SYSTEM_SELECT)
	{
		// move system list
		auto sysList = getSystemListView();
		float offX = sysList->getPosition().x();
		sysList->setPosition(view->getPosition().x(), sysList->getPosition().y());
		offX = sysList->getPosition().x() - offX;
		mCamera.translation().x() -= offX;
	}	
	else if (mState.viewing == GAME_LIST && mState.system != nullptr)
	{
		// Realign current view to center
		auto currentView = getGameListView(mState.system, false);
		if (currentView != nullptr)
		{
			int currentIndex = 0;

			std::vector<std::pair<int, SystemData*>> ids;
			for (auto sys : SystemData::sSystemVector)
			{
				if (sys->isVisible())
				{
					if (sys == destinationSystem)
						currentIndex = ids.size();

					ids.push_back(std::pair<int, SystemData*>(ids.size(), sys));
				}
			}

			if (ids.size() > 2)
			{
				int rotateBy = (ids.size() / 2) - currentIndex;
				if (rotateBy != 0)
				{
					if (rotateBy < 0)
						std::rotate(ids.begin(), ids.begin() - rotateBy, ids.end());
					else
						std::rotate(ids.rbegin(), ids.rbegin() + rotateBy, ids.rend());

					for (auto gameList : mGameListViews)
					{
						for (int idx = 0; idx < ids.size(); idx++)
						{
							if (gameList.first == ids[idx].second)
							{
								gameList.second->setPosition(idx * (float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight() * 2);
								break;
							}
						}
					}

					mCamera.translation().x() = -currentView->getPosition().x();
				}
			}
		}
	}	
	
	mState.viewing = GAME_LIST;
	mState.system = destinationSystem;
	
	if (collectionFolder != nullptr)
	{
		ISimpleGameListView* simpleView = dynamic_cast<ISimpleGameListView*>(view.get());
		if (simpleView != nullptr)
			simpleView->moveToFolder(collectionFolder);
	}	

	if (AudioManager::isInitialized())
		AudioManager::getInstance()->changePlaylist(system->getTheme());

	mDeferPlayViewTransitionTo = nullptr;

	if (forceImmediate || Settings::TransitionStyle() == "fade")
	{
		if (mCurrentView)
			mCurrentView->onHide();

		mCurrentView = view;
		playViewTransition(forceImmediate);
	}
	else
	{
		cancelAnimation(0);
		mDeferPlayViewTransitionTo = view;
	}
}

void ViewController::playViewTransition(bool forceImmediate)
{
	if (mCurrentView)
		mCurrentView->onShow();

	Vector3f target(Vector3f::Zero());
	if(mCurrentView)
		target = mCurrentView->getPosition();

	// no need to animate, we're not going anywhere (probably goToNextGamelist() or goToPrevGamelist() when there's only 1 system)
	if(target == -mCamera.translation() && !isAnimationPlaying(0))
		return;

	std::string transition_style = Settings::TransitionStyle();

	// check <theme defaultTransition> value
	if ((transition_style.empty() || transition_style == "auto") && getState().system != nullptr && getState().system->getTheme() != nullptr)
	{
		auto defaultTransition = getState().system->getTheme()->getDefaultTransition();
		if (!defaultTransition.empty() && defaultTransition != "auto")
			transition_style = defaultTransition;
	}

	if (Settings::PowerSaverMode() == "instant")
		transition_style = "instant";

	if (transition_style == "fade & slide")
		transition_style = "slide";

	if(transition_style == "fade")
	{
		// fade
		// stop whatever's currently playing, leaving mFadeOpacity wherever it is
		cancelAnimation(0);

		auto fadeFunc = [this](float t) {
			mFadeOpacity = Math::lerp(0, 1, t);
		};

		const static int FADE_DURATION = 240; // fade in/out time
		const static int FADE_WAIT = 320; // time to wait between in/out
		setAnimation(new LambdaAnimation(fadeFunc, FADE_DURATION), 0, [this, fadeFunc, target] {
			this->mCamera.translation() = -target;
			updateHelpPrompts();
			setAnimation(new LambdaAnimation(fadeFunc, FADE_DURATION), FADE_WAIT, nullptr, true);
		});

		// fast-forward animation if we're partway faded
		if(target == -mCamera.translation())
		{
			// not changing screens, so cancel the first half entirely
			advanceAnimation(0, FADE_DURATION);
			advanceAnimation(0, FADE_WAIT);
			advanceAnimation(0, FADE_DURATION - (int)(mFadeOpacity * FADE_DURATION));
		}
		else
			advanceAnimation(0, (int)(mFadeOpacity * FADE_DURATION));		
	} 
	else if (transition_style == "slide" || transition_style == "auto")
	{
		// slide or simple slide
		setAnimation(new MoveCameraAnimation(mCamera, target));
		updateHelpPrompts(); // update help prompts immediately
	} 
	else
	{		
		// instant
		setAnimation(new LambdaAnimation([this, target](float)
		{
			this->mCamera.translation() = -target;
		}, 1));

		updateHelpPrompts();
	}
}

void ViewController::onFileChanged(FileData* file, FileChangeType change)
{
	std::string key = file->getFullPath();
	auto sourceSystem = file->getSourceFileData()->getSystem();

	auto it = mGameListViews.find(sourceSystem);
	if (it != mGameListViews.cend())
		it->second->onFileChanged(file, change);
	else
	{
		// System is in a group ?
		for (auto gameListView : mGameListViews)
		{
			if (gameListView.first->isGroupSystem() && gameListView.first->getRootFolder()->FindByPath(key))
			{
				gameListView.second->onFileChanged(file, change);
				break;
			}
		}
	}

	for (auto collection : CollectionSystemManager::get()->getAutoCollectionSystems())
	{		
		auto cit = mGameListViews.find(collection.second.system);
		if (cit != mGameListViews.cend() && collection.second.system->getRootFolder()->FindByPath(key))
			cit->second->onFileChanged(file, change);
	}

	for (auto collection : CollectionSystemManager::get()->getCustomCollectionSystems())
	{
		auto cit = mGameListViews.find(collection.second.system);
		if (cit != mGameListViews.cend() && collection.second.system->getRootFolder()->FindByPath(key))
			cit->second->onFileChanged(file, change);
	}
}

bool ViewController::doLaunchGame(FileData* game, LaunchGameOptions options)
{
	if (mCurrentView) mCurrentView->onHide();

	// Silence TTS when launching a game
	TextToSpeech::getInstance()->say(" ");

	if (game->launchGame(mWindow, options))
		if (game->getSourceFileData()->getSystemName() == "windows_installers")
			return true;

	return false;
}

bool ViewController::checkLaunchOptions(FileData* game, LaunchGameOptions options, Vector3f center)
{
#ifdef _RPI_
	if (Settings::getInstance()->getBool("VideoOmxPlayer") && mCurrentView)
		mCurrentView->onHide();
#endif

	if (!game->isExtensionCompatible())
	{
		auto gui = new GuiMsgBox(mWindow, _("WARNING: THE EMULATOR/CORE CURRENTLY SET DOES NOT SUPPORT THIS GAME'S FILE FORMAT.\nDO YOU WANT TO LAUNCH IT ANYWAY?"),
			_("YES"), [this, game, options, center] { launch(game, options, center, false); },
			_("NO"), nullptr, ICON_ERROR);

		mWindow->pushGui(gui);
		return false;
	}

	if (Settings::getInstance()->getBool("CheckBiosesAtLaunch") && ApiSystem::getInstance()->isScriptingSupported(ApiSystem::ScriptId::BIOSINFORMATION))
	{
		auto bios = ApiSystem::getInstance()->getBiosInformations(game->getSourceFileData()->getSystem()->getName());
		if (bios.size() != 0)
		{
			auto systemName = game->getSystem()->getName();
			auto it = std::find_if(bios.cbegin(), bios.cend(), [&systemName](const BiosSystem& x) { return x.name == systemName; });
			if (it != bios.cend() && it->bios.size() > 0)
			{
				bool hasMissing = std::find_if(it->bios.cbegin(), it->bios.cend(), [&systemName](const BiosFile& x) { return x.status == "MISSING"; }) != it->bios.cend();
				if (hasMissing)
				{
					auto gui = new GuiMsgBox(mWindow, _("WARNING: THE SYSTEM HAS MISSING BIOS FILE(S) AND THE GAME MAY NOT WORK CORRECTLY.\nDO YOU WANT TO LAUNCH IT ANYWAY?"),
						_("YES"), [this, game, options, center] { launch(game, options, center, false); },
						_("NO"), nullptr, ICON_ERROR);

					mWindow->pushGui(gui);
					return false;
				}
			}
		}
	}

	return true;
}

void ViewController::launch(FileData* game, LaunchGameOptions options, Vector3f center, bool allowCheckLaunchOptions)
{
	if (allowCheckLaunchOptions && !checkLaunchOptions(game, options, center))
		return;

	if(game->getType() != GAME)
	{
		LOG(LogError) << "tried to launch something that isn't a game";
		return;
	}

	if (game->getSourceFileData()->getSystem()->hasPlatformId(PlatformIds::IMAGEVIEWER))
	{
		auto ext = Utils::String::toLower(Utils::FileSystem::getExtension(game->getPath()));

		if (Utils::FileSystem::isVideo(game->getPath()))
			GuiVideoViewer::playVideo(mWindow, game->getPath());
		else if (ext == ".pdf")
			GuiImageViewer::showPdf(mWindow, game->getPath());
		else if (ext == ".cbz")
			GuiImageViewer::showCbz(mWindow, game->getPath());
		else if (!Utils::FileSystem::isAudio(game->getPath()))
		{
			auto gameImage = game->getImagePath();
			if (Utils::FileSystem::exists(gameImage))
			{
				auto imgViewer = new GuiImageViewer(mWindow);

				for (auto sibling : game->getParent()->getChildrenListToDisplay())
					imgViewer->add(sibling->getImagePath());

				imgViewer->setCursor(gameImage);
				mWindow->pushGui(imgViewer);
			}
		}
		return;
	}

	if (!SystemConf::getInstance()->getBool("global.netplay") || ApiSystem::getInstance()->getIpAddress() == "NOT CONNECTED" || !game->isNetplaySupported())
		options.netPlayMode = DISABLED;
	else if (options.netPlayMode == DISABLED && SystemConf::getInstance()->getBool("global.netplay_public_announce"))
		options.netPlayMode = SERVER;
	
	Transform4x4f origCamera = mCamera;
	origCamera.translation() = -mCurrentView->getPosition();

	center += mCurrentView->getPosition();
	stopAnimation(1); // make sure the fade in isn't still playing
	mWindow->stopNotificationPopups(); // make sure we disable any existing info popup
	mLockInput = true;

	GuiComponent::isLaunchTransitionRunning = true;
		
	if (!Settings::getInstance()->getBool("HideWindow"))
		mWindow->setCustomSplashScreen(game->getImagePath(), game->getName(), game);

	std::string transition_style = Settings::GameTransitionStyle();
	if (transition_style.empty() || transition_style == "auto")
		transition_style = Settings::TransitionStyle();
	
	if (Settings::PowerSaverMode() == "instant")
		transition_style = "instant";

	if(transition_style == "auto")
		transition_style = "slide";

	if (Settings::PowerSaverMode() == "instant")
		transition_style = "instant";

	if (transition_style == "fade & slide")
		transition_style = "slide";

	// Workaround, the grid scale has problems when sliding giving bad effects
	//if (transition_style == "slide" && mCurrentView->isKindOf<GridGameListView>())
		//transition_style = "fade";

	if (transition_style == "fade" || transition_style == "fast fade")
	{
		int fadeDuration = (transition_style == "fast fade") ? 400 : 800; // Halve the duration for fast fade
		// fade out, launch game, fade back in
		auto fadeFunc = [this](float t) { mFadeOpacity = Math::lerp(0.0f, 1.0f, t); };

		setAnimation(new LambdaAnimation(fadeFunc, fadeDuration), 0, [this, game, fadeFunc, options]
		{
			if (doLaunchGame(game, options))
			{
				GuiComponent::isLaunchTransitionRunning = false;

				Window* w = mWindow;
				mWindow->postToUiThread([w]() { reloadAllGames(w, false); });
			}
			else
			{
				setAnimation(new LambdaAnimation(fadeFunc, 800), 0, [this] { GuiComponent::isLaunchTransitionRunning = false; mLockInput = false; mWindow->closeSplashScreen(); }, true, 3);
				this->onFileChanged(game, FILE_METADATA_CHANGED);
			}
		});
	} 
	else if (transition_style == "slide" || transition_style == "fast slide")
	{
		int slideDuration = (transition_style == "fast slide") ? 750 : 1500; // Halve the duration for fast slide
		// move camera to zoom in on center + fade out, launch game, come back in
		setAnimation(new LaunchAnimation(mCamera, mFadeOpacity, center, slideDuration), 0, [this, origCamera, center, game, options]
		{			
			if (doLaunchGame(game, options))
			{
				GuiComponent::isLaunchTransitionRunning = false;

				Window* w = mWindow;
				mWindow->postToUiThread([w]() { reloadAllGames(w, false); });
			}
			else
			{
				mCamera = origCamera;
				setAnimation(new LaunchAnimation(mCamera, mFadeOpacity, center, 600), 0, [this] { GuiComponent::isLaunchTransitionRunning = false; mLockInput = false; mWindow->closeSplashScreen(); }, true, 3);
				this->onFileChanged(game, FILE_METADATA_CHANGED);
			}
		});
	} 
	else // instant
	{ 		
		setAnimation(new LaunchAnimation(mCamera, mFadeOpacity, center, 10), 0, [this, origCamera, center, game, options]
		{			
			if (doLaunchGame(game, options))
			{
				GuiComponent::isLaunchTransitionRunning = false;

				Window* w = mWindow;
				mWindow->postToUiThread([w]() { reloadAllGames(w, false); });
			}
			else
			{
				mCamera = origCamera;
				setAnimation(new LaunchAnimation(mCamera, mFadeOpacity, center, 10), 0, [this] { GuiComponent::isLaunchTransitionRunning = false; mLockInput = false; mWindow->closeSplashScreen(); }, true, 3);
				this->onFileChanged(game, FILE_METADATA_CHANGED);
			}
		});
	}
}

void ViewController::removeGameListView(SystemData* system)
{
	//if we already made one, return that one
	auto exists = mGameListViews.find(system);
	if(exists != mGameListViews.cend())
	{
		exists->second.reset();
		mGameListViews.erase(system);
	}
}

std::shared_ptr<IGameListView> ViewController::getGameListView(SystemData* system, bool loadIfnull, const std::function<void()>& createAsPopupAndSetExitFunction)
{
	if (createAsPopupAndSetExitFunction == nullptr)
	{
		//if we already made one, return that one
		auto exists = mGameListViews.find(system);
		if (exists != mGameListViews.cend())
			return exists->second;

		if (!loadIfnull)
			return nullptr;

		system->setUIModeFilters();
		system->updateDisplayedGameCount();
	}

	//if we didn't, make it, remember it, and return it
	std::shared_ptr<IGameListView> view;

	bool themeHasGamecarouselView = system->getTheme()->hasView("gamecarousel");
	bool themeHasVideoView = system->getTheme()->hasView("video");
	bool themeHasGridView = system->getTheme()->hasView("grid");

	//decide type
	GameListViewType selectedViewType = AUTOMATIC;
	bool allowDetailedDowngrade = false;
	bool forceView = false;

	std::string viewPreference = Settings::getInstance()->getString("GamelistViewStyle");
	if (!system->getTheme()->hasView(viewPreference))
		viewPreference = "automatic";

	std::string customThemeName;
	Vector2f gridSizeOverride = Vector2f::parseString(Settings::getInstance()->getString("DefaultGridSize"));
	if (viewPreference != "automatic" && !system->getSystemViewMode().empty() && system->getTheme()->hasView(system->getSystemViewMode()) && system->getSystemViewMode() != viewPreference)
		gridSizeOverride = Vector2f(0, 0);

	Vector2f bySystemGridOverride = system->getGridSizeOverride();
	if (bySystemGridOverride != Vector2f(0, 0))
		gridSizeOverride = bySystemGridOverride;

	if (!system->getSystemViewMode().empty() && system->getTheme()->hasView(system->getSystemViewMode()))
	{
		viewPreference = system->getSystemViewMode();
		forceView = true;
	}

	if (viewPreference == "automatic")
	{
		auto defaultView = system->getTheme()->getDefaultView();
		if (!defaultView.empty() && system->getTheme()->hasView(defaultView))
			viewPreference = defaultView;
	}

	if (system->getTheme()->isCustomView(viewPreference))
	{
		auto baseClass = system->getTheme()->getCustomViewBaseType(viewPreference);
		if (!baseClass.empty()) // this is a customView
		{
			customThemeName = viewPreference;
			viewPreference = baseClass;
		}
	}

	if (viewPreference.compare("basic") == 0)
		selectedViewType = BASIC;
	else if (viewPreference.compare("detailed") == 0)
	{
		auto defaultView = system->getTheme()->getDefaultView();
		if (!defaultView.empty() && system->getTheme()->hasView(defaultView) && defaultView != "detailed")
			allowDetailedDowngrade = true;

		selectedViewType = DETAILED;
	}
	else if (themeHasGridView && viewPreference.compare("grid") == 0)
		selectedViewType = GRID;
	else if (themeHasVideoView && viewPreference.compare("video") == 0)
		selectedViewType = VIDEO;
	else if (themeHasGamecarouselView && viewPreference.compare("gamecarousel") == 0)
		selectedViewType = GAMECAROUSEL;

	if (!forceView && (selectedViewType == AUTOMATIC || allowDetailedDowngrade))
	{
		selectedViewType = BASIC;

		if (system->getTheme()->getDefaultView() != "basic")
		{
			std::vector<FileData*> files = system->getRootFolder()->getFilesRecursive(GAME | FOLDER);
			for (auto it = files.cbegin(); it != files.cend(); it++)
			{
				if (!allowDetailedDowngrade && themeHasVideoView && !(*it)->getVideoPath().empty())
				{
					selectedViewType = VIDEO;
					break;
				}
				else if (!(*it)->getThumbnailPath().empty())
				{
					selectedViewType = DETAILED;

					if (!themeHasVideoView)
						break;
				}
			}
		}
	}

	// Create the view
	switch (selectedViewType)
	{
		case VIDEO:
			view = std::shared_ptr<IGameListView>(new VideoGameListView(mWindow, system->getRootFolder()));
			break;
		case DETAILED:
			view = std::shared_ptr<IGameListView>(new DetailedGameListView(mWindow, system->getRootFolder()));
			break;
		case GRID:
			view = std::shared_ptr<IGameListView>(new GridGameListView(mWindow, system->getRootFolder(), system->getTheme(), customThemeName, gridSizeOverride));
			break;
		case GAMECAROUSEL:
			view = std::shared_ptr<IGameListView>(new CarouselGameListView(mWindow, system->getRootFolder()));
			break;
		default:
			view = std::shared_ptr<IGameListView>(new BasicGameListView(mWindow, system->getRootFolder()));
			break;
	}

	if (selectedViewType != GRID)
	{
		// GridGameListView theme needs to be loaded before populating.

		if (!customThemeName.empty())
			view->setThemeName(customThemeName);

		view->setTheme(system->getTheme());
	}

	ISimpleGameListView* simpleListView = dynamic_cast<ISimpleGameListView*>(view.get());

	if (createAsPopupAndSetExitFunction != nullptr && simpleListView != nullptr)
	{
		if (mCurrentView)
		{
			mCurrentView->onHide();
			view->setPosition(mCurrentView->getPosition());
		}

		simpleListView->setPopupContext(view, mCurrentView, system->getIndex(true)->getTextFilter(), createAsPopupAndSetExitFunction);

		mCurrentView = view;
		mCurrentView->onShow();
		mCurrentView->topWindow(true);
	}
	else
	{
		int id = getSystemId(system);
		view->setPosition(id * (float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight() * 2);

		addChild(view.get());
		mGameListViews[system] = view;
	}

	return view;
}

std::shared_ptr<SystemView> ViewController::getSystemListView()
{
	//if we already made one, return that one
	if(mSystemListView)
		return mSystemListView;

	mSystemListView = std::shared_ptr<SystemView>(new SystemView(mWindow));
	addChild(mSystemListView.get());
	mSystemListView->setPosition(0, (float)Renderer::getScreenHeight());
	return mSystemListView;
}

void ViewController::changeVolume(int increment)
{
	int newVal = VolumeControl::getInstance()->getVolume() + increment;
	if (newVal > 100)
		newVal = 100;
	if (newVal < 0)
		newVal = 0;

	VolumeControl::getInstance()->setVolume(newVal);
#if !WIN32
	SystemConf::getInstance()->set("audio.volume", std::to_string(VolumeControl::getInstance()->getVolume()));
#endif
}

bool ViewController::input(InputConfig* config, Input input)
{
	if (mLockInput)
		return true;

	// If we receive a button pressure for a non configured joystick, suggest the joystick configuration
	if (!config->isConfigured() && (config->getDeviceId() == DEVICE_KEYBOARD || input.type == TYPE_BUTTON || input.type == TYPE_HAT))
	{
		mWindow->pushGui(new GuiDetectDevice(mWindow, false, NULL));
		return true;
	}

	if (config->getDeviceId() == DEVICE_KEYBOARD && input.value && input.id == SDLK_F5)
	{
		mWindow->render();
		
		FileSorts::reset();
		ResourceManager::getInstance()->unloadAll();
		ResourceManager::getInstance()->reloadAll();

		ViewController::get()->reloadAll(mWindow);
#if WIN32
		EsLocale::reset();
#endif
		mWindow->closeSplashScreen();
		return true;
	}

	if (config->getDeviceId() == DEVICE_KEYBOARD && input.value && input.id == SDLK_F3)
	  {
	    Settings::getInstance()->setBool("TTS", TextToSpeech::getInstance()->toogle());
	    Settings::getInstance()->saveFile();
	  }

	// open menu
	if(config->isMappedTo("start", input) && input.value != 0)
	{
		// open menu
		mWindow->pushGui(new GuiMenu(mWindow));
		return true;
	}

	// Next song
	if (((mState.viewing != GAME_LIST && config->isMappedTo("l3", input)) || config->isMappedTo("r3", input)) && input.value != 0)
	{		
		AudioManager::getInstance()->playRandomMusic(false);
		return true;
	}

	if (config->isMappedTo("joystick2up", input) && input.value != 0)
	{
		changeVolume(5);
		return true;
	}

	if (config->isMappedTo("joystick2up", input, true) && input.value != 0)
	{
		changeVolume(-5);
		return true;
	}

//	if(UIModeController::getInstance()->listen(config, input))  // check if UI mode has changed due to passphrase completion
//		return true;

	if(mCurrentView)
		return mCurrentView->input(config, input);

	return false;
}

void ViewController::update(int deltaTime)
{
	mSize = Vector2f(Renderer::getScreenWidth(), Renderer::getScreenHeight());

	if (mCurrentView)
		mCurrentView->update(deltaTime);

	updateSelf(deltaTime);

	if (mDeferPlayViewTransitionTo != nullptr)
	{
		if (mCurrentView)
			mCurrentView->onHide();

		mCurrentView = mDeferPlayViewTransitionTo;
		mDeferPlayViewTransitionTo = nullptr;

		playViewTransition(false); 
	}
}

void ViewController::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = mCamera * parentTrans;
	Transform4x4f transInverse;
	transInverse.invert(trans);

	// camera position, position + size
	Vector3f viewStart = transInverse.translation();
	Vector3f viewEnd = transInverse * Vector3f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight(), 0);

	if (!isAnimationPlaying(0) && mCurrentView != nullptr)
	{
		mCurrentView->render(trans);
	}
	else
	{
		// draw systemview
		getSystemListView()->render(trans);

		// draw gamelists
		for (auto it = mGameListViews.cbegin(); it != mGameListViews.cend(); it++)
		{
			// clipping
			Vector3f guiStart = it->second->getPosition();
			Vector3f guiEnd = it->second->getPosition() + Vector3f(it->second->getSize().x(), it->second->getSize().y(), 0);

			//	if (guiEnd.x() >= viewStart.x() && guiEnd.y() >= viewStart.y() && guiStart.x() <= viewEnd.x() && guiStart.y() <= viewEnd.y())
			if (guiEnd.x() > viewStart.x() && guiEnd.y() > viewStart.y() && guiStart.x() < viewEnd.x() && guiStart.y() < viewEnd.y())
				it->second->render(trans);
		}
	}

	if(mWindow->peekGui() == this)
		mWindow->renderHelpPromptsEarly(parentTrans);

	// fade out
	if (mFadeOpacity)
	{
		if (!Settings::getInstance()->getBool("HideWindow") && mLockInput) // We're launching a game
			mWindow->renderSplashScreen(mFadeOpacity, false);
		else
		{
			unsigned int fadeColor = 0x00000000 | (unsigned char)(mFadeOpacity * 255);
			Renderer::setMatrix(parentTrans);
			Renderer::drawRect(0.0f, 0.0f, Renderer::getScreenWidth(), Renderer::getScreenHeight(), fadeColor, fadeColor);
		}
	}
}

void ViewController::preload()
{
	bool preloadUI = Settings::getInstance()->getBool("PreloadUI");
	if (!preloadUI)
		return;

	mWindow->renderSplashScreen(_("Preloading UI"), 0);
	getSystemListView();

	int i = 1;
	int max = SystemData::sSystemVector.size() + 1;
	bool splash = preloadUI && Settings::getInstance()->getBool("SplashScreen") && Settings::getInstance()->getBool("SplashScreenProgress");

	for(auto it = SystemData::sSystemVector.cbegin(); it != SystemData::sSystemVector.cend(); it++)
	{		
		if ((*it)->isGroupChildSystem() || !(*it)->isVisible())
		{
			i++;
			continue;
		}

		if (splash)
		{
			i++;

			if ((i % 4) == 0)
				mWindow->renderSplashScreen(_("Preloading UI"), (float)i / (float)max);
		}

		(*it)->resetFilters();
		getGameListView(*it);
	}
}

void ViewController::reloadSystemListViewTheme(SystemData* system)
{
	if (mSystemListView == nullptr)
		return;

	mSystemListView->reloadTheme(system);
}

void ViewController::reloadGameListView(IGameListView* view)
{
	if (view == nullptr)
		return;

	Vector3f position = view->getPosition();

	bool isCurrent = mCurrentView != nullptr && mCurrentView.get() == view;
	if (isCurrent)
	{
		mCurrentView->onHide();
		mCurrentView = nullptr;
	}

	for(auto it = mGameListViews.cbegin(); it != mGameListViews.cend(); it++)
	{
		if (it->second.get() != view)
			continue;

		SystemData* system = it->first;
			
		std::string cursorPath;
		FileData* cursor = view->getCursor();
		if (cursor != nullptr && !cursor->isPlaceHolder())
			cursorPath = cursor->getPath();

		mGameListViews.erase(it);

		system->setUIModeFilters();
		system->updateDisplayedGameCount();

		std::shared_ptr<IGameListView> newView = getGameListView(system);		
		if (isCurrent)
		{		
			mCurrentView = newView;
			mCurrentView->setPosition(position);

			ISimpleGameListView* view = dynamic_cast<ISimpleGameListView*>(newView.get());
			if (view != nullptr)
			{
				if (!cursorPath.empty())
				{
					for (auto file : system->getRootFolder()->getFilesRecursive(GAME, true))
					{
						if (file->getPath() == cursorPath)
						{
							newView->setCursor(file);
							break;
						}
					}
				}
			}
		}		

		break;		
	}
	
	// Redisplay the current view
	if (mCurrentView)
	{
		if (isCurrent)
			mCurrentView->onShow();

		updateHelpPrompts();
	}
}

SystemData* ViewController::getSelectedSystem()
{
	if (mState.viewing == SYSTEM_SELECT)
	{
		int idx = mSystemListView->getCursorIndex();
		if (idx >= 0 && idx < mSystemListView->getObjects().size())
			return mSystemListView->getObjects()[mSystemListView->getCursorIndex()];
	}

	return mState.getSystem();
}

ViewController::ViewMode ViewController::getViewMode()
{
	return mState.viewing;
}

void ViewController::reloadAll(Window* window, bool reloadTheme)
{
	if (reloadTheme)
		Renderer::resetCache();

	Utils::FileSystem::FileSystemCacheActivator fsc;

	if (mCurrentView != nullptr)
	{
		ISimpleGameListView* simpleView = dynamic_cast<ISimpleGameListView*>(mCurrentView.get());
		if (simpleView != nullptr)
			simpleView->closePopupContext();

		mCurrentView->onHide();
	}
	
	ThemeData::setDefaultTheme(nullptr);

	SystemData* system = nullptr;

	if (mState.viewing == SYSTEM_SELECT)
		system = getSelectedSystem();

	int gameListCount = 0;
	// clear all gamelistviews
	std::map<SystemData*, FileData*> cursorMap;
	for (auto it = mGameListViews.cbegin(); it != mGameListViews.cend(); it++)
	{
		gameListCount++;
		cursorMap[it->first] = it->second->getCursor();
	}

	mGameListViews.clear();
	
	// If preloaded is disabled
	for (auto it = SystemData::sSystemVector.cbegin(); it != SystemData::sSystemVector.cend(); it++)
		if (cursorMap.find((*it)) == cursorMap.end())
			cursorMap[(*it)] = NULL;
	
	if (reloadTheme && cursorMap.size() > 0)
	{
		mCurrentView.reset();
		mSystemListView.reset();
		TextureResource::cleanupTextureResourceCache();

		int processedSystem = 0;
		int systemCount = cursorMap.size();

		Utils::ThreadPool pool;

		for (auto it = cursorMap.cbegin(); it != cursorMap.cend(); it++)
		{
			SystemData* pooledSystem = it->first;

			pool.queueWorkItem([pooledSystem, &processedSystem]
			{ 
				pooledSystem->loadTheme();
				pooledSystem->resetFilters();
				processedSystem++;
			});
		}

		if (window)
		{
			pool.wait([window, &processedSystem, systemCount]
			{
				int px = processedSystem;
				if (px >= 0 && px < systemCount)
					window->renderSplashScreen(_("Loading theme"), (float)px / (float)systemCount);
			}, 5);
		}
		else
			pool.wait();
	}

	bool preloadUI = Settings::getInstance()->getBool("PreloadUI");

	if (gameListCount > 0)
	{
		int lastTime = SDL_GetTicks() - 50;

		if (window)
			window->renderSplashScreen(_("Loading gamelists"), 0.0f);

		float idx = 0;
		// load themes, create gamelistviews and reset filters
		for (auto it = cursorMap.cbegin(); it != cursorMap.cend(); it++)
		{
			if (it->second == nullptr)
				continue;

			if (preloadUI)
				getGameListView(it->first)->setCursor(it->second);
			else if (mState.viewing == GAME_LIST)
			{
				if (mState.getSystem() == it->first)
					getGameListView(mState.getSystem())->setCursor(it->second);
			}
			
			idx++;

			int time = SDL_GetTicks();
			if (window && time - lastTime >= 20)
			{
				lastTime = time;
				window->renderSplashScreen(_("Loading gamelists"), (float)idx / (float)gameListCount);
			}
		}
	}

	if (window != nullptr)
		window->renderSplashScreen(_("Loading..."));

	if (SystemData::sSystemVector.size() > 0)
	{
		for (auto sys : SystemData::sSystemVector)
		{
			auto theme = sys->getTheme();
			if (theme != nullptr)
			{
				ViewController::get()->onThemeChanged(theme);
				break;
			}
		}	
	}

	// Rebuild SystemListView
	mSystemListView.reset();
	getSystemListView();

	// update mCurrentView since the pointers changed
	if(mState.viewing == GAME_LIST)
	{
		mCurrentView = getGameListView(mState.getSystem());
	}
	else if(mState.viewing == SYSTEM_SELECT && system != nullptr)
	{
		goToSystemView(system);
		mSystemListView->goToSystem(system, false);
		mCurrentView = mSystemListView;
	}
	else
		goToSystemView(SystemData::getFirstVisibleSystem());

	if (mCurrentView != nullptr)
		mCurrentView->onShow();

	updateHelpPrompts();
}

std::vector<HelpPrompt> ViewController::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	if(!mCurrentView)
		return prompts;

	prompts = mCurrentView->getHelpPrompts();

	if (!UIModeController::getInstance()->isUIModeKid())
		prompts.push_back(HelpPrompt("start", _("MENU"), [&] { mWindow->pushGui(new GuiMenu(mWindow)); }));

	return prompts;
}

HelpStyle ViewController::getHelpStyle()
{
	if(!mCurrentView)
		return GuiComponent::getHelpStyle();

	return mCurrentView->getHelpStyle();
}

void ViewController::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	Font::OnThemeChanged();
	ThemeData::setDefaultTheme(theme.get());
	mWindow->onThemeChanged(theme);
}


void ViewController::onShow()
{
	if (mCurrentView)
		mCurrentView->onShow();
}

void ViewController::onScreenSaverActivate()
{
	GuiComponent::onScreenSaverActivate();

	if (mCurrentView)
		mCurrentView->onScreenSaverActivate();
}

void ViewController::onScreenSaverDeactivate()
{
	GuiComponent::onScreenSaverDeactivate();

	if (mCurrentView)
		mCurrentView->onScreenSaverDeactivate();
}

void ViewController::reloadAllGames(Window* window, bool deleteCurrentGui, bool doCallExternalTriggers)
{
	if (sInstance == nullptr)
		return;

	Utils::FileSystem::FileSystemCacheActivator fsc;

	auto viewMode = ViewController::get()->getViewMode();
	auto systemName = ViewController::get()->getSelectedSystem()->getName();

	window->closeSplashScreen();
	window->renderSplashScreen(_("Loading..."));

	if (!deleteCurrentGui)
	{
		GuiComponent* topGui = window->peekGui();
		if (topGui != nullptr)
			window->removeGui(topGui);
	}

	GuiComponent *gui;
	while ((gui = window->peekGui()) != NULL)
	{
		window->removeGui(gui);

		if (gui != sInstance)
			delete gui;
	}

	ViewController::deinit();

	// call external triggers
	if (doCallExternalTriggers && ApiSystem::getInstance()->isScriptingSupported(ApiSystem::BATOCERAPREGAMELISTSHOOK))
		ApiSystem::getInstance()->callBatoceraPreGameListsHook();

	ViewController::init(window);
	
	CollectionSystemManager::init(window);		
	SystemData::loadConfig(window);
	
	ViewController::get()->goToSystemView(systemName, true, viewMode);	
	ViewController::get()->reloadAll(nullptr, false); // Avoid reloading themes a second time

	window->closeSplashScreen();
	window->pushGui(ViewController::get());
}

void ViewController::setActiveView(std::shared_ptr<GuiComponent> view)
{
	if (mCurrentView != nullptr)
	{
		mCurrentView->topWindow(false);
		mCurrentView->onHide();
	}

	mCurrentView = view;
	mCurrentView->onShow();
	mCurrentView->topWindow(true);
}


bool ViewController::hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult)
{
	bool ret = false;

	Transform4x4f trans = mCamera * parentTransform;

//  Skip ViewController rect

	for (int i = 0; i < getChildCount(); i++)
	{
		auto child = getChild(i);
		if (mCurrentView == nullptr || child != mCurrentView.get())
			ret |= getChild(i)->hitTest(x, y, trans, pResult);
	}

	if (mCurrentView != nullptr)
		mCurrentView->hitTest(x, y, trans, pResult);

	return ret;
}
