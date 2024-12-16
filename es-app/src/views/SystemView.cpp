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
#include "Log.h"
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

SystemView::SystemView(Window* window) : GuiComponent(window),
	mViewNeedsReload(true),
	mSystemInfo(window, _("SYSTEM INFO"), Font::get(FONT_SIZE_SMALL), 0x33333300, ALIGN_CENTER), mYButton("y")
{
	mExtraTransitionSpeed = 500.0f;
	mExtraTransitionHorizontal = false;

	mCamOffset = 0;
	mExtrasCamOffset = 0;
	mExtrasFadeOpacity = 0.0f;
	mExtrasFadeMove = 0.0f;
	mScreensaverActive = false;
	mDisable = false;
	mLastCursor = 0;
	mExtrasFadeOldCursor = -1;

	mSystemInfoDelay = 2000;
	mSystemInfoCountOnly = false;

	mLockCamOffsetChanges = false;
	mLockExtraChanges = false;
	mPressedCursor = -1;
	mPressedPoint = Vector2i(-1, -1);

	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	populate();
}

SystemView::~SystemView()
{
	mCarousel.attach(nullptr);

	for (auto sb : mStaticBackgrounds)
		delete sb;

	mStaticBackgrounds.clear();
	clearEntries();
}

void SystemView::clearEntries()
{
	for (auto& entry : mEntries)
	{
		setExtraRequired(entry, false);

		for (auto extra : entry.backgroundExtras)
			delete extra;

		entry.backgroundExtras.clear();
	}

	mEntries.clear();
	mCarousel.clear();
}

void SystemView::reloadTheme(SystemData* system)
{
	auto carousel = mCarousel.asCarousel();
	if (carousel == nullptr)
	{
		mCarousel.attach(nullptr);

		mViewNeedsReload = true;
		populate();

		goToSystem(system, false);
		return;
	}

	const std::shared_ptr<ThemeData>& theme = system->getTheme();
	getViewElements(theme);

	int idx = -1;
	for (int i = 0; i < mEntries.size(); i++)
	{
		if (mEntries[i].object == system)
		{
			idx = i;
			break;
		}
	}

	if (idx < 0)
		return;

	auto logo = carousel->getLogo(idx);
	if (logo != nullptr)
	{
		if (logo->isKindOf<TextComponent>())
			logo->applyTheme(theme, "system", "logoText", ThemeFlags::FONT_PATH | ThemeFlags::FONT_SIZE | ThemeFlags::COLOR | ThemeFlags::FORCE_UPPERCASE | ThemeFlags::LINE_SPACING | ThemeFlags::TEXT);
		else if (logo->isKindOf<ImageComponent>())
			logo->applyTheme(theme, "system", "logo", ThemeFlags::COLOR | ThemeFlags::ALIGNMENT | ThemeFlags::VISIBLE);
	}

	loadExtras(system);	
}

void SystemView::loadExtras(SystemData* system)
{
	auto it = std::find_if(mEntries.begin(), mEntries.end(), [system](const SystemViewData& ss) { return ss.object == system; });
	if (it != mEntries.cend())
	{
		// delete any existing extras
		for (auto extra : it->backgroundExtras)
			delete extra;

		it->backgroundExtras.clear();
	}
	
	size_t vram = Settings::getInstance()->getInt("MaxVRAM") * 1024 * 1024;
	size_t size = TextureResource::getTotalMemUsage();

	// make background extras
	auto extras = ThemeData::makeExtras(system->getTheme(), "system", mWindow);
	for (auto extra : extras)
	{
		if (extra->isKindOf<VideoComponent>())
		{
			auto elem = system->getTheme()->getElement("system", extra->getTag(), "video");
			if (elem != nullptr && elem->has("path") && Utils::String::startsWith(elem->get<std::string>("path"), "{random"))
				((VideoComponent*)extra)->setPlaylist(std::make_shared<SystemRandomPlaylist>(system, SystemRandomPlaylist::VIDEO));
			else if (elem != nullptr && elem->has("path") && Utils::String::toLower(Utils::FileSystem::getExtension(elem->get<std::string>("path"))) == ".m3u")
				((VideoComponent*)extra)->setPlaylist(std::make_shared<M3uPlaylist>(elem->get<std::string>("path")));

			continue;
		}
		
		if (extra->isKindOf<ImageComponent>())
		{
			// Preload image if there's enough VRAM space				
			auto tex = ((ImageComponent*)extra)->getTexture();
			if (tex && !tex->isLoaded())
			{
				auto texSize = tex->getEstimatedVRAMUsage();
				if (size + texSize < vram)
				{
					tex->reload();
					size += texSize;
				}
			}

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
		}
	}

	// sort the extras by z-index
	std::stable_sort(extras.begin(), extras.end(), [](GuiComponent* a, GuiComponent* b) { return b->getZIndex() > a->getZIndex(); });

	if (it == mEntries.cend())
	{
		SystemViewData data;
		data.object = system;
		data.backgroundExtras = extras;
		mEntries.push_back(data);
	}
	else
		it->backgroundExtras = extras;

	SystemRandomPlaylist::resetCache();
}


