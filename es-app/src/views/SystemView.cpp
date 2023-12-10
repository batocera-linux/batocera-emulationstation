#include "views/SystemView.h"

#include "animations/LambdaAnimation.h"
#include "guis/GuiMsgBox.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "Log.h"
#include "Scripting.h"
#include "Settings.h"
#include "SystemData.h"
#include "Window.h"
#include "LocaleES.h"
#include "ApiSystem.h"
#include "SystemConf.h"
#include "guis/GuiMenu.h"
#include "AudioManager.h"
#include "components/VideoComponent.h"
#include "components/VideoVlcComponent.h"
#include "guis/GuiNetPlay.h"
#include "SystemRandomPlaylist.h"
#include "playlists/M3uPlaylist.h"
#include "CollectionSystemManager.h"
#include "resources/TextureDataManager.h"
#include "guis/GuiTextEditPopup.h"
#include "guis/GuiTextEditPopupKeyboard.h"
#include "TextToSpeech.h"
#include "BindingManager.h"
#include "guis/GuiRetroAchievements.h"
#include "components/CarouselComponent.h"

// buffer values for scrolling velocity (left, stopped, right)
const int logoBuffersLeft[] = { -5, -2, -1 };
const int logoBuffersRight[] = { 1, 2, 5 };

SystemView::SystemView(Window* window) : IList<SystemViewData, SystemData*>(window, LIST_SCROLL_STYLE_SLOW, LIST_ALWAYS_LOOP),
										 mViewNeedsReload(true),
										 mSystemInfo(window, _("SYSTEM INFO"), Font::get(FONT_SIZE_SMALL), 0x33333300, ALIGN_CENTER), mYButton("y")
{
	mCamOffset = 0;
	mExtrasCamOffset = 0;
	mExtrasFadeOpacity = 0.0f;
	mExtrasFadeMove = 0.0f;	
	mScreensaverActive = false;
	mDisable = false;		
	mLastCursor = 0;
	mExtrasFadeOldCursor = -1;

	mLockCamOffsetChanges = false;
	mLockExtraChanges = false;
	mPressedCursor = -1;
	mPressedPoint = Vector2i(-1, -1);

	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	populate();
}

SystemView::~SystemView()
{
	for(auto sb : mStaticBackgrounds) delete sb;
	mStaticBackgrounds.clear();

	clearEntries();
}

void SystemView::clearEntries()
{
	for (int i = 0; i < mEntries.size(); i++)
	{
		setExtraRequired(i, false);

		for (auto extra : mEntries[i].data.backgroundExtras)
			delete extra;

		mEntries[i].data.backgroundExtras.clear();
	}

	mEntries.clear();
}

void SystemView::reloadTheme(SystemData* system)
{
	const std::shared_ptr<ThemeData>& theme = system->getTheme();
	getViewElements(theme);

	for (auto& e : mEntries)
	{
		if (e.object != system)
			continue;
		
		if (e.data.logo != nullptr)
		{
			if (e.data.logo->isKindOf<TextComponent>())
				e.data.logo->applyTheme(theme, "system", "logoText", ThemeFlags::FONT_PATH | ThemeFlags::FONT_SIZE | ThemeFlags::COLOR | ThemeFlags::FORCE_UPPERCASE | ThemeFlags::LINE_SPACING | ThemeFlags::TEXT);
			else if (e.data.logo->isKindOf<ImageComponent>())
				e.data.logo->applyTheme(theme, "system", "logo", ThemeFlags::COLOR | ThemeFlags::ALIGNMENT | ThemeFlags::VISIBLE);
		}

		loadExtras(system, e);
	}
}

void SystemView::loadExtras(SystemData* system, IList<SystemViewData, SystemData*>::Entry& e)
{
	// delete any existing extras
	for (auto extra : e.data.backgroundExtras)
		delete extra;

	e.data.backgroundExtras.clear();

	// make background extras
	e.data.backgroundExtras = ThemeData::makeExtras(system->getTheme(), "system", mWindow);

	for (auto extra : e.data.backgroundExtras)
	{
		if (extra->isKindOf<VideoComponent>())
		{
			auto elem = system->getTheme()->getElement("system", extra->getTag(), "video");
			if (elem != nullptr && elem->has("path") && Utils::String::startsWith(elem->get<std::string>("path"), "{random"))
				((VideoComponent*)extra)->setPlaylist(std::make_shared<SystemRandomPlaylist>(system, SystemRandomPlaylist::VIDEO));
			else if (elem != nullptr && elem->has("path") && Utils::String::toLower(Utils::FileSystem::getExtension(elem->get<std::string>("path"))) == ".m3u")
				((VideoComponent*)extra)->setPlaylist(std::make_shared<M3uPlaylist>(elem->get<std::string>("path")));
		}
		else if (extra->isKindOf<ImageComponent>())
		{
			auto elem = system->getTheme()->getElement("system", extra->getTag(), "image");
			if (elem != nullptr && elem->has("path") && Utils::String::startsWith(elem->get<std::string>("path"), "{random"))
			{
				std::string src = elem->get<std::string>("path");

				SystemRandomPlaylist::PlaylistType type = SystemRandomPlaylist::IMAGE;

				if (src == "{random:thumbnail}")
					type = SystemRandomPlaylist::THUMBNAIL;
				else if (src == "{random:marquee}")
					type = SystemRandomPlaylist::MARQUEE;
				else if (src == "{random:fanart}")
					type = SystemRandomPlaylist::FANART;
				else if (src == "{random:titleshot}")
					type = SystemRandomPlaylist::TITLESHOT;

				((ImageComponent*)extra)->setPlaylist(std::make_shared<SystemRandomPlaylist>(system, type));
			}
		/*	else if (elem != nullptr && elem->has("path") && Utils::String::toLower(Utils::FileSystem::getExtension(elem->get<std::string>("path"))) == ".m3u")
			{
				((ImageComponent*)extra)->setAllowFading(false);
				((ImageComponent*)extra)->setPlaylist(std::make_shared<M3uPlaylist>(elem->get<std::string>("path")));
			}*/
		}
	}

	// sort the extras by z-index
	std::stable_sort(e.data.backgroundExtras.begin(), e.data.backgroundExtras.end(), [](GuiComponent* a, GuiComponent* b) {
		return b->getZIndex() > a->getZIndex();
	});

	SystemRandomPlaylist::resetCache();
}

