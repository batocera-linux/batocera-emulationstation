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
#include "resources/TextureResource.h"
#include "guis/GuiTextEditPopup.h"
#include "guis/GuiTextEditPopupKeyboard.h"
#include "TextToSpeech.h"
#include "BindingManager.h"
#include "guis/GuiRetroAchievements.h"
#include "components/CarouselComponent.h"

namespace
{
	constexpr int EXTRA_LOAD_PADDING = 2;
	constexpr int EXTRA_RETAIN_PADDING = 4;

	bool canReleaseExtraTree(GuiComponent* component, bool restorable)
	{
		if (component->isShowing())
			return false;

		if (!restorable)
		{
			if (auto video = dynamic_cast<VideoComponent*>(component))
			{
				if (video->getPlaylist() != nullptr)
					return false;
			}

			if (auto image = dynamic_cast<ImageComponent*>(component))
			{
				if (image->getPlaylist() != nullptr)
					return false;
			}
		}

		for (unsigned int i = 0; i < component->getChildCount(); i++)
		{
			auto child = component->getChild(i);
			bool childRestorable = restorable && child->getExtraType() == ExtraType::EXTRACHILDREN;

			if (!canReleaseExtraTree(child, childRestorable))
				return false;
		}

		return true;
	}

	template <typename Visitor>
	void visitThemeExtraTree(
		GuiComponent* component,
		std::vector<std::string>& path,
		const Visitor& visitor)
	{
		path.push_back(component->getTag());
		visitor(component, path);

		for (unsigned int i = 0; i < component->getChildCount(); i++)
		{
			auto child = component->getChild(i);
			if (child->getExtraType() == ExtraType::EXTRACHILDREN)
			{
				path.push_back(std::to_string(i));
				visitThemeExtraTree(child, path, visitor);
				path.pop_back();
			}
		}

		path.pop_back();
	}

	bool restoreExtraPlaylist(
		GuiComponent* component,
		const SystemViewPlaylistState& state)
	{
		if (auto video = dynamic_cast<VideoComponent*>(component))
		{
			video->setPlaylist(state.playlist, false);
			video->setVideo(state.path);
			return true;
		}

		if (auto image = dynamic_cast<ImageComponent*>(component))
		{
			image->setPlaylist(state.playlist, false);
			image->setImage(state.path, state.tiled, MaxSizeInfo::Empty, true, false);
			return true;
		}

		return false;
	}
}

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
	mExtraResidency.invalidate();

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
	mExtraResidency.invalidate();

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

	auto& entry = mEntries[idx];
	releaseExtras(entry);
	entry.playlists.clear();
	if (idx == mCarousel.getCursorIndex())
	{
		updateExtraTextBinding();
		activateExtras(idx);
	}
	SystemRandomPlaylist::resetCache();
}

void SystemView::saveExtraPlaylists(SystemViewData& entry)
{
	if (!entry.extrasLoaded)
		return;

	entry.playlists.clear();

	auto save = [&entry](GuiComponent* extra, const std::vector<std::string>& path)
	{
		SystemViewPlaylistState state;

		if (extra->isKindOf<VideoComponent>())
		{
			auto video = static_cast<VideoComponent*>(extra);
			state.playlist = video->getPlaylist();
			state.path = video->getPlaylistMediaPath();
		}
		else if (extra->isKindOf<ImageComponent>())
		{
			auto image = static_cast<ImageComponent*>(extra);
			state.playlist = image->getPlaylist();
			state.path = image->getPlaylistMediaPath();
			state.tiled = image->isTiled();
		}

		if (state.playlist != nullptr)
			entry.playlists.emplace(path, std::move(state));
	};

	std::vector<std::string> path;
	for (auto extra : entry.backgroundExtras)
		visitThemeExtraTree(extra, path, save);
}

void SystemView::ensureExtras(int index)
{
	if (index < 0 || index >= static_cast<int>(mEntries.size()))
		return;

	if (mEntries[index].extrasLoaded)
		return;

	auto& entry = mEntries[index];
	loadExtras(entry);

	for (auto extra : entry.backgroundExtras)
		BindingManager::updateBindings(extra, entry.object);
}