void SystemView::populate()
{
	TextureLoader::paused = true;

	clearEntries();
	
	for (auto system : SystemData::sSystemVector)
	{
		const std::shared_ptr<ThemeData>& theme = system->getTheme();
		if (theme == nullptr)
			continue;

		if (mViewNeedsReload)
			getViewElements(theme);

		if (system->isVisible())
		{
			mCarousel.add(system->getName(), system, true);
			loadExtras(system);

			auto carousel = mCarousel.asCarousel();
			if (carousel)
			{
				auto logo = carousel->getLogo(mCarousel.size() - 1);
				if (logo)
					ensureTexture(logo.get(), true);
			}
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

		return;
	}

	auto grid = mCarousel.asGrid();
	if (grid)
		grid->preloadTiles();
}

void SystemView::goToSystem(SystemData* system, bool animate)
{
	mLastCursor = -1;
	mCarousel.setCursor(system);

	if (mLastCursor == -1)
		onCursorChanged(CURSOR_STOPPED);

	if (!animate)
	{
		finishAnimation(0);
		finishAnimation(1);
		finishAnimation(2);

		mCarousel.finishAnimation(0);
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
	int mCursor = mCarousel.getCursorIndex();
	int cursor = mCursor;

	if (SystemData::IsManufacturerSupported && Settings::getInstance()->getString("SortSystems") == "manufacturer" && cursor >= 0 && cursor < mEntries.size())
	{
		std::string man = mEntries[cursor].object->getSystemMetadata().manufacturer;

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
	else if (SystemData::IsManufacturerSupported && Settings::getInstance()->getString("SortSystems") == "hardware" && cursor >= 0 && cursor < mEntries.size())
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

	if (ApiSystem::getInstance()->getIpAddress() == "NOT CONNECTED")
		mWindow->pushGui(new GuiMsgBox(mWindow, _("YOU ARE NOT CONNECTED TO A NETWORK"), _("OK"), nullptr));
	else
		mWindow->pushGui(new GuiNetPlay(mWindow));
}

SystemData* SystemView::getSelected()
{
	return mCarousel.getSelected();
	//return dynamic_cast<SystemData*>(mCarousel.getActiveObject());
}

bool SystemView::input(InputConfig* config, Input input)
{
	if (mYButton.isShortPressed(config, input))
	{
		showQuickSearch();
		return true;
	}

	// Move selection fast using group (manufacturers)
	if (mCarousel.isCarousel() || mCarousel.isTextList())
	{
		if (input.value != 0)
		{
			auto mCursor = mCarousel.getCursorIndex();

			auto carousel = mCarousel.asCarousel();
			if (carousel != nullptr && carousel->isHorizontalCarousel())
			{
				if (config->isMappedLike("left", input) || config->isMappedLike("l2", input))
				{
					mCarousel.moveSelectionBy(-1);
					return true;
				}
				if (config->isMappedLike("right", input) || config->isMappedLike("r2", input))
				{
					mCarousel.moveSelectionBy(1);
					return true;
				}
				if ((Settings::getInstance()->getBool("QuickSystemSelect") && config->isMappedLike("down", input)) || config->isMappedTo("pagedown", input))
				{
					int cursor = moveCursorFast(true);
					mCarousel.moveSelectionBy(cursor - mCursor);
					return true;
				}
				if ((Settings::getInstance()->getBool("QuickSystemSelect") && config->isMappedLike("up", input)) || config->isMappedTo("pageup", input))
				{
					int cursor = moveCursorFast(false);
					mCarousel.moveSelectionBy(cursor - mCursor);
					return true;
				}
			}
			else
			{
				if (config->isMappedLike("up", input) || config->isMappedLike("l2", input))
				{
					mCarousel.moveSelectionBy(-1);
					return true;
				}
				if (config->isMappedLike("down", input) || config->isMappedLike("r2", input))
				{
					mCarousel.moveSelectionBy(1);
					return true;
				}
				if ((Settings::getInstance()->getBool("QuickSystemSelect") && config->isMappedLike("right", input)) || config->isMappedTo("pagedown", input))
				{
					int cursor = moveCursorFast(true);
					mCarousel.moveSelectionBy(cursor - mCursor);
					return true;
				}
				if ((Settings::getInstance()->getBool("QuickSystemSelect") && config->isMappedLike("left", input)) || config->isMappedTo("pageup", input))
				{
					int cursor = moveCursorFast(false);
					mCarousel.moveSelectionBy(cursor - mCursor);
					return true;
				}
			}
		}
		else
		{
			if (config->isMappedLike("left", input) ||
				config->isMappedLike("right", input) ||
				config->isMappedLike("up", input) ||
				config->isMappedLike("down", input) ||
				config->isMappedLike("pagedown", input) ||
				config->isMappedLike("pageup", input) ||
				config->isMappedLike("l2", input) ||
				config->isMappedLike("r2", input))
				mCarousel.moveSelectionBy(0);
		}
	}

	if (mCarousel.input(config, input))
		return true;

	if (input.value != 0)
	{
		bool netPlay = SystemData::isNetplayActivated() && SystemConf::getInstance()->getBool("global.netplay");
		if (netPlay && config->isMappedTo("x", input))
		{
			showNetplay();
			return true;
		}

		if (config->getDeviceId() == DEVICE_KEYBOARD && input.value && input.id == SDLK_r && SDL_GetModState() & KMOD_LCTRL && Settings::getInstance()->getBool("Debug"))
		{
			LOG(LogInfo) << " Reloading all";
			ViewController::get()->reloadAll();
			return true;
		}

#ifdef _ENABLE_FILEMANAGER_
		if (UIModeController::getInstance()->isUIModeFull()) {
			if (config->getDeviceId() == DEVICE_KEYBOARD && input.value && input.id == SDLK_F1)
			{
				ApiSystem::getInstance()->launchFileManager(mWindow);
				return true;
			}
		}
#endif

		if (config->isMappedTo(BUTTON_OK, input))
		{
			mCarousel.stopScrolling();
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
			mCarousel.setCursor(SystemData::getRandomSystem());
			return true;
		}

		if (config->isMappedTo("select", input))
		{
			GuiMenu::openQuitMenu_static(mWindow, true);
			return true;
		}

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
	mCarousel.stopScrolling();

	int mCursor = mCarousel.getCursorIndex();

	GuiSettings* gs = new GuiSettings(mWindow, title, "-----");

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

			gs->getMenu().addWithDescription(mf, Utils::String::join(names, ", "), nullptr, [this, mCursor, gs, system, idx]
				{
					mCarousel.moveSelectionBy(idx - mCursor);
					mCarousel.moveSelectionBy(0);

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
	mCarousel.update(deltaTime);
	mSystemInfo.update(deltaTime);

	for (auto sb : mStaticBackgrounds)
		sb->update(deltaTime);

	for (auto& entry : mEntries)
		for (auto extra : entry.backgroundExtras)
			extra->update(deltaTime);

	GuiComponent::update(deltaTime);

	if (mYButton.isLongPressed(deltaTime))
	{
		bool netPlay = SystemData::isNetplayActivated() && SystemConf::getInstance()->getBool("global.netplay");
		if (netPlay)
			mCarousel.setCursor(SystemData::getRandomSystem());
		else
			showQuickSearch();
	}
}

void SystemView::updateExtraTextBinding()
{
	int mCursor = mCarousel.getCursorIndex();
	if (mCursor < 0 || mCursor >= mEntries.size())
		return;

	auto system = getSelected();
	if (system == nullptr)
		return;

	for (auto extra : mEntries[mCursor].backgroundExtras)
		BindingManager::updateBindings(extra, system);
}

void SystemView::onCursorChanged(const CursorState& state)
{
	if (mCarousel.size() == 0)
		return;

	int mCursor = mCarousel.getCursorIndex();

	if (AudioManager::isInitialized())
		AudioManager::getInstance()->changePlaylist(getSelected()->getTheme());

	Utils::FileSystem::preloadFileSystemCache(mEntries.at(mCursor).object->getRootFolder()->getPath(), true);

	// update help style
	updateHelpPrompts();

	float startPos = mCamOffset;

	float posMax = (float)mEntries.size();
	float target = (float)mCursor;

	// what's the shortest way to get to our target?
	// it's one of these...

	float endPos = target; // directly
	float dist = abs(endPos - startPos);

	if (abs(target + posMax - startPos) < dist)
		endPos = target + posMax; // loop around the end (0 -> max)
	if (abs(target - posMax - startPos) < dist)
		endPos = target - posMax; // loop around the start (max - 1 -> -1)

	// animate mSystemInfo's opacity (fade out, wait, fade back in)
	cancelAnimation(1);
	cancelAnimation(2);

	std::string defaultTransition = mExtraTransitionType;
	float transitionSpeed = mExtraTransitionSpeed;

	std::string transition_style = Settings::TransitionStyle();
	if (transition_style == "auto")
	{
		if (defaultTransition == "instant" || defaultTransition == "fade" || defaultTransition == "slide" || defaultTransition == "fade & slide")
			transition_style = defaultTransition;
		else
			transition_style = "slide";
	}

	bool goFast = transition_style == "instant" || mSystemInfoDelay == 0;
	const float infoStartOpacity = mSystemInfo.getOpacity() / 255.f;

	Animation* infoFadeOut = new LambdaAnimation(
		[infoStartOpacity, this](float t)
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
				else if (mSystemInfoCountOnly)
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
		setAnimation(infoFadeIn, goFast ? 0 : mSystemInfoDelay, nullptr, false, 2);
	}

	// no need to animate transition, we're not going anywhere (probably mEntries.size() == 1)
	if (endPos == mCamOffset && endPos == mExtrasCamOffset)
		return;

	if (mLastCursor == mCursor)
		return;

	// tts
	if (state == CURSOR_STOPPED)
	{
		TextToSpeech::getInstance()->say(getSelected()->getFullName());
		Scripting::fireEvent("system-selected", getSelected()->getName());
	}

	int oldCursor = mLastCursor;
	mLastCursor = mCursor;

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

			}, transitionSpeed);
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

			}, transitionSpeed);
	}
	else // instant
	{
		anim = new LambdaAnimation([this, lockExtraChanges, startPos, endPos, posMax, move_carousel](float t)
			{
				float f = Math::lerp(startPos, endPos, Math::easeOutQuint(t));
				if (f < 0) f += posMax;
				if (f >= posMax) f -= posMax;

				if (!mLockCamOffsetChanges)
					this->mCamOffset = move_carousel ? f : endPos;

				if (!lockExtraChanges)
					this->mExtrasCamOffset = endPos;

			}, move_carousel ? transitionSpeed : 1);
	}

	if (!mLockExtraChanges)
	{
		for (int i = 0; i < mEntries.size(); i++)
			if (i != oldCursor && i != mCursor)
				activateExtras(i, false);

		activateExtras(mCursor);
	}

	setAnimation(anim, 0, [this, mCursor, lockExtraChanges]
		{
			if (!lockExtraChanges)
			{
				mExtrasFadeOpacity = 0.0f;
				mExtrasFadeMove = 0.0f;
				mExtrasFadeOldCursor = -1;

				int currentCursor = mCarousel.getCursorIndex();

				for (int i = 0; i < mEntries.size(); i++)
					if (i != currentCursor)
						activateExtras(i, false);
			}
		}, false, 0);
}

void SystemView::render(const Transform4x4f& parentTrans)
{
	if (mEntries.size() == 0 || !mVisible)
		return;  // nothing to render

	Transform4x4f trans = getTransform() * parentTrans;

	auto rect = Renderer::getScreenRect(trans, mSize);
	if (!Renderer::isVisibleOnScreen(rect))
		return;

	auto carouselZindex = mCarousel.getZIndex();
	auto systemInfoZIndex = mSystemInfo.getZIndex();
	auto minMax = std::minmax(carouselZindex, systemInfoZIndex);
	
	renderExtras(trans, INT16_MIN, minMax.first);

	for (auto sb : mStaticBackgrounds)
		sb->render(trans);

	if (mCarousel.getZIndex() > mSystemInfo.getZIndex())
		renderInfoBar(trans);	
	else
		renderCarousel(trans);

	renderExtras(trans, minMax.first, minMax.second);

	if (mCarousel.getZIndex() > mSystemInfo.getZIndex())
		renderCarousel(trans);	
	else
		renderInfoBar(trans);

	renderExtras(trans, minMax.second, INT16_MAX);
}

std::vector<HelpPrompt> SystemView::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mCarousel.getHelpPrompts();

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
	int mCursor = mCarousel.getCursorIndex();

	HelpStyle style;
	
	if (mEntries.size())
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

	const ThemeData::ThemeElement* textListElem = theme->getElement("system", "textlist", "textlist");
	if (textListElem)
	{
		if (!mCarousel.isTextList())
		{
			auto textListNative = new TextListComponent<SystemData*>(mWindow);
			textListNative->setCursorChangedCallback([this](CursorState state) { onCursorChanged(state); });
			mCarousel.attach(textListNative);
		}
		
		getDefaultElements();
		mCarousel.applyTheme(theme, "system", "textlist", ThemeFlags::ALL);
	}
	else
	{
		const ThemeData::ThemeElement* imageGridElem = theme->getElement("system", "imagegrid", "imagegrid");
		if (imageGridElem)
		{
			if (!mCarousel.isGrid())
			{
				auto imageGridNative = new ImageGridComponent<SystemData*>(mWindow);
				imageGridNative->setThemeName("system");
				imageGridNative->setCursorChangedCallback([this](CursorState state) { onCursorChanged(state); });
				mCarousel.attach(imageGridNative);				
			}

			getDefaultElements();

			mCarousel.applyTheme(theme, "system", "imagegrid", ThemeFlags::ALL);
		}
		else
		{
			if (!mCarousel.isCarousel())
			{
				auto carouselNative = new CarouselComponent(mWindow);
				carouselNative->setDefaultBackground(0xFFFFFFD8, 0xFFFFFFD8, true);
				carouselNative->setThemedContext("logo", "logoText", "systemcarousel", "carousel", CarouselType::HORIZONTAL, CarouselImageSource::IMAGE);
				carouselNative->setCursorChangedCallback([this](CursorState state) { onCursorChanged(state); });
				
				mCarousel.attach(carouselNative);
			}
						
			getDefaultElements();

			mCarousel.applyTheme(theme, "system", "systemcarousel", ThemeFlags::ALL);

			const ThemeData::ThemeElement* carouselElem = theme->getElement("system", "systemcarousel", "carousel");
			if (carouselElem)
				getCarouselFromTheme(carouselElem);
		}
	}

	// load <system name="system" extraTransition="" extraTransitionSpeed="" extraTransitionHorizontal=""> attributes overrides
	auto carousel = mCarousel.asCarousel();
	mExtraTransitionType = carousel ? carousel->getDefaultTransition() : "fade";
	mExtraTransitionSpeed = carousel ? carousel->getTransitionSpeed() : 350.0f;
	mExtraTransitionHorizontal = carousel ? carousel->isHorizontalCarousel() : false;
	
	auto view = theme->getView("system");
	if (view != nullptr)
	{
		if (!view->extraTransition.empty())
			mExtraTransitionType = view->extraTransition;

		if (view->extraTransitionSpeed >= 0.0f)
			mExtraTransitionSpeed = view->extraTransitionSpeed;

		if (!view->extraTransitionDirection.empty())
			mExtraTransitionHorizontal = view->extraTransitionDirection != "vertical";
	}

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
	mCarousel.render(trans);
}