void SystemView::ensureLogo(IList<SystemViewData, SystemData*>::Entry& entry)
{
	if (entry.data.logo != nullptr)
		return;

	auto system = entry.object;
	const std::shared_ptr<ThemeData>& theme = system->getTheme();

	// itemTemplate
	const ThemeData::ThemeElement* carouselElem = theme->getElement("system", "systemcarousel", "carousel");
	if (carouselElem)
	{
		auto itemTemplate = std::find_if(carouselElem->children.cbegin(), carouselElem->children.cend(), [](const std::pair<std::string, ThemeData::ThemeElement> ss) { return ss.first == "itemTemplate"; });
		if (itemTemplate != carouselElem->children.cend())
		{
			CarouselItemTemplate* templ = new CarouselItemTemplate(entry.name, mWindow);
			templ->setScaleOrigin(0.0f);
			templ->setSize(mCarousel.logoSize * mCarousel.logoScale);
			templ->loadTemplatedChildren(&itemTemplate->second);

			entry.data.logo = std::shared_ptr<GuiComponent>(templ);
		}
	}

	// Logo is <image name="logo"> ?
	if (!entry.data.logo)
	{
		const ThemeData::ThemeElement* logoElem = theme->getElement("system", "logo", "image");
		if (logoElem && logoElem->has("path") && theme->getSystemThemeFolder() != "default")
		{
			std::string path = logoElem->get<std::string>("path");
			if (path.empty())
				path = logoElem->has("default") ? logoElem->get<std::string>("default") : "";

			if (!path.empty())
			{
				// Remove dynamic flags for png & jpg files : themes can contain oversized images that can't be unloaded by the TextureResource manager
				auto logo = std::make_shared<ImageComponent>(mWindow, false, true); // Utils::String::toLower(Utils::FileSystem::getExtension(path)) != ".svg");
				logo->setMaxSize(mCarousel.logoSize * mCarousel.logoScale);
				logo->applyTheme(theme, "system", "logo", ThemeFlags::COLOR | ThemeFlags::ALIGNMENT | ThemeFlags::VISIBLE); //  ThemeFlags::PATH | 
																															// Process here to be enable to set max picture size
				auto elem = theme->getElement("system", "logo", "image");
				if (elem && elem->has("path"))
				{
					auto logoPath = elem->get<std::string>("path");
					if (!logoPath.empty())
						logo->setImage(logoPath, (elem->has("tile") && elem->get<bool>("tile")), MaxSizeInfo(mCarousel.logoSize * mCarousel.logoScale), false);
				}

				// If logosize is defined for full width/height, don't rotate by target size
				// ex : <logoSize>1 .05< / logoSize>
				if (mCarousel.size.x() != mCarousel.logoSize.x() & mCarousel.size.y() != mCarousel.logoSize.y())
					logo->setRotateByTargetSize(true);

				entry.data.logo = logo;
			}
		}
	}

	// Logo is <text name="logo"> ?
	if (!entry.data.logo)
	{
		// no logo in theme; use text
		TextComponent* text = new TextComponent(mWindow,
			system->getFullName(),
			Renderer::isSmallScreen() ? Font::get(FONT_SIZE_MEDIUM) : Font::get(FONT_SIZE_LARGE),
			0x000000FF,
			ALIGN_CENTER);

		text->setScaleOrigin(0.0f);
		text->setSize(mCarousel.logoSize * mCarousel.logoScale);
		text->applyTheme(system->getTheme(), "system", "logoText", ThemeFlags::FONT_PATH | ThemeFlags::FONT_SIZE | ThemeFlags::COLOR | ThemeFlags::FORCE_UPPERCASE | ThemeFlags::LINE_SPACING | ThemeFlags::TEXT);
		// system->getTheme()
		entry.data.logo = std::shared_ptr<GuiComponent>(text);

		if (mCarousel.type == VERTICAL || mCarousel.type == VERTICAL_WHEEL)
		{
			text->setHorizontalAlignment(mCarousel.logoAlignment);
			text->setVerticalAlignment(ALIGN_CENTER);
		}
		else {
			text->setHorizontalAlignment(ALIGN_CENTER);
			text->setVerticalAlignment(mCarousel.logoAlignment);
		}
	}

	entry.data.logo->updateBindings(system);

	if (mCarousel.type == VERTICAL || mCarousel.type == VERTICAL_WHEEL)
	{
		if (mCarousel.logoAlignment == ALIGN_LEFT)
			entry.data.logo->setOrigin(0, 0.5);
		else if (mCarousel.logoAlignment == ALIGN_RIGHT)
			entry.data.logo->setOrigin(1.0, 0.5);
		else
			entry.data.logo->setOrigin(0.5, 0.5);
	}
	else 
	{
		if (mCarousel.logoAlignment == ALIGN_TOP)
			entry.data.logo->setOrigin(0.5, 0);
		else if (mCarousel.logoAlignment == ALIGN_BOTTOM)
			entry.data.logo->setOrigin(0.5, 1);
		else
			entry.data.logo->setOrigin(0.5, 0.5);
	}

	Vector2f denormalized = mCarousel.logoSize * entry.data.logo->getOrigin();
	entry.data.logo->setPosition(denormalized.x(), denormalized.y(), 0.0);

	mCarousel.anyLogoHasScaleStoryboard = 
		entry.data.logo->storyBoardExists("deactivate", "scale") || 
		entry.data.logo->storyBoardExists("activate", "scale") ||
		entry.data.logo->storyBoardExists("scroll", "scale") ||
		entry.data.logo->storyBoardExists("", "scale");

	mCarousel.anyLogoHasOpacityStoryboard =
		entry.data.logo->storyBoardExists("deactivate", "opacity") ||
		entry.data.logo->storyBoardExists("activate", "opacity") ||
		entry.data.logo->storyBoardExists("scroll", "opacity") ||
		entry.data.logo->storyBoardExists("", "opacity");

	if (!entry.data.logo->selectStoryboard("deactivate") && !entry.data.logo->selectStoryboard())
		entry.data.logo->deselectStoryboard();
}

void SystemView::populate()
{
	TextureLoader::paused = true;

	clearEntries();

	for(auto it = SystemData::sSystemVector.cbegin(); it != SystemData::sSystemVector.cend(); it++)
	{
		const std::shared_ptr<ThemeData>& theme = (*it)->getTheme();
		if (theme == nullptr)
			continue;

		if(mViewNeedsReload)
			getViewElements(theme);

		if((*it)->isVisible())
		{
			Entry e;
			e.name = (*it)->getName();
			e.object = *it;

			ensureLogo(e);
			loadExtras(*it, e);

			add(e);
		}
	}

	TextureLoader::paused = false;

	if (mEntries.size() == 0)
	{
		// Something is wrong, there is not a single system to show, check if UI mode is not full
		if (!UIModeController::getInstance()->isUIModeFull())
		{
			Settings::getInstance()->setString("UIMode", "Full");
			mWindow->pushGui(new GuiMsgBox(mWindow, "The selected UI mode has nothing to show,\n returning to UI mode: FULL", "OK", nullptr));
		}

		if (Settings::getInstance()->setString("HiddenSystems", ""))
		{
			Settings::getInstance()->saveFile();

			// refresh GUI
			populate();
			mWindow->pushGui(new GuiMsgBox(mWindow, _("ERROR: EVERY SYSTEM IS HIDDEN, RE-DISPLAYING ALL OF THEM NOW"), _("OK"), nullptr));
		}
	}
}

void SystemView::goToSystem(SystemData* system, bool animate)
{
	setCursor(system);

	if (!animate)
	{
		finishAnimation(0);
		finishAnimation(1);
		finishAnimation(2);
	}
}

static void _moveCursorInRange(int& value, int count, int sz)
{
	if (count >= sz)
		return;

	value += count;
	if (value < 0) value += sz;
	if (value >= sz) value -= sz;	
}

int SystemView::moveCursorFast(bool forward)
{
	int cursor = mCursor;

	if (SystemData::IsManufacturerSupported && Settings::getInstance()->getString("SortSystems") == "manufacturer" && mCursor >= 0 && mCursor < mEntries.size())
	{
		std::string man = mEntries[mCursor].object->getSystemMetadata().manufacturer;

		int direction = forward ? 1 : -1;

		_moveCursorInRange(cursor, direction, mEntries.size());

		while (cursor != mCursor && mEntries[cursor].object->getSystemMetadata().manufacturer == man)
			_moveCursorInRange(cursor, direction, mEntries.size());

		if (!forward && cursor != mCursor)
		{
			// Find first item
			man = mEntries[cursor].object->getSystemMetadata().manufacturer;
			while (cursor != mCursor && mEntries[cursor].object->getSystemMetadata().manufacturer == man)
				_moveCursorInRange(cursor, -1, mEntries.size());

			_moveCursorInRange(cursor, 1, mEntries.size());
		}
	}
	else if(SystemData::IsManufacturerSupported && Settings::getInstance()->getString("SortSystems") == "hardware" && mCursor >= 0 && mCursor < mEntries.size())
	{
		std::string hwt = mEntries[mCursor].object->getSystemMetadata().hardwareType;

		int direction = forward ? 1 : -1;

		_moveCursorInRange(cursor, direction, mEntries.size());

		while (cursor != mCursor && mEntries[cursor].object->getSystemMetadata().hardwareType == hwt)
			_moveCursorInRange(cursor, direction, mEntries.size());

		if (!forward && cursor != mCursor)
		{
			// Find first item
			hwt = mEntries[cursor].object->getSystemMetadata().hardwareType;
			while (cursor != mCursor && mEntries[cursor].object->getSystemMetadata().hardwareType == hwt)
				_moveCursorInRange(cursor, -1, mEntries.size());

			_moveCursorInRange(cursor, 1, mEntries.size());
		}
	}
	else
		_moveCursorInRange(cursor, forward ? 10 : -10, mEntries.size());

	return cursor;
}

void SystemView::showQuickSearch()
{
	SystemData* all = SystemData::getSystem("all");
	if (all != nullptr)
	{
		auto updateVal = [this, all](const std::string& newVal)
		{
			auto index = all->getIndex(true);

			index->resetFilters();
			index->setTextFilter(newVal);

			ViewController::get()->reloadGameListView(all);
			ViewController::get()->goToGameList(all, false);
		};

		if (Settings::getInstance()->getBool("UseOSK"))
			mWindow->pushGui(new GuiTextEditPopupKeyboard(mWindow, _("QUICK SEARCH"), "", updateVal, false));
		else
			mWindow->pushGui(new GuiTextEditPopup(mWindow, _("QUICK SEARCH"), "", updateVal, false));
	}
}

void SystemView::showNetplay()
{
	if (!SystemData::isNetplayActivated() && SystemConf::getInstance()->getBool("global.netplay"))
		return;

	if (ApiSystem::getInstance()->getIpAdress() == "NOT CONNECTED")
		mWindow->pushGui(new GuiMsgBox(mWindow, _("YOU ARE NOT CONNECTED TO A NETWORK"), _("OK"), nullptr));
	else
		mWindow->pushGui(new GuiNetPlay(mWindow));
}