void SystemView::prepareExtras(int index, bool prioritize)
{
	if (index < 0 || index >= static_cast<int>(mEntries.size()))
		return;

	ensureExtras(index);

	for (auto extra : mEntries[index].backgroundExtras)
		extra->preloadArtwork(prioritize);
}

void SystemView::loadExtras(SystemViewData& entry)
{
	auto system = entry.object;

	// make background extras
	auto extras = ThemeData::makeExtras(system->getTheme(), "system", mWindow);

	if (!entry.playlists.empty())
	{
		auto restore = [&entry](GuiComponent* extra, const std::vector<std::string>& path)
		{
			// The loop below restores the top-level extras.
			if (path.size() == 1)
				return;

			auto saved = entry.playlists.find(path);
			if (saved != entry.playlists.end())
				restoreExtraPlaylist(extra, saved->second);
		};

		std::vector<std::string> path;
		for (auto extra : extras)
			visitThemeExtraTree(extra, path, restore);
	}

	for (auto extra : extras)
	{
		if (!entry.playlists.empty())
		{
			auto saved = entry.playlists.find(std::vector<std::string>{extra->getTag()});

			if (saved != entry.playlists.end() && restoreExtraPlaylist(extra, saved->second))
				continue;
		}

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

	entry.backgroundExtras = std::move(extras);
	entry.extrasLoaded = true;
	entry.playlists.clear();

	SystemRandomPlaylist::resetCache();
}


void SystemView::populate()
{
	TextureResource::setLoadingPaused(true);

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
			SystemViewData entry;
			entry.object = system;
			mEntries.push_back(std::move(entry));

			mCarousel.add(system->getName(), system, false);
		}
	}

	TextureResource::setLoadingPaused(false);

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
}

void SystemView::prepareHiddenSystem(SystemData* system)
{
	if (isShowing() || system == nullptr || mEntries.empty())
		return;

	if (!mCarousel.supportsHiddenArtwork())
	{
		releaseHiddenExtras();
		return;
	}

	if (getSelected() != system || mCarousel.getScrollingVelocity() != 0)
	{
		if (!mCarousel.setHiddenCursor(system))
			return;
	}

	if (!mCarousel.prepareHiddenArtwork())
		return;

	const int cursor = mCarousel.getCursorIndex();

	mLastCursor = cursor;
	mCamOffset = static_cast<float>(cursor);
	mExtrasCamOffset = static_cast<float>(cursor);
	mExtrasFadeOpacity = 0.0f;
	mExtrasFadeMove = 0.0f;
	mExtrasFadeOldCursor = -1;

	updateExtraResidency(ExtraResidencyMode::HiddenPrepared, true);
}