void SystemView::renderInfoBar(const Transform4x4f& trans)
{
	Renderer::setMatrix(trans);
	mSystemInfo.render(trans);
}

void SystemView::setExtraRequired(SystemViewData& data, bool required)
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
	for (GuiComponent* extra : data.backgroundExtras)
		setTexture(extra, [required](std::shared_ptr<TextureResource> x) { x->setRequired(required); });
}

void SystemView::ensureTexture(GuiComponent* extra, bool reload)
{
	if (extra == nullptr)
		return;

	ImageComponent* image = dynamic_cast<ImageComponent*>(extra);
	if (image != nullptr)
	{
		auto tex = image->getTexture();
		if (tex == nullptr)
			return;

		if (reload)
			tex->reload();
		else
			tex->prioritize();
	}

	for (auto child : extra->enumerateExtraChildrens())
	{
		image = dynamic_cast<ImageComponent*>(child);
		if (image != nullptr)
		{
			auto tex = image->getTexture();
			if (tex == nullptr)
				return;

			if (reload)
				tex->reload();
			else
				tex->prioritize();
		}
	}
};

void SystemView::preloadExtraNeighbours(int cursor)
{
	// Make sure near textures are in at top position & will be released last if VRAM is required
	int distancesStatic[] = { -2, 2, -1, 1, 0 };
	int distancesRight[] = { 3, -1, 2, 1, 0 };
	int distancesLeft[] = { -3, 1, -2, -1, 0 };

	int* distances = &distancesStatic[0];

	if (mCarousel.getScrollingVelocity() > 0)
		distances = &distancesRight[0];
	else if (mCarousel.getScrollingVelocity() < 0)
		distances = &distancesLeft[0];

	for (int dx = 0; dx < 5; dx++)
	{
		int i = cursor + distances[dx];
		int index = i % (int)mEntries.size();
		if (index < 0)
			index += (int)mEntries.size();

		SystemViewData& entry = mEntries.at(index);

		auto carousel = mCarousel.asCarousel();
		if (carousel)
		{
			auto logo = carousel->getLogo(dx);
			if (logo)
				ensureTexture(logo.get(), dx > 1);
		}

		for (auto extra : entry.backgroundExtras)
			ensureTexture(extra, dx > 1);
	}
}