bool SystemView::input(InputConfig* config, Input input)
{
	if (mYButton.isShortPressed(config, input))
	{
		showQuickSearch();
		return true;
	}

	if(input.value != 0)
	{	
		bool netPlay = SystemData::isNetplayActivated() && SystemConf::getInstance()->getBool("global.netplay");
		if (netPlay && config->isMappedTo("x", input))
		{
			showNetplay();
			return true;
		}
		/*
		if (config->isMappedTo("y", input))
		{	
			showQuickSearch();
			return true;
		}
		*/
		if(config->getDeviceId() == DEVICE_KEYBOARD && input.value && input.id == SDLK_r && SDL_GetModState() & KMOD_LCTRL && Settings::getInstance()->getBool("Debug"))
		{
			LOG(LogInfo) << " Reloading all";
			ViewController::get()->reloadAll();
			return true;
		}
		
#ifdef _ENABLE_FILEMANAGER_
		if(UIModeController::getInstance()->isUIModeFull()) {
		  if(config->getDeviceId() == DEVICE_KEYBOARD && input.value && input.id == SDLK_F1)
		    {
		      ApiSystem::getInstance()->launchFileManager(mWindow);
		      return true;
		    }
		}
#endif

		switch (mCarousel.type)
		{
		case VERTICAL:
		case VERTICAL_WHEEL:
			if (config->isMappedLike("up", input) || config->isMappedLike("l2", input))
			{
				listInput(-1);
				return true;
			}
			if (config->isMappedLike("down", input) || config->isMappedLike("r2", input))
			{
				listInput(1);
				return true;
			}
			if ((Settings::getInstance()->getBool("QuickSystemSelect") && config->isMappedLike("right", input)) || config->isMappedTo("pagedown", input))
			{
				int cursor = moveCursorFast(true);
				listInput(cursor - mCursor);				
				return true;
			}
			if ((Settings::getInstance()->getBool("QuickSystemSelect") && config->isMappedLike("left", input)) || config->isMappedTo("pageup", input))
			{
				int cursor = moveCursorFast(false);
				listInput(cursor - mCursor);
				return true;
			}

			break;
		case HORIZONTAL:
		case HORIZONTAL_WHEEL:
		default:
			if (config->isMappedLike("left", input) || config->isMappedLike("l2", input))
			{
				listInput(-1);
				return true;
			}
			if (config->isMappedLike("right", input) || config->isMappedLike("r2", input))
			{
				listInput(1);
				return true;
			}
			if ((Settings::getInstance()->getBool("QuickSystemSelect") && config->isMappedLike("down", input)) || config->isMappedTo("pagedown", input))
			{
				int cursor = moveCursorFast(true);
				listInput(cursor - mCursor);
				return true;
			}
			if ((Settings::getInstance()->getBool("QuickSystemSelect") && config->isMappedLike("up", input)) || config->isMappedTo("pageup", input))
			{
				int cursor = moveCursorFast(false);
				listInput(cursor - mCursor);
				return true;
			}

			break;
		}

		if(config->isMappedTo(BUTTON_OK, input))
		{
			stopScrolling();
			ViewController::get()->goToGameList(getSelected());
			return true;
		}

		if (config->isMappedTo(BUTTON_BACK, input))
		{
			if (showNavigationBar())
				return true;
		}
		
		if (config->isMappedTo("x", input))
		{
			// get random system
			// go to system
			setCursor(SystemData::getRandomSystem());
			return true;
		}
				
		if(config->isMappedTo("select", input))
		{
			GuiMenu::openQuitMenu_static(mWindow, true);        
			return true;
		}

	}else{
		if(config->isMappedLike("left", input) ||
			config->isMappedLike("right", input) ||
			config->isMappedLike("up", input) ||
			config->isMappedLike("down", input) ||
			config->isMappedLike("pagedown", input) ||
			config->isMappedLike("pageup", input) ||
			config->isMappedLike("l2", input) ||
			config->isMappedLike("r2", input))
			listInput(0);
		/*
#ifdef WIN32		
		if(!UIModeController::getInstance()->isUIModeKid() && config->isMappedTo("select", input) && Settings::getInstance()->getBool("ScreenSaverControls"))
		{
			mWindow->startScreenSaver();
			mWindow->renderScreenSaver();
			return true;
		}
#endif*/
	}

	return GuiComponent::input(config, input);
}

bool SystemView::showNavigationBar()
{
	if (!SystemData::IsManufacturerSupported)
		return false;

	auto sortMode = Settings::getInstance()->getString("SortSystems");
	if (sortMode == "alpha")
	{
		showNavigationBar(_("GO TO LETTER"), [](SystemData* meta) { if (meta->isCollection()) return _("COLLECTIONS"); return Utils::String::toUpper(meta->getSystemMetadata().fullName.substr(0, 1)); });
		return true;
	}
	else if (sortMode == "manufacturer" || sortMode == "subgroup")
	{
		showNavigationBar(_("GO TO MANUFACTURER"), [](SystemData* meta) { return meta->getSystemMetadata().manufacturer; });
		return true;
	}
	else if (sortMode == "hardware")
	{
		showNavigationBar(_("GO TO HARDWARE"), [](SystemData* meta) { return meta->getSystemMetadata().hardwareType; });
		return true;
	}
	else if (sortMode == "releaseDate")
	{
		showNavigationBar(_("GO TO DECADE"), [](SystemData* meta) { if (meta->getSystemMetadata().releaseYear == 0) return _("Unknown"); return std::to_string((meta->getSystemMetadata().releaseYear / 10) * 10) + "'s"; });
		return true;
	}

	return false;
}

void SystemView::showNavigationBar(const std::string& title, const std::function<std::string(SystemData* system)>& selector)
{
	stopScrolling();
	
	GuiSettings* gs = new GuiSettings(mWindow, title, "-----"); // , "", nullptr, true);

	int idx = 0;
	std::string sel = selector(getSelected());

	std::string man = "*-*";
	for (int i = 0; i < SystemData::sSystemVector.size(); i++)
	{
		auto system = SystemData::sSystemVector[i];
		if (!system->isVisible())
			continue;
		
		auto mf = selector(system);
		if (man != mf)
		{
			std::vector<std::string> names;
			for (auto sy : SystemData::sSystemVector)
				if (sy->isVisible() && selector(sy) == mf)
					names.push_back(sy->getFullName());

			gs->getMenu().addWithDescription(mf, Utils::String::join(names, ", "), nullptr, [this, gs, system, idx]
			{
				listInput(idx - mCursor);
				listInput(0);

				auto pthis = this;

				delete gs;

				pthis->mLastCursor = -1;
				pthis->onCursorChanged(CURSOR_STOPPED);

			}, "", sel == mf);

			man = mf;
		}

		idx++;
	}
	
	float w = Math::min(Renderer::getScreenWidth() * 0.5, ThemeData::getMenuTheme()->Text.font->sizeText("S").x() * 31.0f);
	w = Math::max(w, Renderer::getScreenWidth() / 3.0f);

	gs->getMenu().setSize(w, Renderer::getScreenHeight());

	gs->getMenu().animateTo(
		Vector2f(-w, 0),
		Vector2f(0, 0), AnimateFlags::OPACITY | AnimateFlags::POSITION);

	mWindow->pushGui(gs);
}

void SystemView::update(int deltaTime)
{
	for(auto sb : mStaticBackgrounds)
		sb->update(deltaTime);

	listUpdate(deltaTime);
	mSystemInfo.update(deltaTime);

	for (auto it = mEntries.cbegin(); it != mEntries.cend(); it++)
	{
		if (it->data.logo)
			it->data.logo->update(deltaTime);

		for (auto xt : it->data.backgroundExtras)
			xt->update(deltaTime);
	}
	
	GuiComponent::update(deltaTime);

	if (mYButton.isLongPressed(deltaTime))
	{
		bool netPlay = SystemData::isNetplayActivated() && SystemConf::getInstance()->getBool("global.netplay");
		if (netPlay)
			setCursor(SystemData::getRandomSystem());
		else
			showQuickSearch();
	}
}

void SystemView::updateExtraTextBinding()
{
	if (mCursor < 0 || mCursor >= mEntries.size())
		return;

	auto system = getSelected();
	if (system == nullptr)
		return;

	for (auto extra : mEntries[mCursor].data.backgroundExtras)
		BindingManager::updateBindings(extra, system);
}