void SystemView::releaseHiddenExtras()
{
	updateExtraResidency(ExtraResidencyMode::Hidden);
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
	else if (SystemData::IsManufacturerSupported && (Settings::getInstance()->getString("SortSystems") == "hardware"
			|| Settings::getInstance()->getString("SortSystems") == "hardware-year") && cursor >= 0 && cursor < mEntries.size())
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
		// Quick search needs the "all games" collection : when the user disabled it, fall back
		// to a random system instead of silently doing nothing.
		if (SystemData::getSystem("all") != nullptr)
			showQuickSearch();
		else
			mCarousel.setCursor(SystemData::getRandomSystem());

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

	auto grid = mCarousel.asGrid();
	if (grid && !grid->getGroupBy().empty())
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
	else if (sortMode == "hardware" || sortMode == "hardware-year")
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

	GuiSettings* gs = new GuiSettings(mWindow, title, "-----", nullptr, false, false);

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

void SystemView::collectExtraUpdateEntries(std::unordered_set<int>& entries)
{
	entries.clear();

	if (mEntries.empty())
		return;

	const int entryCount = (int)mEntries.size();

	auto addEntry = [&entries, entryCount](int index)
	{
		index %= entryCount;
		if (index < 0)
			index += entryCount;

		entries.insert(index);
	};

	const int cursor = mCarousel.getCursorIndex();
	if (cursor >= 0 && cursor < entryCount)
		addEntry(cursor);

	addEntry((int)mExtrasCamOffset);
	addEntry((int)(mExtrasCamOffset + 0.99999f));

	if (mExtrasFadeOldCursor >= 0 && mExtrasFadeOldCursor < entryCount)
		addEntry(mExtrasFadeOldCursor);
}

void SystemView::collectExtraResidencyEntries(
	std::unordered_set<int>& entries, int padding)
{
	collectExtraUpdateEntries(entries);

	if (mEntries.empty())
		return;

	const int entryCount = (int)mEntries.size();

	const std::vector<int> activeEntries(entries.begin(), entries.end());
	const int distance = Math::min(Math::max(0, padding), entryCount - 1);

	auto grid = mCarousel.asGrid();
	for (int index : activeEntries)
	{
		const int position = grid ? grid->getDisplayIndex(index) : index;
		if (position < 0)
			continue;

		for (int offset = 1; offset <= distance; offset++)
		{
			const int next = (position + offset) % entryCount;
			const int previous = (position - offset + entryCount) % entryCount;
			entries.insert(grid ? grid->getSourceIndex(next) : next);
			entries.insert(grid ? grid->getSourceIndex(previous) : previous);
		}
	}
}

void SystemView::ensureWarmExtras()
{
	std::unordered_set<int> entries;
	collectExtraResidencyEntries(entries, EXTRA_LOAD_PADDING);

	const int cursor = mCarousel.getCursorIndex();

	prepareExtras(cursor, true);

	for (int index : entries)
	{
		if (index != cursor)
			prepareExtras(index, false);
	}
}

void SystemView::releaseExtras(SystemViewData& entry)
{
	saveExtraPlaylists(entry);
	setExtraRequired(entry, false);

	for (auto extra : entry.backgroundExtras)
		delete extra;

	std::vector<GuiComponent*>().swap(entry.backgroundExtras);
	entry.extrasLoaded = false;
}

void SystemView::updateExtraResidency(ExtraResidencyMode mode, bool allowRelease)
{
	if (mEntries.empty())
	{
		mExtraResidency.invalidate();
		return;
	}

	const bool navigation = mode == ExtraResidencyMode::Navigation;
	if (navigation)
	{
		if (!isShowing() || mScreensaverActive || mDisable || mLockExtraChanges || mLockCamOffsetChanges)
			return;
	}
	else if (isShowing())
		return;

	const auto key = std::make_tuple(
		static_cast<int>(mEntries.size()),
		mCarousel.getCursorIndex(),
		static_cast<int>(mExtrasCamOffset),
		static_cast<int>(mExtrasCamOffset + 0.99999f),
		mExtrasFadeOldCursor,
		mode);

	const auto work = mExtraResidency.workFor(key);
	if (work == ExtraTracker::Work::None)
		return;

	if (work == ExtraTracker::Work::Prepare)
	{
		if (navigation)
			ensureWarmExtras();
		else if (mode == ExtraResidencyMode::HiddenPrepared)
			prepareExtras(mCarousel.getCursorIndex(), false);
		else
			mCarousel.releaseHiddenArtwork();

		mExtraResidency.complete(key, true);
	}

	std::unordered_set<int> retainEntries;
	if (mode != ExtraResidencyMode::Hidden)
	{
		if (!allowRelease ||
			isAnimationPlaying(0) ||
			mCarousel.getScrollingVelocity() != 0 ||
			!TextureResource::isLoadingIdle())
		{
			return;
		}

		collectExtraResidencyEntries(retainEntries, EXTRA_RETAIN_PADDING);
	}

	for (int index = 0; index < static_cast<int>(mEntries.size()); ++index)
	{
		auto& entry = mEntries[index];

		if (!entry.extrasLoaded || entry.backgroundExtras.empty() ||
			retainEntries.count(index) != 0)
		{
			continue;
		}

		bool canRelease = true;
		for (auto extra : entry.backgroundExtras)
		{
			if (!canReleaseExtraTree(extra, true))
			{
				canRelease = false;
				break;
			}
		}

		if (!canRelease)
			continue;

		releaseExtras(entry);
		return;
	}

	mExtraResidency.complete(key, false);
}

void SystemView::update(int deltaTime)
{
	mCarousel.update(deltaTime);
	mSystemInfo.update(deltaTime);

	for (auto sb : mStaticBackgrounds)
		sb->update(deltaTime);

	GuiComponent::update(deltaTime);

	if (isShowing() && !mEntries.empty())
	{
		std::unordered_set<int> updateEntries;
		collectExtraUpdateEntries(updateEntries);

		for (int index : updateEntries)
		{
			ensureExtras(index);

			for (auto extra : mEntries[index].backgroundExtras)
				extra->update(deltaTime);
		}
	}

	// Long press on the west button always selects a random system, matching the game list view.
	// It used to do so only when netplay was enabled, and otherwise just repeated the quick
	// search already bound to the short press.
	if (mYButton.isLongPressed(deltaTime))
		mCarousel.setCursor(SystemData::getRandomSystem());

	updateExtraResidency(ExtraResidencyMode::Navigation);
}

void SystemView::updateExtraTextBinding()
{
	int mCursor = mCarousel.getCursorIndex();
	if (mCursor < 0 || mCursor >= mEntries.size())
		return;

	auto system = getSelected();
	if (system == nullptr)
		return;

	ensureExtras(mCursor);
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
	{
		mLastCursor = mCursor;
		return;
	}

	// tts
	if (state == CURSOR_STOPPED)
	{
		TextToSpeech::getInstance()->say(getSelected()->getFullName());
		Scripting::fireEvent("system-selected", getSelected()->getName());
	}

	if (mLastCursor == mCursor)
		return;

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

	stopAnimation(0);
	mWindow->normalizeNextUpdate();

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

	const bool loaderWasIdle = TextureResource::isLoadingIdle();

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

	updateExtraResidency(ExtraResidencyMode::Navigation, loaderWasIdle);
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
			prompts.push_back(HelpPrompt("y", _("SEARCH") + std::string("/") + _("RANDOM"), [&] { showQuickSearch(); })); // QUICK 
		else
			prompts.push_back(HelpPrompt("y", _("RANDOM"), [&] { mCarousel.setCursor(SystemData::getRandomSystem()); }));
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
	for (auto extra : data.backgroundExtras)
	{
		if (!extra->isKindOf<ImageComponent>())
			continue;

		auto image = static_cast<ImageComponent*>(extra);
		auto texture = image->getTexture();

		if (texture != nullptr)
			texture->setRequired(required, image);
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

		ensureExtras(index);

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
	mExtraResidency.invalidate();

	GuiComponent::onShow();
	mCarousel.onShow();

	activateExtras(mCarousel.getCursorIndex());
	updateExtraResidency(ExtraResidencyMode::Navigation, false);

	for (auto sb : mStaticBackgrounds)
		sb->onShow();

	if (getSelected() != nullptr)
		TextToSpeech::getInstance()->say(getSelected()->getFullName());
}

void SystemView::onHide()
{
	mExtraResidency.invalidate();

	finishAnimation(0);
	cancelAnimation(1);
	cancelAnimation(2);

	GuiComponent::onHide();

	mCarousel.onHide();

	updateExtras([](GuiComponent* p) { p->onHide(); });

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

	if (activate)
		ensureExtras(cursor);

	bool show = activate && isShowing() && !mScreensaverActive && !mDisable;

	SystemViewData& data = mEntries.at(cursor);
	for (unsigned int j = 0; j < data.backgroundExtras.size(); j++)
	{
		GuiComponent* extra = data.backgroundExtras[j];

		if (show && activate)
		{
			extra->onShow();
		}
		else
			extra->onHide();
	}

	setExtraRequired(data, show);

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