// Draw background extras
void SystemView::renderExtras(const Transform4x4f& trans, float lower, float upper)
{
	int mCursor = mCarousel.getCursorIndex();

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
			for (GuiComponent* extra : mEntries.at(mCursor).backgroundExtras)
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

			for (GuiComponent* extra : mEntries.at(mExtrasFadeOldCursor).backgroundExtras)
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
		for (GuiComponent* extra : mEntries.at(mExtrasFadeOldCursor).backgroundExtras)
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
			}
		}

		Renderer::popClipRect();
	}

	for (int i = min; i <= max; i++)
	{
		int index = i % (int)mEntries.size();
		if (index < 0)
			index += (int)mEntries.size();

		if (mExtrasFadeOpacity && (index == mExtrasFadeOldCursor || index != mCursor))
			continue;

		// Only render selected system when not showing
		if (!isShowing() && index != mCursor)
			continue;

		Vector2i size = Vector2i(Math::round(mSize.x()), Math::round(mSize.y()));

		Transform4x4f extrasTrans = trans;
		if (mExtraTransitionHorizontal) //.type == HORIZONTAL || mCarousel.type == HORIZONTAL_WHEEL)
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

		for (GuiComponent* extra : mEntries.at(index).backgroundExtras)
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

					if (mExtraTransitionHorizontal)
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
void SystemView::getDefaultElements()
{
	// Transitions
	mExtraTransitionType = "";
	mExtraTransitionSpeed = 500.0f;
	mExtraTransitionHorizontal = true;

	// Carousel
	mCarousel.setZIndex(40);
	mCarousel.setDefaultZIndex(40);
	mCarousel.setSize(mSize.x(), 0.2325f * mSize.y());
	mCarousel.setPosition(0.0f, 0.5f * (mSize.y() - mCarousel.getSize().y()));

	auto carousel = mCarousel.asCarousel();
	if (carousel)
		carousel->setDefaultBackground(0xFFFFFFD8, 0xFFFFFFD8, true);

	// System Info Bar
	mSystemInfo.setSize(mSize.x(), mSystemInfo.getFont()->getLetterHeight() * 2.2f);
	mSystemInfo.setPosition(0, (mCarousel.getPosition().y() + mCarousel.getSize().y() - 0.2f));
	mSystemInfo.setBackgroundColor(0xDDDDDDD8);
	mSystemInfo.setRenderBackground(true);
	mSystemInfo.setFont(Font::get((int)(0.035f * mSize.y()), Font::getDefaultPath()));
	mSystemInfo.setColor(0x000000FF);
	mSystemInfo.setZIndex(50);
	mSystemInfo.setDefaultZIndex(50);

	for (auto sb : mStaticBackgrounds)
		delete sb;

	mStaticBackgrounds.clear();
}

void SystemView::getCarouselFromTheme(const ThemeData::ThemeElement* elem)
{
	if (elem->has("systemInfoDelay"))
		mSystemInfoDelay = elem->get<float>("systemInfoDelay");

	if (elem->has("systemInfoCountOnly"))
		mSystemInfoCountOnly = elem->get<bool>("systemInfoCountOnly");
}

void SystemView::onShow()
{
	GuiComponent::onShow();

	mCarousel.onShow();

	activateExtras(mCarousel.getCursorIndex());

	for (auto sb : mStaticBackgrounds)
		sb->onShow();

	if (getSelected() != nullptr)
		TextToSpeech::getInstance()->say(getSelected()->getFullName());
}

void SystemView::onHide()
{
	GuiComponent::onHide();

	mCarousel.onHide();

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
	for (auto& entry : mEntries)
		for (auto extra : entry.backgroundExtras)
			func(extra);
}

void SystemView::activateExtras(int cursor, bool activate)
{
	if (cursor < 0 || cursor >= mEntries.size())
		return;

	bool show = activate && isShowing() && !mScreensaverActive && !mDisable;

	SystemViewData& data = mEntries.at(cursor);
	for (unsigned int j = 0; j < data.backgroundExtras.size(); j++)
	{
		GuiComponent* extra = data.backgroundExtras[j];

		if (show && activate)
			extra->onShow();
		else
			extra->onHide();
	}

	setExtraRequired(data, activate);

	if (activate)
		preloadExtraNeighbours(cursor);
}

SystemData* SystemView::getActiveSystem()
{
	int mCursor = mCarousel.getCursorIndex();
	if (mCursor < 0 || mCursor >= mEntries.size())
		return nullptr;

	return mEntries[mCursor].object;
}

bool SystemView::hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult)
{
	bool ret = GuiComponent::hitTest(x, y, parentTransform, pResult);

	int mCursor = mCarousel.getCursorIndex();
	if (mCursor < 0 || mCursor >= mEntries.size())
		return ret;

	Transform4x4f trans = getTransform() * parentTransform;

	for (auto extra : mEntries[mCursor].backgroundExtras)
		ret |= extra->hitTest(x, y, trans, pResult);

	ret |= mCarousel.hitTest(x, y, trans, pResult);
	
	return ret;
}