void SystemView::onCursorChanged(const CursorState& state)
{
	if (AudioManager::isInitialized())
		AudioManager::getInstance()->changePlaylist(getSelected()->getTheme());
	
	Utils::FileSystem::preloadFileSystemCache(mEntries.at(mCursor).object->getRootFolder()->getPath(), true);

	ensureLogo(mEntries.at(mCursor));

	// update help style
	updateHelpPrompts();

	float startPos = mCamOffset;

	float posMax = (float)mEntries.size();
	float target = (float)mCursor;

	// what's the shortest way to get to our target?
	// it's one of these...

	float endPos = target; // directly
	float dist = abs(endPos - startPos);

	if(abs(target + posMax - startPos) < dist)
		endPos = target + posMax; // loop around the end (0 -> max)
	if(abs(target - posMax - startPos) < dist)
		endPos = target - posMax; // loop around the start (max - 1 -> -1)
	
	// animate mSystemInfo's opacity (fade out, wait, fade back in)
	cancelAnimation(1);
	cancelAnimation(2);

	std::string transition_style = Settings::TransitionStyle();
	if (transition_style == "auto")
	{
		if (mCarousel.defaultTransition == "instant" || mCarousel.defaultTransition == "fade" || mCarousel.defaultTransition == "slide" || mCarousel.defaultTransition == "fade & slide")
			transition_style = mCarousel.defaultTransition;
		else
			transition_style = "slide";
	}

	int systemInfoDelay = mCarousel.systemInfoDelay;

	bool goFast = transition_style == "instant" || systemInfoDelay == 0;
	const float infoStartOpacity = mSystemInfo.getOpacity() / 255.f;

	Animation* infoFadeOut = new LambdaAnimation(
		[infoStartOpacity, this] (float t)
	{
		mSystemInfo.setOpacity((unsigned char)(Math::lerp(infoStartOpacity, 0.f, t) * 255));
	}, (int)(infoStartOpacity * (goFast ? 10 : 150)));

	unsigned int gameCount = getSelected()->getGameCountInfo()->visibleGames;

	updateExtraTextBinding();

	// also change the text after we've fully faded out
	if (!mLockExtraChanges)
	{
		setAnimation(infoFadeOut, 0, [this, gameCount]
		{
			if (!getSelected()->isGameSystem() && !getSelected()->isGroupSystem())
				mSystemInfo.setText(_("CONFIGURATION"));
			else if (mCarousel.systemInfoCountOnly)
				mSystemInfo.setText(std::to_string(gameCount));
			else
			{
				std::stringstream ss;
				char strbuf[256];

				if (getSelected() == CollectionSystemManager::get()->getCustomCollectionsBundle())
				{
					int collectionCount = getSelected()->getRootFolder()->getChildren().size();
					snprintf(strbuf, 256, ngettext("%i COLLECTION", "%i COLLECTIONS", collectionCount), collectionCount);
				}
				else if (getSelected()->hasPlatformId(PlatformIds::PLATFORM_IGNORE) && !getSelected()->isCollection())
					snprintf(strbuf, 256, ngettext("%i ITEM", "%i ITEMS", gameCount), gameCount);
				else
					snprintf(strbuf, 256, ngettext("%i GAME", "%i GAMES", gameCount), gameCount);

				ss << strbuf;
				mSystemInfo.setText(ss.str());
			}

			mSystemInfo.onShow();
		}, false, 1);
	}

	if (!mSystemInfo.hasStoryBoard())
	{
		Animation* infoFadeIn = new LambdaAnimation(
			[this](float t)
		{
			mSystemInfo.setOpacity((unsigned char)(Math::lerp(0.f, 1.f, t) * 255));
		}, goFast ? 10 : 300);

		// wait 600ms to fade in
		setAnimation(infoFadeIn, goFast ? 0 : systemInfoDelay, nullptr, false, 2);
	}

	// no need to animate transition, we're not going anywhere (probably mEntries.size() == 1)
	if(endPos == mCamOffset && endPos == mExtrasCamOffset)
		return;

	if (mLastCursor == mCursor)
		return;

	// tts
	if(state == CURSOR_STOPPED)
	{
	  TextToSpeech::getInstance()->say(getSelected()->getFullName());
	  Scripting::fireEvent("system-selected", getSelected()->getName());
	}

	if (!mCarousel.scrollSound.empty())
		Sound::get(mCarousel.scrollSound)->play();

	int oldCursor = mLastCursor;
	mLastCursor = mCursor;

	bool oldCursorHasStoryboard = false;

	if (oldCursor >= 0 && oldCursor < mEntries.size())
	{
		auto logo = mEntries.at(oldCursor).data.logo;				
		if (logo)
		{
			if (logo->selectStoryboard("deactivate"))
			{
				logo->startStoryboard();
				oldCursorHasStoryboard = true;
			}
			else
				logo->deselectStoryboard();
		}
	}

	bool cursorHasStoryboard = false;

	if (mCursor >= 0 && mCursor < mEntries.size())
	{
		auto logo = mEntries.at(mCursor).data.logo;
		if (logo)
		{
			if (logo->selectStoryboard("activate"))
			{
				logo->startStoryboard();
				cursorHasStoryboard = true;
			}
			else
				logo->deselectStoryboard();
		}
	}

	for (int i = 0 ; i < mEntries.size() ; i++)
	{
		if ((cursorHasStoryboard && i == mCursor) || (oldCursorHasStoryboard && i == oldCursor))
			continue;

		auto logo = mEntries.at(i).data.logo;
		if (logo && logo->selectStoryboard("scroll"))
			logo->startStoryboard();		
	}

	Animation* anim;
	bool move_carousel = Settings::getInstance()->getBool("MoveCarousel");
	if (Settings::PowerSaverMode() == "instant")
		move_carousel = false;

	bool lockExtraChanges = mLockExtraChanges;
	
	if (transition_style == "fade" || transition_style == "fade & slide")
	{
		anim = new LambdaAnimation([this, lockExtraChanges, startPos, endPos, posMax, move_carousel, oldCursor, transition_style](float t)
		{
			mExtrasFadeOldCursor = oldCursor;

			float f = Math::lerp(startPos, endPos, Math::easeOutQuint(t));

			if (f < 0) f += posMax;
			if (f >= posMax) f -= posMax;

			if (!mLockCamOffsetChanges)
				this->mCamOffset = move_carousel ? f : endPos;

			if (!lockExtraChanges)
			{
				this->mExtrasFadeOpacity = Math::lerp(1, 0, Math::easeOutQuint(t));

				if (transition_style == "fade & slide")
					this->mExtrasFadeMove = Math::lerp(1, 0, Math::easeOutCubic(t));

				this->mExtrasCamOffset = endPos;
			}

		}, mCarousel.transitionSpeed);
	} 
	else if (transition_style == "slide") 
	{
		anim = new LambdaAnimation([this, lockExtraChanges, startPos, endPos, posMax, move_carousel](float t)
		{			
			float f = Math::lerp(startPos, endPos, Math::easeOutQuint(t));
			if (f < 0) f += posMax;
			if (f >= posMax) f -= posMax;

			if (!mLockCamOffsetChanges)
				this->mCamOffset = move_carousel ? f : endPos;

			if (!lockExtraChanges)
				this->mExtrasCamOffset = f;

		}, mCarousel.transitionSpeed);
	} 
	else // instant
	{		
		anim = new LambdaAnimation([this, lockExtraChanges,  startPos, endPos, posMax, move_carousel ](float t)
		{
			float f = Math::lerp(startPos, endPos, Math::easeOutQuint(t));
			if (f < 0) f += posMax; 
			if (f >= posMax) f -= posMax;

			if (!mLockCamOffsetChanges)
				this->mCamOffset = move_carousel ? f : endPos;

			if (!lockExtraChanges)
				this->mExtrasCamOffset = endPos;

		}, move_carousel ? mCarousel.transitionSpeed : 1);
	}

	if (!mLockExtraChanges)
	{
		for (int i = 0; i < mEntries.size(); i++)
			if (i != oldCursor && i != mCursor)
				activateExtras(i, false);

		activateExtras(mCursor);
	}
	
	setAnimation(anim, 0, [this, lockExtraChanges]
	{
		if (!lockExtraChanges)
		{
			mExtrasFadeOpacity = 0.0f;
			mExtrasFadeMove = 0.0f;
			mExtrasFadeOldCursor = -1;

			for (int i = 0; i < mEntries.size(); i++)
				if (i != mCursor)
					activateExtras(i, false);
		}
	}, false, 0);
}

void SystemView::render(const Transform4x4f& parentTrans)
{
	if (size() == 0 || !mVisible)
		return;  // nothing to render

	Transform4x4f trans = getTransform() * parentTrans;

	auto rect = Renderer::getScreenRect(trans, mSize);
	if (!Renderer::isVisibleOnScreen(rect))
		return;

	auto systemInfoZIndex = mSystemInfo.getZIndex();
	auto minMax = std::minmax(mCarousel.zIndex, systemInfoZIndex);

	renderExtras(trans, INT16_MIN, minMax.first);

	for (auto sb : mStaticBackgrounds)
		sb->render(trans);

	if (mCarousel.zIndex > mSystemInfo.getZIndex()) {
		renderInfoBar(trans);
	} else {
		renderCarousel(trans);
	}

	renderExtras(trans, minMax.first, minMax.second);

	if (mCarousel.zIndex > mSystemInfo.getZIndex()) {
		renderCarousel(trans);
	} else {
		renderInfoBar(trans);
	}

	renderExtras(trans, minMax.second, INT16_MAX);
}

std::vector<HelpPrompt> SystemView::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;

	if (mCarousel.type == VERTICAL || mCarousel.type == VERTICAL_WHEEL)
		prompts.push_back(HelpPrompt("up/down", _("CHOOSE")));
	else
		prompts.push_back(HelpPrompt("left/right", _("CHOOSE")));
	
	prompts.push_back(HelpPrompt(BUTTON_OK, _("SELECT"), [&] {ViewController::get()->goToGameList(getSelected()); }));

	bool netPlay = SystemData::isNetplayActivated() && SystemConf::getInstance()->getBool("global.netplay");

	if (netPlay)
	{
		prompts.push_back(HelpPrompt("x", _("NETPLAY"), [&] { showNetplay(); }));
		prompts.push_back(HelpPrompt("y", _("SEARCH") + std::string("/") + _("RANDOM"), [&] { showQuickSearch(); })); // QUICK 
	}
	else
	{
		prompts.push_back(HelpPrompt("x", _("RANDOM")));	
		if (SystemData::getSystem("all") != nullptr)
			prompts.push_back(HelpPrompt("y", _("SEARCH"), [&] { showQuickSearch(); })); // QUICK 
	}

	if (SystemData::IsManufacturerSupported)
		prompts.push_back(HelpPrompt("b", _("NAVIGATION BAR"), [&] { showNavigationBar(); }));
	
#ifdef _ENABLE_FILEMANAGER_
	if (UIModeController::getInstance()->isUIModeFull()) {
		prompts.push_back(HelpPrompt("F1", _("FILES")));
	}
#endif

	return prompts;
}

HelpStyle SystemView::getHelpStyle()
{
	HelpStyle style;
	style.applyTheme(mEntries.at(mCursor).object->getTheme(), "system");
	return style;
}

void  SystemView::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	LOG(LogDebug) << "SystemView::onThemeChanged()";
	mViewNeedsReload = true;
	populate();
}

//  Get the ThemeElements that make up the SystemView.
void  SystemView::getViewElements(const std::shared_ptr<ThemeData>& theme)
{
	LOG(LogDebug) << "SystemView::getViewElements()";

	getDefaultElements();

	if (!theme->hasView("system"))
		return;

	const ThemeData::ThemeElement* carouselElem = theme->getElement("system", "systemcarousel", "carousel");
	if (carouselElem)
		getCarouselFromTheme(carouselElem);

	const ThemeData::ThemeElement* sysInfoElem = theme->getElement("system", "systemInfo", "text");
	if (sysInfoElem)
	{
		mSystemInfo.applyTheme(theme, "system", "systemInfo", ThemeFlags::ALL);
		mSystemInfo.setOpacity(0);
	}
		
	for (auto sb : mStaticBackgrounds) delete sb;
	mStaticBackgrounds.clear();

	for (auto name : theme->getElementNames("system", "image"))
	{
		if (Utils::String::startsWith(name, "staticBackground"))
		{
			ImageComponent* staticBackground = new ImageComponent(mWindow, false);
			staticBackground->applyTheme(theme, "system", name, ThemeFlags::ALL);
			mStaticBackgrounds.push_back(staticBackground);		
		}
	}

	for (auto name : theme->getElementNames("system", "video"))
	{
		if (Utils::String::startsWith(name, "staticBackground"))
		{
			VideoVlcComponent* sv = new VideoVlcComponent(mWindow);
			sv->applyTheme(theme, "system", name, ThemeFlags::ALL);
			mStaticBackgrounds.push_back(sv);
		}
	}

	std::stable_sort(mStaticBackgrounds.begin(), mStaticBackgrounds.end(), [](GuiComponent* a, GuiComponent* b) { return b->getZIndex() > a->getZIndex(); });

	mViewNeedsReload = false;
}

//  Render system carousel
void SystemView::renderCarousel(const Transform4x4f& trans)
{
	// background box behind logos
	Transform4x4f carouselTrans = trans;
	carouselTrans.translate(Vector3f(mCarousel.pos.x(), mCarousel.pos.y(), 0.0));
	carouselTrans.translate(Vector3f(mCarousel.origin.x() * mCarousel.size.x() * -1, mCarousel.origin.y() * mCarousel.size.y() * -1, 0.0f));

	Vector2f clipPos(carouselTrans.translation().x(), carouselTrans.translation().y());
	Renderer::pushClipRect(Vector2i((int)clipPos.x(), (int)clipPos.y()), Vector2i((int)mCarousel.size.x(), (int)mCarousel.size.y()));

	Renderer::setMatrix(carouselTrans);
	Renderer::drawRect(0.0f, 0.0f, mCarousel.size.x(), mCarousel.size.y(), mCarousel.color, mCarousel.colorEnd, mCarousel.colorGradientHorizontal);

	// draw logos
	Vector2f logoSpacing(0.0, 0.0); // NB: logoSpacing will include the size of the logo itself as well!
	float xOff = 0.0;
	float yOff = 0.0;

	switch (mCarousel.type)
	{
		case VERTICAL_WHEEL:
			yOff = (mCarousel.size.y() - mCarousel.logoSize.y()) / 2.f - (mCamOffset * logoSpacing[1]);
			if (mCarousel.logoAlignment == ALIGN_LEFT)
				xOff = mCarousel.logoSize.x() / 10.f;
			else if (mCarousel.logoAlignment == ALIGN_RIGHT)
				xOff = mCarousel.size.x() - (mCarousel.logoSize.x() * 1.1f);
			else
				xOff = (mCarousel.size.x() - mCarousel.logoSize.x()) / 2.f;
			break;
		case VERTICAL:
			logoSpacing[1] = ((mCarousel.size.y() - (mCarousel.logoSize.y() * mCarousel.maxLogoCount)) / (mCarousel.maxLogoCount)) + mCarousel.logoSize.y();
			yOff = (mCarousel.size.y() - mCarousel.logoSize.y()) / 2.f - (mCamOffset * logoSpacing[1]);

			if (mCarousel.logoAlignment == ALIGN_LEFT)
				xOff = mCarousel.logoSize.x() / 10.f;
			else if (mCarousel.logoAlignment == ALIGN_RIGHT)
				xOff = mCarousel.size.x() - (mCarousel.logoSize.x() * 1.1f);
			else
				xOff = (mCarousel.size.x() - mCarousel.logoSize.x()) / 2;
			break;
		case HORIZONTAL_WHEEL:
			xOff = (mCarousel.size.x() - mCarousel.logoSize.x()) / 2 - (mCamOffset * logoSpacing[1]);
			if (mCarousel.logoAlignment == ALIGN_TOP)
				yOff = mCarousel.logoSize.y() / 10;
			else if (mCarousel.logoAlignment == ALIGN_BOTTOM)
				yOff = mCarousel.size.y() - (mCarousel.logoSize.y() * 1.1f);
			else
				yOff = (mCarousel.size.y() - mCarousel.logoSize.y()) / 2;
			break;
		case HORIZONTAL:
		default:
			logoSpacing[0] = ((mCarousel.size.x() - (mCarousel.logoSize.x() * mCarousel.maxLogoCount)) / (mCarousel.maxLogoCount)) + mCarousel.logoSize.x();
			xOff = (mCarousel.size.x() - mCarousel.logoSize.x()) / 2.f - (mCamOffset * logoSpacing[0]);

			if (mCarousel.logoAlignment == ALIGN_TOP)
				yOff = mCarousel.logoSize.y() / 10.f;
			else if (mCarousel.logoAlignment == ALIGN_BOTTOM)
				yOff = mCarousel.size.y() - (mCarousel.logoSize.y() * 1.1f);
			else
				yOff = (mCarousel.size.y() - mCarousel.logoSize.y()) / 2.f;
			break;
	}

	if (mCarousel.logoPos.x() >= 0)
		xOff = mCarousel.logoPos.x() - (mCarousel.type == HORIZONTAL ? (mCamOffset * logoSpacing[0]) : 0);

	if (mCarousel.logoPos.y() >= 0)
		yOff = mCarousel.logoPos.y() - (mCarousel.type == VERTICAL ? (mCamOffset * logoSpacing[1]) : 0);

	int center = (int)(mCamOffset);
	int logoCount = Math::min(mCarousel.maxLogoCount, (int)mEntries.size());

	// Adding texture loading buffers depending on scrolling speed and status
	int bufferIndex = Math::max(0, Math::min(2, getScrollingVelocity() + 1));
	int bufferLeft = logoBuffersLeft[bufferIndex];
	int bufferRight = logoBuffersRight[bufferIndex];

	if (logoCount == 1 && mCamOffset == 0)
	{
		bufferLeft = 0;
		bufferRight = 0;
	}

	auto renderLogo = [this, carouselTrans, logoSpacing, xOff, yOff](int i)
	{
		int index = i % (int)mEntries.size();
		if (index < 0)
			index += (int)mEntries.size();

		Transform4x4f logoTrans = carouselTrans;
		logoTrans.translate(Vector3f(i * logoSpacing[0] + xOff, i * logoSpacing[1] + yOff, 0));

		float distance = i - mCamOffset;

		float scale = 1.0f + ((mCarousel.logoScale - 1.0f) * (1.0f - fabs(distance)));
		scale = Math::min(mCarousel.logoScale, Math::max(1.0f, scale));
		scale /= mCarousel.logoScale;

		int opref = (Math::clamp(mCarousel.minLogoOpacity, 0, 1) * 255);

		int opacity = (int)Math::round(opref + ((0xFF - opref) * (1.0f - fabs(distance))));
		opacity = Math::max((int)opref, opacity);

		ensureLogo(mEntries.at(index));

		const std::shared_ptr<GuiComponent> &comp = mEntries.at(index).data.logo;
		if (mCarousel.type == VERTICAL_WHEEL || mCarousel.type == HORIZONTAL_WHEEL) {
			comp->setRotationDegrees(mCarousel.logoRotation * distance);
			comp->setRotationOrigin(mCarousel.logoRotationOrigin);
		}
		
		if (!mCarousel.anyLogoHasOpacityStoryboard)
			comp->setOpacity((unsigned char)opacity);

		if (!mCarousel.anyLogoHasScaleStoryboard)
			comp->setScale(scale);
		
		comp->render(logoTrans);
	};


	std::vector<int> activePositions;
	for (int i = center - logoCount / 2 + bufferLeft; i <= center + logoCount / 2 + bufferRight; i++)
	{
		int index = i % (int)mEntries.size();
		if (index < 0)
			index += (int)mEntries.size();
	
		if (index == mCursor)
			activePositions.push_back(i);
		else
			renderLogo(i);
	}
	
	for (auto activePos : activePositions)
		renderLogo(activePos);

	Renderer::popClipRect();
}