bool SystemView::onMouseClick(int button, bool pressed, int x, int y)
{
	if (mCarousel.onMouseClick(button, pressed, x, y))
		return true;

	if (button == 2 && pressed)
		showQuickSearch();
	else if (button == 3 && pressed)
		showNavigationBar();

	return true;
}

void SystemView::onMouseMove(int x, int y)
{
	mCarousel.onMouseMove(x, y);
}

bool SystemView::onMouseWheel(int delta)
{
	return mCarousel.onMouseWheel(delta);
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
		mCarousel.stopScrolling();
		ViewController::get()->goToGameList(getSelected());
		return true;
	}

	if (action == "random")
	{
		mCarousel.setCursor(SystemData::getRandomSystem());
		return true;
	}

	if (action == "prev")
	{
		mCarousel.moveSelectionBy(-1);
		return true;
	}

	if (action == "next")
	{
		mCarousel.moveSelectionBy(1);
		return true;
	}

	if (action == "cheevos")
	{
		if (SystemConf::getInstance()->getBool("global.retroachievements") && !Settings::getInstance()->getBool("RetroachievementsMenuitem") && SystemConf::getInstance()->get("global.retroachievements.username") != "")
		{
			if (ApiSystem::getInstance()->getIpAddress() == "NOT CONNECTED")
				mWindow->pushGui(new GuiMsgBox(mWindow, _("YOU ARE NOT CONNECTED TO A NETWORK"), _("OK"), nullptr));
			else
				GuiRetroAchievements::show(mWindow);
		}

		return true;
	}

	return false;
}

int SystemView::getCursorIndex()
{
	return mCarousel.getCursorIndex();
}

std::vector<SystemData*> SystemView::getObjects()
{
	return mCarousel.getObjects();
}