void SystemView::renderInfoBar(const Transform4x4f& trans)
{
	Renderer::setMatrix(trans);
	mSystemInfo.render(trans);
}

#include <unordered_set>

void SystemView::setExtraRequired(int cursor, bool required)
{
	auto setTexture = [](GuiComponent* extra, const std::function<void(std::shared_ptr<TextureResource>)>& func)
	{
		if (extra->isKindOf<ImageComponent>())
		{
			auto tex = ((ImageComponent*)extra)->getTexture();
			if (tex != nullptr)
				func(tex);
		}
	};

	// Disable unloading for textures that will have to display 
	for (GuiComponent* extra : mEntries.at(cursor).data.backgroundExtras)
		setTexture(extra, [required](std::shared_ptr<TextureResource> x) { x->setRequired(required); });
}

void SystemView::preloadExtraNeighbours(int cursor)
{
	auto ensureTexture = [](GuiComponent* extra, bool reload)
	{
		if (extra == nullptr)
			return;

		ImageComponent* image = dynamic_cast<ImageComponent*>(extra);
		if (image == nullptr)
			return;
		
		auto tex = image->getTexture();
		if (tex == nullptr)
			return;

		if (reload)
			tex->reload();
		else
			tex->prioritize();
	};

	// Make sure near textures are in at top position & will be released last if VRAM is required
	int distancesStatic[] = { -2, 2, -1, 1, 0 };
	int distancesRight[] = { 3, -1, 2, 1, 0 };
	int distancesLeft[] = { -3, 1, -2, -1, 0 };

	int* distances = &distancesStatic[0];
	
	if (getScrollingVelocity() > 0)
		distances = &distancesRight[0];
	else if (getScrollingVelocity() < 0)
		distances = &distancesLeft[0];
		
	for (int dx = 0; dx < 5; dx++)
	{
		int i = cursor + distances[dx];
		int index = i % (int)mEntries.size();
		if (index < 0)
			index += (int)mEntries.size();

		Entry& entry = mEntries.at(index);

		if (entry.data.logo)
		{
			ensureTexture(entry.data.logo.get(), dx > 1);

			for (auto child : entry.data.logo->enumerateExtraChildrens())
				ensureTexture(child, dx > 1);
		}

		for (auto extra : entry.data.backgroundExtras)
			ensureTexture(extra, dx > 1);
	}
}

// Draw background extras
void SystemView::renderExtras(const Transform4x4f& trans, float lower, float upper)
{
	int min = (int)mExtrasCamOffset;
	int max = (int)(mExtrasCamOffset + 0.99999f);

	int extrasCenter = (int)mExtrasCamOffset;

	Renderer::pushClipRect(Vector2i::Zero(), Vector2i((int)mSize.x(), (int)mSize.y()));
	
	std::unordered_set<std::string> allPaths;
	std::unordered_set<std::string> paths;
	std::unordered_set<std::string> allValues;
	std::unordered_set<std::string> values;

	if (mExtrasFadeOpacity && mExtrasFadeOldCursor >= 0 && mExtrasFadeOldCursor < mEntries.size() && mExtrasFadeOldCursor != mCursor)
	{
		// ExtrasFadeOpacity : Collect images paths & text values		
		// paths & values must have only the elements that are not common 
		if (mCursor >= 0 && mCursor < mEntries.size())
		{
			for (GuiComponent* extra : mEntries.at(mCursor).data.backgroundExtras)
			{
				if (extra->getZIndex() < lower || extra->getZIndex() >= upper)
					continue;
				
				if (extra->isStaticExtra())
					continue;

				std::string value = extra->getValue();
				if (extra->isKindOf<VideoComponent>())
					paths.insert(value);
				else if (extra->isKindOf<ImageComponent>())
					paths.insert(value);
				else if (extra->isKindOf<TextComponent>())
					values.insert(value);
			}

			allValues = values;
			allPaths = paths;

			for (GuiComponent* extra : mEntries.at(mExtrasFadeOldCursor).data.backgroundExtras)
			{
				if (extra->getZIndex() < lower || extra->getZIndex() >= upper)
					continue;

				if (extra->isStaticExtra())
					continue;

				std::string value = extra->getValue();
				if (extra->isKindOf<VideoComponent>())
					paths.erase(value);
				else if (extra->isKindOf<ImageComponent>())
					paths.erase(value);
				else if (extra->isKindOf<TextComponent>())
					values.erase(value);
			}
		}

		Renderer::pushClipRect(Vector2i((int)trans.translation()[0], (int)trans.translation()[1]), Vector2i((int)mSize.x(), (int)mSize.y()));

		// ExtrasFadeOpacity : Render only items with different paths or values
		for (GuiComponent* extra : mEntries.at(mExtrasFadeOldCursor).data.backgroundExtras)
		{
			if (extra->getZIndex() < lower || extra->getZIndex() >= upper)
				continue;
			
			if (extra->isStaticExtra())
				continue;

			std::string value = extra->getValue();
			if (extra->isKindOf<ImageComponent>() || extra->isKindOf<VideoComponent>())
			{
				if (allPaths.find(value) == allPaths.cend())
				{					
					if (extra->getTag() == "background" || extra->getTag().find("bg-") == 0 || extra->getTag().find("bg_") == 0)
						extra->render(trans);
					else
					{
						auto opa = extra->getOpacity();
						extra->setOpacity(mExtrasFadeOpacity * opa);
						extra->render(trans);
						extra->setOpacity(opa);
					}
				}
				else if (extra->isKindOf<ImageComponent>() && ((ImageComponent*)extra)->isTiled() && extra->getPosition() == Vector3f::Zero() && extra->getSize() == Vector2f(Renderer::getScreenWidth(), Renderer::getScreenHeight()))					
					extra->render(trans);
			}
			else if (extra->isKindOf<TextComponent>() && allValues.find(value) == allValues.cend())
			{
				auto opa = extra->getOpacity();
				extra->setOpacity(mExtrasFadeOpacity * opa);
				extra->render(trans);
				extra->setOpacity(opa);
				//extra->render(trans);
			}				
		}

		Renderer::popClipRect();
	}

	for (int i = min ; i <= max; i++)
	{
		int index = i % (int)mEntries.size();
		if (index < 0)
			index += (int)mEntries.size();

		if (mExtrasFadeOpacity && (index == mExtrasFadeOldCursor || index != mCursor))
			continue;

		//Only render selected system when not showing
		if (!isShowing() && index != mCursor)
			continue;

		Entry& entry = mEntries.at(index);
		
		Vector2i size = Vector2i(Math::round(mSize.x()), Math::round(mSize.y()));

		Transform4x4f extrasTrans = trans;
		if (mCarousel.type == HORIZONTAL || mCarousel.type == HORIZONTAL_WHEEL)
		{
			extrasTrans.translate(Vector3f((i - mExtrasCamOffset) * mSize.x(), 0, 0));
		
			if (extrasTrans.translation()[0] >= 0 && extrasTrans.translation()[0] <= Renderer::getScreenWidth() && extrasTrans.translation()[0] + mSize.x() > Renderer::getScreenWidth())
				size.x() = Renderer::getScreenWidth() - extrasTrans.translation()[0];
		}
		else
		{
			extrasTrans.translate(Vector3f(0, (i - mExtrasCamOffset) * mSize.y(), 0));

			if (extrasTrans.translation()[1] >= 0 && extrasTrans.translation()[1] <= Renderer::getScreenHeight() && extrasTrans.translation()[1] + mSize.y() > Renderer::getScreenHeight())
				size.y() = Renderer::getScreenHeight() - extrasTrans.translation()[1];
		}

		auto rectExtra = Renderer::getScreenRect(extrasTrans, mSize);
		if (!Renderer::isVisibleOnScreen(rectExtra))
			continue;

		if (mExtrasFadeOpacity && mExtrasFadeOldCursor == index)
			extrasTrans = trans;

		Renderer::pushClipRect(Vector2i(Math::round(extrasTrans.translation()[0]), Math::round(extrasTrans.translation()[1])), Vector2i(Math::round(size.x()), Math::round(size.y())));

		for (GuiComponent* extra : mEntries.at(index).data.backgroundExtras)
		{
			if (extra->getZIndex() < lower || extra->getZIndex() >= upper)
				continue;

			// ExtrasFadeOpacity : Apply opacity only on elements that are not common with the original view
			if (mExtrasFadeOpacity && !extra->isStaticExtra())
			{			
				auto xt = extrasTrans;

				// Fade Animation
				if (mExtrasFadeMove)
				{
					float mvs = 48.0f;

					if (mCarousel.type == HORIZONTAL || mCarousel.type == HORIZONTAL_WHEEL)
					{
						if ((mExtrasFadeOldCursor < mCursor && !(mExtrasFadeOldCursor == 0 && mCursor == mEntries.size() - 1)) ||
							(mCursor == 0 && mExtrasFadeOldCursor == mEntries.size() - 1))
							xt.translate(Vector3f((mExtrasFadeMove / mvs) * mSize.x(), 0, 0));
						else
							xt.translate(Vector3f(-(mExtrasFadeMove / mvs) * mSize.x(), 0, 0));
					}
					else
					{
						if ((mExtrasFadeOldCursor < mCursor && !(mExtrasFadeOldCursor == 0 && mCursor == mEntries.size() - 1)) ||
							(mCursor == 0 && mExtrasFadeOldCursor == mEntries.size() - 1))
							xt.translate(Vector3f(0, (mExtrasFadeMove / mvs) * mSize.y(), 0));
						else
							xt.translate(Vector3f(0, -(mExtrasFadeMove / mvs) * mSize.y(), 0));
					}
				}

				std::string value = extra->getValue();
				if (extra->isKindOf<ImageComponent>() || extra->isKindOf<VideoComponent>())
				{
					if (paths.find(value) != paths.cend())
					{							
						auto opa = extra->getOpacity();
						extra->setOpacity((1.0f - mExtrasFadeOpacity) * opa);						
						extra->render(extra->isStaticExtra() ? trans : xt);
						extra->setOpacity(opa);
						continue;
					}								
					else if (extra->isKindOf<ImageComponent>() && ((ImageComponent*)extra)->isTiled() && extra->getPosition() == Vector3f::Zero() && extra->getSize() == Vector2f(Renderer::getScreenWidth(), Renderer::getScreenHeight()))
					{
						auto opa = extra->getOpacity();
						extra->setOpacity((1.0f - mExtrasFadeOpacity) * opa);
						extra->render(extra->isStaticExtra() ? trans : extrasTrans);
						extra->setOpacity(opa);
						continue;
					}
				}
				else if (extra->isKindOf<TextComponent>() && values.find(value) != values.cend())
				{
					auto opa = extra->getOpacity();
					extra->setOpacity((1.0f - mExtrasFadeOpacity) * opa);
					extra->render(extra->isStaticExtra() ? trans : extrasTrans);
					extra->setOpacity(opa);
					continue;
				}
			}
			
			if (extra->isStaticExtra())
			{		
				bool popClip = false;
				
				if (extrasTrans.translation()[0] > Renderer::getScreenWidth())
					continue;
				else if (extrasTrans.translation()[1] > Renderer::getScreenHeight())
					continue;
				else if (extrasTrans.translation()[0] < 0)
				{
					int x = Math::round(size.x() + extrasTrans.translation()[0]);
					if (x == 0)
						continue;

					Renderer::pushClipRect(Vector2i(0, Math::round(extrasTrans.translation()[1])), Vector2i(x, Math::round(size.y())));
					popClip = true;
				}
				else if (extrasTrans.translation()[1] < 0)
				{
					int y = Math::round(size.y() + extrasTrans.translation()[1]);
					if (y == 0)
						continue;

					Renderer::pushClipRect(Vector2i(Math::round(extrasTrans.translation()[0]), 0), Vector2i(Math::round(size.x()), y));
					popClip = true;
				}
			
				extra->render(trans);
				
				if (popClip)
					Renderer::popClipRect();	
			}
			else
				extra->render(extrasTrans);
		}

		Renderer::popClipRect();		
	}

	Renderer::popClipRect();
}

// Populate the system carousel with the legacy values
void  SystemView::getDefaultElements(void)
{
	// Carousel
	mCarousel.type = HORIZONTAL;
	mCarousel.logoAlignment = ALIGN_CENTER;
	mCarousel.size.x() = mSize.x();
	mCarousel.size.y() = 0.2325f * mSize.y();
	mCarousel.pos.x() = 0.0f;
	mCarousel.pos.y() = 0.5f * (mSize.y() - mCarousel.size.y());
	mCarousel.origin.x() = 0.0f;
	mCarousel.origin.y() = 0.0f;
	mCarousel.color = 0xFFFFFFD8;
	mCarousel.colorEnd = 0xFFFFFFD8;
	mCarousel.colorGradientHorizontal = true;
	mCarousel.logoScale = 1.2f;
	mCarousel.logoRotation = 7.5f;
	mCarousel.logoRotationOrigin.x() = -5;
	mCarousel.logoRotationOrigin.y() = 0.5;
	mCarousel.logoSize.x() = 0.25f * mSize.x();
	mCarousel.logoSize.y() = 0.155f * mSize.y();
	mCarousel.logoPos = Vector2f(-1, -1);
	mCarousel.maxLogoCount = 3;
	mCarousel.zIndex = 40;
	mCarousel.systemInfoDelay = 2000;
	mCarousel.systemInfoCountOnly = false;
	mCarousel.scrollSound = "";
	mCarousel.defaultTransition = "";
	mCarousel.transitionSpeed = 500;
	mCarousel.minLogoOpacity = 0.5f;
	mCarousel.anyLogoHasOpacityStoryboard = false;
	mCarousel.anyLogoHasScaleStoryboard = false;

	// System Info Bar
	mSystemInfo.setSize(mSize.x(), mSystemInfo.getFont()->getLetterHeight()*2.2f);
	mSystemInfo.setPosition(0, (mCarousel.pos.y() + mCarousel.size.y() - 0.2f));
	mSystemInfo.setBackgroundColor(0xDDDDDDD8);
	mSystemInfo.setRenderBackground(true);
	mSystemInfo.setFont(Font::get((int)(0.035f * mSize.y()), Font::getDefaultPath()));
	mSystemInfo.setColor(0x000000FF);
	mSystemInfo.setZIndex(50);
	mSystemInfo.setDefaultZIndex(50);

	for (auto sb : mStaticBackgrounds) delete sb;
	mStaticBackgrounds.clear();
}

void SystemView::getCarouselFromTheme(const ThemeData::ThemeElement* elem)
{
	if (elem->has("type"))
	{
		if (!(elem->get<std::string>("type").compare("vertical")))
			mCarousel.type = VERTICAL;
		else if (!(elem->get<std::string>("type").compare("vertical_wheel")))
			mCarousel.type = VERTICAL_WHEEL;
		else if (!(elem->get<std::string>("type").compare("horizontal_wheel")))
			mCarousel.type = HORIZONTAL_WHEEL;
		else
			mCarousel.type = HORIZONTAL;
	}
	if (elem->has("size"))
		mCarousel.size = elem->get<Vector2f>("size") * mSize;
	if (elem->has("pos"))
		mCarousel.pos = elem->get<Vector2f>("pos") * mSize;
	if (elem->has("origin"))
		mCarousel.origin = elem->get<Vector2f>("origin");
	if (elem->has("color"))
	{
		mCarousel.color = elem->get<unsigned int>("color");
		mCarousel.colorEnd = mCarousel.color;
	}
	if (elem->has("colorEnd"))
		mCarousel.colorEnd = elem->get<unsigned int>("colorEnd");
	if (elem->has("gradientType"))
		mCarousel.colorGradientHorizontal = elem->get<std::string>("gradientType").compare("horizontal");
	if (elem->has("logoScale"))
		mCarousel.logoScale = elem->get<float>("logoScale");
	if (elem->has("logoSize"))
		mCarousel.logoSize = elem->get<Vector2f>("logoSize") * mSize;
	if (elem->has("logoPos"))
		mCarousel.logoPos = elem->get<Vector2f>("logoPos") * mSize;
	if (elem->has("maxLogoCount"))
		mCarousel.maxLogoCount = (int)Math::round(elem->get<float>("maxLogoCount"));
	if (elem->has("zIndex"))
		mCarousel.zIndex = elem->get<float>("zIndex");
	if (elem->has("logoRotation"))
		mCarousel.logoRotation = elem->get<float>("logoRotation");
	if (elem->has("logoRotationOrigin"))
		mCarousel.logoRotationOrigin = elem->get<Vector2f>("logoRotationOrigin");
	if (elem->has("logoAlignment"))
	{
		if (!(elem->get<std::string>("logoAlignment").compare("left")))
			mCarousel.logoAlignment = ALIGN_LEFT;
		else if (!(elem->get<std::string>("logoAlignment").compare("right")))
			mCarousel.logoAlignment = ALIGN_RIGHT;
		else if (!(elem->get<std::string>("logoAlignment").compare("top")))
			mCarousel.logoAlignment = ALIGN_TOP;
		else if (!(elem->get<std::string>("logoAlignment").compare("bottom")))
			mCarousel.logoAlignment = ALIGN_BOTTOM;
		else
			mCarousel.logoAlignment = ALIGN_CENTER;
	}

	if (elem->has("systemInfoDelay"))
		mCarousel.systemInfoDelay = elem->get<float>("systemInfoDelay");

	if (elem->has("systemInfoCountOnly"))
		mCarousel.systemInfoCountOnly = elem->get<bool>("systemInfoCountOnly");

	if (elem->has("scrollSound"))
		mCarousel.scrollSound = elem->get<std::string>("scrollSound");

	if (elem->has("defaultTransition"))
		mCarousel.defaultTransition = elem->get<std::string>("defaultTransition");

	if (elem->has("minLogoOpacity"))
		mCarousel.minLogoOpacity = elem->get<float>("minLogoOpacity");
	
	if (elem->has("transitionSpeed"))
		mCarousel.transitionSpeed = elem->get<float>("transitionSpeed");
}

void SystemView::onShow()
{
	GuiComponent::onShow();		

	bool cursorStoryboardSet = false;

	for (int i = 0; i < mEntries.size(); i++)
	{
		auto logo = mEntries.at(i).data.logo;
		if (logo)
			logo->isShowing() = true;
	}

	if (mCursor >= 0 && mCursor < mEntries.size())
	{
		auto logo = mEntries.at(mCursor).data.logo;
		if (logo)
		{
			if (logo->selectStoryboard("activate"))
			{
				logo->startStoryboard();
				cursorStoryboardSet = true;
			}
			else if (logo->selectStoryboard())
			{
				logo->startStoryboard();
				cursorStoryboardSet = true;
			}
			else 
				logo->deselectStoryboard();
		}
	}

	for (int i = 0 ; i < mEntries.size() ; i++)
	{
		if (cursorStoryboardSet && mCursor == i)
			continue;

		auto logo = mEntries.at(i).data.logo;
		if (logo && (logo->selectStoryboard("scroll") || logo->selectStoryboard()))
			logo->startStoryboard();
	}

	activateExtras(mCursor);

	for (auto sb : mStaticBackgrounds)
		sb->onShow();

	if (getSelected() != nullptr)
		TextToSpeech::getInstance()->say(getSelected()->getFullName());
}

void SystemView::onHide()
{
	GuiComponent::onHide();	

	for (int i = 0; i < mEntries.size(); i++)
	{
		auto logo = mEntries.at(i).data.logo;
		if (logo)
			logo->isShowing() = false;
	}

	updateExtras([this](GuiComponent* p) { p->onHide(); });

	for (auto sb : mStaticBackgrounds)
		sb->onHide();
}

void SystemView::onScreenSaverActivate()
{
	mScreensaverActive = true;
	updateExtras([this](GuiComponent* p) { p->onScreenSaverActivate(); });

	for (auto sb : mStaticBackgrounds)
		sb->onScreenSaverActivate();
}

void SystemView::onScreenSaverDeactivate()
{
	mScreensaverActive = false;
	updateExtras([this](GuiComponent* p) { p->onScreenSaverDeactivate(); });

	for (auto sb : mStaticBackgrounds)
		sb->onScreenSaverDeactivate();
}

void SystemView::topWindow(bool isTop)
{
	mDisable = !isTop;
	updateExtras([this, isTop](GuiComponent* p) { p->topWindow(isTop); });

	for (auto sb : mStaticBackgrounds)
		sb->topWindow(isTop);
}

void SystemView::updateExtras(const std::function<void(GuiComponent*)>& func)
{
	for (auto it : mEntries)
		for (auto xt : it.data.backgroundExtras)
			func(xt);
}

void SystemView::activateExtras(int cursor, bool activate)
{
	if (cursor < 0 || cursor >= mEntries.size())
		return;

	bool show = activate && isShowing() && !mScreensaverActive && !mDisable;

	SystemViewData data = mEntries.at(cursor).data;
	for (unsigned int j = 0; j < data.backgroundExtras.size(); j++)
	{
		GuiComponent *extra = data.backgroundExtras[j];

		if (show && activate)
			extra->onShow();
		else
			extra->onHide();
	}

	setExtraRequired(cursor, activate);

	if (activate)
		preloadExtraNeighbours(cursor);
}

SystemData* SystemView::getActiveSystem()
{
	if (mCursor < 0 || mCursor >= mEntries.size())
		return nullptr;

	return mEntries[mCursor].object;
}


bool SystemView::hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult)
{
	bool ret = GuiComponent::hitTest(x, y, parentTransform, pResult);

	if (mCursor < 0 || mCursor >= mEntries.size())
		return ret;

	Transform4x4f trans = getTransform() * parentTransform;

	for (auto extra : mEntries[mCursor].data.backgroundExtras)
		ret |= extra->hitTest(x, y, trans, pResult);

	return ret;
}


bool SystemView::onMouseClick(int button, bool pressed, int x, int y)
{
	if (button == 1 && pressed)
	{
		mLockCamOffsetChanges = false;
		mPressedPoint = Vector2i(x, y);
		mPressedCursor = mCursor;
		mWindow->setMouseCapture(this);		
	}
	else if (button == 1 && !pressed)
	{		
		if (mWindow->hasMouseCapture(this))
		{
			mLockCamOffsetChanges = false;

			mWindow->releaseMouseCapture();

			mPressedPoint = Vector2i(-1, -1);

			if (mCamOffset != mCursor)
			{
				mLastCursor = -1;
				mLockExtraChanges = true;
				onCursorChanged(CursorState::CURSOR_STOPPED);
				mLockExtraChanges = false;
			}

			stopScrolling();

			if (mPressedCursor == mCursor)
				ViewController::get()->goToGameList(getSelected());
		}
	}
	else if (button == 2 && pressed)
	{
		showQuickSearch();
	}
	else if (button == 3 && pressed)
	{
		showNavigationBar();	
	}

	return true;
}

#define CAROUSEL_MOUSE_SPEED 100.0f

void SystemView::onMouseMove(int x, int y)
{
	if (mPressedPoint.x() != -1 && mPressedPoint.y() != -1 && mWindow->hasMouseCapture(this))
	{
		mPressedCursor = -1;

		if (isHorizontalCarousel())
		{
			float speed = CAROUSEL_MOUSE_SPEED;
			if (mCarousel.maxLogoCount >= 1)
				speed = mSize.x() / mCarousel.maxLogoCount;

			if (mCarousel.type == HORIZONTAL_WHEEL)
				speed *= 2;

			mCamOffset += (mPressedPoint.x() - x) / speed;
		}
		else
		{
			float speed = CAROUSEL_MOUSE_SPEED;
			if (mCarousel.maxLogoCount >= 1)
				speed = mSize.y() / mCarousel.maxLogoCount;

			if (mCarousel.type == VERTICAL_WHEEL)
				speed *= 2;

			mCamOffset += (mPressedPoint.y() - y) / speed;
		}

		int itemCount = mEntries.size();

		if (mCamOffset < 0)
			mCamOffset += itemCount;
		else if (mCamOffset >= itemCount)
			mCamOffset = mCamOffset - (float)itemCount;

		int offset = (int)Math::round(mCamOffset);
		if (offset < 0)
			offset += (int)itemCount;
		else if (offset >= (int)itemCount)
			offset -= (int)itemCount;

		if (mCursor != offset)
		{
			float camOffset = mCamOffset;

			mLockCamOffsetChanges = true;

			if (mLastCursor == offset)
				mLastCursor = -1;

			mCursor = offset;

			onCursorChanged(CursorState::CURSOR_STOPPED);

			mCursor = offset;
			mCamOffset = camOffset;
		}

		mPressedPoint = Vector2i(x, y);
	}

}

void SystemView::onMouseWheel(int delta)
{
	listInput(-delta);
	mScrollVelocity = 0;
}



bool SystemView::onAction(const std::string& action)
{
	if (action == "netplay")
	{
		showNetplay();
		return true;
	}

	if (action == "navigationbar" || action == "back")
	{
		showNavigationBar();
		return true;
	}

	if (action == "search")
	{
		showQuickSearch();
		return true;
	}

	if (action == "launch" || action == "open")
	{
		stopScrolling();
		ViewController::get()->goToGameList(getSelected());
		return true;
	}

	if (action == "random")
	{
		setCursor(SystemData::getRandomSystem());
		return true;
	}

	if (action == "prev")
	{
		listInput(-1);
		return true;
	}

	if (action == "next")
	{
		listInput(1);
		return true;
	}

	if (action == "cheevos")
	{
		if (SystemConf::getInstance()->getBool("global.retroachievements") && !Settings::getInstance()->getBool("RetroachievementsMenuitem") && SystemConf::getInstance()->get("global.retroachievements.username") != "")
		{
			if (ApiSystem::getInstance()->getIpAdress() == "NOT CONNECTED")
				mWindow->pushGui(new GuiMsgBox(mWindow, _("YOU ARE NOT CONNECTED TO A NETWORK"), _("OK"), nullptr));				
			else
				GuiRetroAchievements::show(mWindow);
		}

		return true;
	}
	
	return false;
}