#include "guis/GuiRommSettings.h"

#include "romm/RommAuth.h"
#include "romm/RommSync.h"
#include "romm/RommCatalog.h"
#include "romm/RommCollections.h"
#include "romm/RommBios.h"
#include "romm/RommCache.h"
#include "romm/RommMediaCache.h"
#include "romm/RommDownloader.h"
#include "romm/RommMedia.h"
#include "romm/RommPlaySessions.h"
#include "ApiSystem.h"
#include "guis/GuiLoading.h"
#include "guis/GuiMenu.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiRommPairing.h"
#include "components/OptionListComponent.h"
#include "guis/GuiRommDownload.h"
#include "SystemData.h"
#include "FileData.h"
#include "guis/GuiTextEditPopupKeyboard.h"
#include "components/SwitchComponent.h"
#include "components/TextComponent.h"
#include "views/ViewController.h"
#include "LocaleES.h"
#include "SystemConf.h"
#include "ThemeData.h"
#include "Window.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"

#include <algorithm>
#include <set>
#include <vector>
#include <cstdio>
#include <memory>
#include <string>

using namespace Romm;


// The library ES would show for this configuration (snapshot present and usable)
static bool hasLibrarySnapshot(const RommConfig& config)
{
	if (!config.isUsable())
		return false;

	std::string libraryDir = getLibraryDir(config.baseUrl());
	if (!Utils::FileSystem::isDirectory(libraryDir))
		return false;

	for (auto file : Utils::FileSystem::getDirContent(libraryDir, false, false))
		if (Utils::String::toLower(Utils::FileSystem::getExtension(file)) == ".xml")
			return true;

	return false;
}

void GuiRommSettings::open(Window* window)
{
	window->pushGui(new GuiRommSettings(window));
}

void GuiRommSettings::onIntegrationToggled(Window* window)
{
	RommConfig config = RommConfig::load();
	if (config.isUsable())
		RommSync::requestSync(false);

	// RomM games appear or disappear : reload the game lists (this also closes the menus)
	if (hasLibrarySnapshot(config) != RommCatalog::isActive())
	{
		ViewController::reloadAllGames(window, false);
		return;
	}

	// Otherwise only the main menu changes : rebuild it so the RomM entry appears / disappears
	GuiComponent* top = window->peekGui();
	if (dynamic_cast<GuiMenu*>(top) != nullptr)
	{
		delete top;
		window->pushGui(new GuiMenu(window, false));
	}
}

static std::string accountText(const RommConfig& config)
{
	if (!config.isPaired())
		return _("NOT PAIRED");

	return config.username.empty() ? _("PAIRED") : _("PAIRED AS") + " " + config.username;
}

GuiRommSettings::GuiRommSettings(Window* window) : GuiSettings(window, _("ROMM").c_str())
{
	RommConfig config = RommConfig::load();
	mWasEnabled = config.enabled;
	mWasPaired = config.isPaired();

	auto theme = ThemeData::getMenuTheme();

	// Before pairing: setting up the server and the account is all there is to do. Once paired, they
	// are in SERVER & ACCOUNT, and the library comes first.
	if (!config.isPaired())
	{
		addGroup(_("SERVER"));

		addInputTextConfigRow(_("SERVER ADDRESS"), CONF_URL, false);

		mInsecure = std::make_shared<SwitchComponent>(mWindow);
		mInsecure->setState(config.allowSelfSigned);
		addWithDescription(_("ALLOW SELF-SIGNED CERTIFICATE"), _("Only for HTTPS servers using a certificate you created yourself."), mInsecure);

		addGroup(_("ACCOUNT"));

		addWithLabel(_("STATUS"), std::make_shared<TextComponent>(mWindow, accountText(config), theme->Text.font, theme->Text.color));

		addEntry(_("PAIR THIS DEVICE"), true, [this] { pairDevice(); });
		addEntry(_("ENTER A PAIRING CODE"), true, [this] { enterPairingCode(); });
	}

	if (config.isPaired())
	{
		addGroup(_("LIBRARY"));
		// While a synchronization runs, OK on SYNC STATUS: pause / unpause / cancel it
		std::string status = RommSync::isRunning() ? (RommSync::isPaused() ? _("PAUSED") : _("SYNCHRONIZING...")) : RommSync::getStatus();
		if (!status.empty())
		{
			auto statusText = std::make_shared<TextComponent>(mWindow, status, theme->TextSmall.font, theme->Text.color);
			addWithLabel(_("SYNC STATUS"), statusText, false, [this, statusText] { openSyncActions(statusText); });
		}

		addEntry(_("SYNCHRONIZE NOW"), false, [this]
		{
			if (RommSync::requestSync(false))
				mWindow->displayNotificationMessage(_("ROMM SYNCHRONIZATION STARTED"));
			else
				mWindow->displayNotificationMessage(_("ROMM SYNCHRONIZATION ALREADY RUNNING"));
		});

		addEntry(_("APPLY LIBRARY CHANGES"), false, [this]
		{
			Window* window = mWindow;
			close(); // saves the switches
			RommSync::clearPendingChanges();
			window->postToUiThread([window] { ViewController::reloadAllGames(window, false); });
		});
		addEntry(_("SYSTEMS"), true, [this] { openSystems(); });
		addEntry(_("COLLECTIONS"), true, [this] { openCollections(); });

		if (ApiSystem::getInstance()->isScriptingSupported(ApiSystem::ScriptId::BIOSINFORMATION))
		{
			addEntry(_("DOWNLOAD MISSING BIOS"), false, [this]
			{
				Window* window = mWindow;
				window->pushGui(new GuiLoading<BiosDownloadResult>(window, _("CHECKING BIOS FILES..."),
					[](IGuiLoadingHandler* gui)
					{
						return downloadMissingBios(findMissingBios(), [gui](const std::string& file)
						{
							gui->setText(_("BIOS FROM ROMM") + "\r\n" + file);
						});
					},
					[window](BiosDownloadResult result)
					{
						std::string text = _("BIOS FILES DOWNLOADED FROM ROMM") + ": " + std::to_string(result.downloaded) + "\n" +
							_("MISSING BIOS FILES NOT ON ROMM") + ": " + std::to_string(result.notAvailable);

						if (result.failed > 0)
							text += "\n" + _("FAILED DOWNLOADS") + ": " + std::to_string(result.failed);

						window->pushGui(new GuiMsgBox(window, text));
					}));
			});
		}

		addGroup(_("SETTINGS"));
		addWithDescription(_("LIBRARY MANAGEMENT"), _("Media, favorites, play time, cache"), makeArrow(mWindow), [this] { openLibraryManagement(); });
		addWithDescription(_("SERVER & ACCOUNT"), accountText(config), makeArrow(mWindow), [this] { openServerAccount(); });
	}

	addSaveFunc([this] { if (mInsecure != nullptr) SystemConf::getInstance()->setBool(CONF_INSECURE, mInsecure->getState()); });

	onFinalize([this]
	{
		RommConfig after = RommConfig::load();

		if (after.isUsable() && (!mWasEnabled || !mWasPaired))
			RommSync::requestSync(false);

		// Reload only when the games ES would show differ from what is loaded now
		bool hasSnapshot = hasLibrarySnapshot(after);

		if (hasSnapshot != RommCatalog::isActive())
		{
			Window* window = mWindow;
			window->postToUiThread([window] { ViewController::reloadAllGames(window, false); });
		}
	});
}

void GuiRommSettings::openLibraryManagement()
{
	// What the library holds and how: media, favorites, play time, cache, maintenance
	Window* window = mWindow;
	RommConfig config = RommConfig::load();
	auto gui = new GuiSettings(window, _("LIBRARY MANAGEMENT").c_str());

	gui->addGroup(_("MEDIA"));
	// Media: which ones are cached from the library, which ones come with a downloaded rom
	gui->addEntry(_("SYNC FROM LIBRARY"), true, [this] { openMediaSelection(true); });
	gui->addWithDescription(_("DOWNLOAD ALONG WITH ROM"), _("Besides the media synced from the library"), makeArrow(window), [this] { openMediaSelection(false); });

	// IMAGE SHOWN : which media is the image ES shows (fetched even when not selected above)
	{
		auto imageList = std::make_shared<OptionListComponent<std::string>>(window, _("IMAGE SHOWN"), false);
		imageList->add(_("AUTOMATIC"), IMAGE_AUTOMATIC, config.image == IMAGE_AUTOMATIC);

		bool found = config.image == IMAGE_AUTOMATIC;
		for (auto& m : allMedia())
		{
			if (!m.canBeImage)
				continue;

			imageList->add(_(m.label), m.key, config.image == m.key);
			found = found || config.image == m.key;
		}

		if (!found)
			imageList->selectFirstItem();

		gui->addWithDescription(_("IMAGE SHOWN"), _("Automatic: the cover, else the large cover, a screenshot, the title screen..."), imageList);

		std::string before = config.image;
		gui->addSaveFunc([window, imageList, before]
		{
			std::string image = imageList->getSelected();
			if (image == before)
				return;

			SystemConf::getInstance()->set(CONF_IMAGE, image);
			RommSync::applyMediaSettings();

			if (RommConfig::load().isUsable())
				RommSync::requestSync(false); // fetches the chosen media

			window->postToUiThread([window] { ViewController::reloadAllGames(window, false); });
		});
	}

	gui->addGroup(_("FAVORITES & PLAY TIME"));
	// romm.favorites : OFF when absent. Applied when the game lists load.
	gui->addSwitch(_("SYNC FAVORITES"), _("Games in your RomM favorites are Batocera favorites."), CONF_FAVORITES, false, [window]
	{
		window->postToUiThread([window] { ViewController::reloadAllGames(window, false); });
	});

	// romm.playsessions : OFF when absent. Needs the roms.user.write scope (granted at pairing).
	std::string playDescription = _("Send the play time of RomM games to RomM.");
	if (!config.scopes.empty() && !config.hasScope("roms.user.write"))
		playDescription = _("Pair this device again to allow it.");
	else if (RommPlaySessions::missingPermission())
		playDescription = _("Refused by RomM: pair this device again.");

	gui->addSwitch(_("SYNC PLAY TIME"), playDescription, CONF_PLAYSESSIONS, false, []
	{
		if (SystemConf::getInstance()->getBool(CONF_PLAYSESSIONS, false))
			RommPlaySessions::flushAsync();
	});

	gui->addGroup(_("STORAGE"));
	// MEDIA CACHE SIZE (limits: RommMediaCache)
	{
		auto sizeText = [](unsigned long long bytes)
		{
			char buf[32];
			if (bytes >= 1024ULL * 1024ULL * 1024ULL)
				snprintf(buf, sizeof(buf), "%.1f GB", bytes / (1024.0 * 1024.0 * 1024.0));
			else
				snprintf(buf, sizeof(buf), "%.0f MB", bytes / (1024.0 * 1024.0));
			return std::string(buf);
		};

		unsigned long long usage = mediaCacheUsage(config.baseUrl());
		unsigned long long limit = mediaCacheLimit(config, usage);
		std::string used = _("USED") + ": " + sizeText(usage) + (limit == ~0ULL ? std::string() : " / " + sizeText(limit));

		auto sizeList = std::make_shared<OptionListComponent<std::string>>(window, _("MEDIA CACHE SIZE"), false);
		sizeList->add(_("AUTOMATIC"), "auto", config.cacheSize == "auto");
		for (auto gb : { 1, 2, 5, 10, 20, 50 })
			sizeList->add(std::to_string(gb) + " GB", std::to_string(gb * 1024), config.cacheSize == std::to_string(gb * 1024));
		sizeList->add(_("UNLIMITED"), "unlimited", config.cacheSize == "unlimited");

		if (!sizeList->hasSelection())
			sizeList->selectFirstItem();

		gui->addWithDescription(_("MEDIA CACHE SIZE"), used, sizeList);

		std::string before = config.cacheSize;
		gui->addSaveFunc([sizeList, before]
		{
			std::string size = sizeList->getSelected();
			if (size == before)
				return;

			SystemConf::getInstance()->set(CONF_CACHE_SIZE, size);
			if (RommConfig::load().isUsable())
				RommSync::requestSync(false); // applies the new size
		});
	}

	gui->addEntry(_("CLEAN UP LOCAL CACHE"), false, [window]
	{
		window->pushGui(new GuiLoading<CacheCleanupResult>(window, _("CLEANING UP..."),
			[](IGuiLoadingHandler* gui) { return cleanupCache(); },
			[window](CacheCleanupResult result)
			{
				char size[32];
				snprintf(size, sizeof(size), "%.1f MB", result.bytes / (1024.0 * 1024.0));
				std::string text = _("FILES REMOVED") + ": " + std::to_string(result.files) + "\n" + _("SPACE FREED") + ": " + size;

				// Games may still point to removed media: reload the lists once the result is read
				window->pushGui(new GuiMsgBox(window, text, _("OK"), [window, result]
				{
					if (result.files > 0)
					{
						RommSync::clearPendingChanges();
						window->postToUiThread([window] { ViewController::reloadAllGames(window, false); });
					}
				}));
			}));
	});

	gui->addGroup(_("MAINTENANCE"));
	gui->addEntry(_("FULL RESYNC"), false, [window]
	{
		if (RommSync::requestSync(true))
			window->displayNotificationMessage(_("ROMM SYNCHRONIZATION STARTED"));
		else
			window->displayNotificationMessage(_("ROMM SYNCHRONIZATION ALREADY RUNNING"));
	});

	window->pushGui(gui);
}

void GuiRommSettings::openServerAccount()
{
	// Changed rarely once paired: the server, the account, pairing again
	Window* window = mWindow;
	RommConfig config = RommConfig::load();
	auto gui = new GuiSettings(window, _("SERVER & ACCOUNT").c_str());

	gui->addGroup(_("SERVER"));
	gui->addInputTextConfigRow(_("SERVER ADDRESS"), CONF_URL, false);

	auto insecure = std::make_shared<SwitchComponent>(window);
	insecure->setState(config.allowSelfSigned);
	gui->addWithDescription(_("ALLOW SELF-SIGNED CERTIFICATE"), _("Only for HTTPS servers using a certificate you created yourself."), insecure);
	gui->addSaveFunc([insecure] { SystemConf::getInstance()->setBool(CONF_INSECURE, insecure->getState()); });

	gui->addGroup(_("ACCOUNT"));
	gui->addWithLabel(_("STATUS"), std::make_shared<TextComponent>(window, accountText(config), ThemeData::getMenuTheme()->Text.font, ThemeData::getMenuTheme()->Text.color));

	// The actions of the ROMM menu: this one closes first (saving the switch they use)
	gui->addEntry(_("PAIR THIS DEVICE"), true, [this, window, gui] { window->postToUiThread([this] { pairDevice(); }); gui->close(); });
	gui->addEntry(_("ENTER A PAIRING CODE"), true, [this, window, gui] { window->postToUiThread([this] { enterPairingCode(); }); gui->close(); });
	gui->addEntry(_("UNPAIR THIS DEVICE"), false, [this, window, gui] { window->postToUiThread([this] { unpair(); }); gui->close(); });

	window->pushGui(gui);
}

RommConfig GuiRommSettings::currentConfig()
{
	RommConfig config = RommConfig::load();
	if (mInsecure != nullptr) // shown at the top level before pairing only
		config.allowSelfSigned = mInsecure->getState();
	return config;
}

void GuiRommSettings::reopen()
{
	Window* window = mWindow;
	close();
	window->pushGui(new GuiRommSettings(window));
}

void GuiRommSettings::pairDevice()
{
	RommConfig config = currentConfig();
	if (config.baseUrl().empty())
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("ENTER THE SERVER ADDRESS FIRST.")));
		return;
	}

	Window* window = mWindow;
	std::string url = config.baseUrl();
	bool insecure = config.allowSelfSigned;

	window->pushGui(new GuiLoading<ServerInfo>(window, _("CONTACTING THE SERVER..."),
		[url, insecure](auto gui) { return probeServer(url, insecure); },
		[this, window, config](ServerInfo info)
		{
			if (!info.reachable)
			{
				window->pushGui(new GuiMsgBox(window, _("UNABLE TO REACH THE ROMM SERVER") + "\n" + info.error, _("OK"), nullptr, GuiMsgBoxIcon::ICON_ERROR));
				return;
			}

			if (!info.supportsDevicePairing)
			{
				// RomM 4.x : pairing codes created from the web UI
				enterPairingCode();
				return;
			}

			window->pushGui(new GuiRommPairing(window, config, [this](bool paired)
			{
				if (paired)
					reopen();
			}));
		}));
}

void GuiRommSettings::enterPairingCode()
{
	RommConfig config = currentConfig();
	if (config.baseUrl().empty())
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("ENTER THE SERVER ADDRESS FIRST.")));
		return;
	}

	Window* window = mWindow;

	window->pushGui(new GuiTextEditPopupKeyboard(window, _("ROMM PAIRING CODE"), "", [this, window, config](const std::string& code)
	{
		window->pushGui(new GuiLoading<std::string>(window, _("PAIRING..."),
			[config, code](auto gui)
			{
				RommConfig cfg = config;
				std::string error;
				return exchangePairingCode(cfg, code, error) ? std::string() : (error.empty() ? std::string("error") : error);
			},
			[this, window](std::string error)
			{
				if (!error.empty())
				{
					window->pushGui(new GuiMsgBox(window, _("PAIRING FAILED") + "\n" + error, _("OK"), nullptr, GuiMsgBoxIcon::ICON_ERROR));
					return;
				}

				window->displayNotificationMessage(_("PAIRED WITH ROMM"));
				reopen();
			}));
	}, false));
}

// Number of games in a system's snapshot file (counted without a full XML parse)
static int countSnapshotGames(const std::string& path)
{
	std::string text = Utils::FileSystem::readAllText(path);

	int count = 0;
	for (size_t pos = text.find("<rom "); pos != std::string::npos; pos = text.find("<rom ", pos + 5))
		count++;

	return count;
}

void GuiRommSettings::openSystems()
{
	RommConfig config = RommConfig::load();
	std::string libraryDir = getLibraryDir(config.baseUrl());

	// Systems with RomM games, sorted by full name
	std::vector<std::pair<std::string, std::string>> systems; // full name, system
	if (Utils::FileSystem::isDirectory(libraryDir))
	{
		for (auto file : Utils::FileSystem::getDirContent(libraryDir, false, false))
		{
			if (Utils::String::toLower(Utils::FileSystem::getExtension(file)) != ".xml")
				continue;

			std::string system = Utils::FileSystem::getStem(file);
			systems.push_back(std::make_pair(RommCatalog::getSystemFullName(system), system));
		}
	}

	if (systems.empty())
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("NO ROMM GAMES YET. SYNCHRONIZE FIRST.")));
		return;
	}

	std::sort(systems.begin(), systems.end(), [](const std::pair<std::string, std::string>& a, const std::pair<std::string, std::string>& b)
	{
		return Utils::String::toLower(a.first) < Utils::String::toLower(b.first);
	});

	Window* window = mWindow;
	auto gui = new GuiSettings(window, _("ROMM SYSTEMS").c_str());
	auto switches = std::make_shared<std::vector<std::pair<std::string, std::shared_ptr<SwitchComponent>>>>();

	for (auto& item : systems)
	{
		int games = countSnapshotGames(libraryDir + "/" + item.second + ".xml");

		auto sw = std::make_shared<SwitchComponent>(window);
		sw->setState(config.hiddenSystems.find(item.second) == config.hiddenSystems.cend());
		gui->addWithDescription(item.first, std::to_string(games) + " " + _("GAMES"), sw);
		switches->push_back(std::make_pair(item.second, sw));
	}

	std::set<std::string> before = config.hiddenSystems;

	gui->addSaveFunc([window, switches, before]
	{
		// Keep hidden systems that are not in the list (no RomM games right now)
		std::set<std::string> hidden = before;
		for (auto& item : *switches)
		{
			if (item.second->getState())
				hidden.erase(item.first);
			else
				hidden.insert(item.first);
		}

		if (hidden == before)
			return;

		saveHiddenSystems(hidden);
		RommSync::clearPendingChanges();
		window->postToUiThread([window] { ViewController::reloadAllGames(window, false); });
	});

	window->pushGui(gui);
}

void GuiRommSettings::openSyncActions(const std::shared_ptr<TextComponent>& statusText)
{
	Window* window = mWindow;

	if (!RommSync::isRunning())
	{
		window->pushGui(new GuiMsgBox(window, _("NO SYNCHRONIZATION IN PROGRESS")));
		return;
	}

	auto cancelSync = [statusText]
	{
		if (RommSync::cancel())
			statusText->setText(_("CANCELLING..."));
	};

	if (RommSync::isPaused())
	{
		window->pushGui(new GuiMsgBox(window, _("SYNCHRONIZATION PAUSED"),
			_("UNPAUSE SYNC"), [statusText] { RommSync::resume(); statusText->setText(_("SYNCHRONIZING...")); },
			_("CANCEL SYNC"), cancelSync,
			_("BACK"), nullptr));
	}
	else
	{
		window->pushGui(new GuiMsgBox(window, _("SYNCHRONIZATION IN PROGRESS"),
			_("PAUSE SYNC"), [statusText] { RommSync::pause(); statusText->setText(_("PAUSED")); },
			_("CANCEL SYNC"), cancelSync,
			_("BACK"), nullptr));
	}
}

void GuiRommSettings::openMediaSelection(bool fromLibrary)
{
	RommConfig config = RommConfig::load();
	Window* window = mWindow;

	auto gui = new GuiSettings(window, fromLibrary ? _("SYNC FROM LIBRARY").c_str() : _("DOWNLOAD ALONG WITH ROM").c_str());
	auto switches = std::make_shared<std::vector<std::pair<Media, std::shared_ptr<SwitchComponent>>>>();

	for (auto& m : allMedia())
	{
		if (m.imageOnly)
			continue; // only in IMAGE SHOWN

		auto sw = std::make_shared<SwitchComponent>(window);
		sw->setState(fromLibrary ? config.syncs(m.type) : config.withRom(m.type));
		gui->addWithLabel(_(m.label), sw);
		switches->push_back(std::make_pair(m.type, sw));
	}

	// Bottom of SYNC FROM LIBRARY: the selected media downloaded again
	if (fromLibrary)
	{
		gui->addEntry(_("DOWNLOAD MEDIA AGAIN"), false, [window, switches]
		{
			if (RommSync::isRunning())
			{
				window->pushGui(new GuiMsgBox(window, _("A SYNCHRONIZATION IS RUNNING. TRY AGAIN WHEN IT IS OVER.")));
				return;
			}

			std::vector<Media> selected;
			for (auto& item : *switches)
				if (item.second->getState())
					selected.push_back(item.first);

			if (selected.empty())
			{
				window->pushGui(new GuiMsgBox(window, _("SELECT AT LEAST ONE MEDIA TYPE.")));
				return;
			}

			window->pushGui(new GuiMsgBox(window,
				_("DOWNLOAD THE SELECTED MEDIA AGAIN FOR ALL ROMM GAMES NOT ON THIS DEVICE?"),
				_("YES"), [window, switches, selected]
				{
					// The selection shown is the one used: keep it
					for (auto& item : *switches)
					{
						SystemConf::getInstance()->set(std::string(CONF_SYNC_PREFIX) + mediaInfo(item.first).key, item.second->getState() ? "1" : "0");
						RommSync::setMediaSyncEnabled(item.first, item.second->getState());
					}

					CacheCleanupResult removed = clearMediaCache(selected);

					if (RommConfig::load().isUsable() && RommSync::requestSync(false))
						window->displayNotificationMessage(_("DOWNLOADING MEDIA AGAIN") + " (" + std::to_string(removed.files) + ")");
				},
				_("NO"), nullptr));
		});
	}

	// Bottom of DOWNLOAD ALONG WITH ROM: the selected media downloaded again for the games on the device
	if (!fromLibrary)
	{
		gui->addEntry(_("DOWNLOAD MEDIA AGAIN"), false, [window, switches]
		{
			// The selection shown is the one used: keep it
			for (auto& item : *switches)
				SystemConf::getInstance()->set(std::string(CONF_WITHROM_PREFIX) + mediaInfo(item.first).key, item.second->getState() ? "1" : "0");

			RommConfig config = RommConfig::load();

			// What will be downloaded, by name (the IMAGE SHOWN and the large cover included)
			std::string mediaNames;
			for (auto& m : allMedia())
			{
				if (!config.fetchesWithRom(m.type))
					continue;

				if (!mediaNames.empty())
					mediaNames += ", ";

				mediaNames += _(m.label);
			}

			if (mediaNames.empty())
			{
				window->pushGui(new GuiMsgBox(window, _("SELECT AT LEAST ONE MEDIA TYPE.")));
				return;
			}

			if (RommSync::isRunning())
			{
				window->pushGui(new GuiMsgBox(window, _("A SYNCHRONIZATION IS RUNNING. TRY AGAIN WHEN IT IS OVER.")));
				return;
			}

			// The games downloaded from RomM by ES (not the games of the device RomM also has)
			std::vector<std::string> games;
			for (auto system : SystemData::sSystemVector)
			{
				if (!system->isGameSystem() || system->isCollection() || system->getRootFolder() == nullptr)
					continue;

				for (auto game : system->getRootFolder()->getFilesRecursive(GAME))
				{
					RemoteGame known;
					if (!game->isRemote() && RommCatalog::getKnownGame(game->getPath(), known) && isDownloadedFromRomm(game->getPath()))
						games.push_back(game->getPath());
				}
			}

			if (games.empty())
			{
				window->pushGui(new GuiMsgBox(window, _("NO GAME DOWNLOADED FROM ROMM ON THIS DEVICE YET.")));
				return;
			}

			// e.g. "DOWNLOAD AGAIN: COVER, MARQUEE / FOR THE 12 GAMES DOWNLOADED FROM ROMM?"
			window->pushGui(new GuiMsgBox(window,
				_("DOWNLOAD AGAIN") + ": " + mediaNames + "\n" +
				Utils::String::format(_("FOR THE %d GAMES DOWNLOADED FROM ROMM?").c_str(), (int)games.size()),
				_("YES"), [window, games]
				{
					// In the background, like a synchronization (SYNC STATUS: pause / cancel)
					if (RommSync::requestMediaRefresh(games))
						window->displayNotificationMessage(_("DOWNLOADING MEDIA AGAIN IN THE BACKGROUND"));
				},
				_("NO"), nullptr));
		});
	}

	gui->addSaveFunc([fromLibrary, switches, config]
	{
		bool turnedOn = false;

		for (auto& item : *switches)
		{
			const MediaInfo& m = mediaInfo(item.first);
			bool on = item.second->getState();
			bool was = fromLibrary ? config.syncs(item.first) : config.withRom(item.first);
			if (on == was)
				continue;

			// Written even when off, so the legacy keys (romm.covers...) no longer apply
			std::string key = std::string(fromLibrary ? CONF_SYNC_PREFIX : CONF_WITHROM_PREFIX) + m.key;
			SystemConf::getInstance()->set(key, on ? "1" : "0");

			if (fromLibrary)
				turnedOn = turnedOn || on;
		}

		RommSync::applyMediaSettings(); // turning one off stops its downloads in progress

		// Fetch the newly selected media right away
		if (turnedOn && RommConfig::load().isUsable())
			RommSync::requestSync(false);
	});

	window->pushGui(gui);
}

void GuiRommSettings::openCollections()
{
	RommConfig config = RommConfig::load();

	std::vector<RommCollection> collections;
	readCollectionsSnapshot(getCollectionsSnapshotPath(config.baseUrl()), collections);

	if (collections.empty())
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("NO ROMM COLLECTIONS YET. SYNCHRONIZE FIRST.")));
		return;
	}

	std::sort(collections.begin(), collections.end(), [](const RommCollection& a, const RommCollection& b)
	{
		return Utils::String::toLower(collectionDisplayName(a)) < Utils::String::toLower(collectionDisplayName(b));
	});

	Window* window = mWindow;
	auto gui = new GuiSettings(window, _("ROMM COLLECTIONS").c_str());
	auto switches = std::make_shared<std::vector<std::pair<std::string, std::shared_ptr<SwitchComponent>>>>();

	// Downloads every game of the collections switched on below that is not on the device yet,
	// with the media of DOWNLOAD ALONG WITH ROM. Nothing is ever deleted when a collection is unselected.
	gui->addEntry(_("DOWNLOAD SELECTED COLLECTIONS"), false, [window, switches, collections]
	{
		std::set<std::string> selected;
		for (auto& item : *switches)
			if (item.second->getState())
				selected.insert(item.first);

		if (selected.empty())
		{
			window->pushGui(new GuiMsgBox(window, _("SELECT AT LEAST ONE COLLECTION.")));
			return;
		}

		// The selection shown is the one used: keep it (the reload at the end closes this menu)
		saveSelectedCollections(selected);

		std::vector<FileData*> games;
		std::set<std::string> seen; // a game in several collections is downloaded once
		long long totalSize = 0;

		for (auto& c : collections)
		{
			if (selected.find(c.key) == selected.cend())
				continue;

			for (auto& g : c.games)
			{
				std::string systemPath = RommCatalog::getSystemPath(g.first);
				if (systemPath.empty())
					continue;

				std::string path = Utils::FileSystem::getGenericPath(systemPath + "/" + Utils::FileSystem::getFileName(g.second));
				if (!seen.insert(path).second)
					continue;

				RemoteGame remote;
				if (!RommCatalog::getRemoteGame(path, remote))
					continue; // on the device already, or not shown (hidden system)

				SystemData* system = SystemData::getSystem(g.first);
				FileData* game = system != nullptr && system->getRootFolder() != nullptr ? system->getRootFolder()->FindByPath(path) : nullptr;
				if (game == nullptr || !game->isRemote())
					continue;

				games.push_back(game);
				totalSize += remote.size;
			}
		}

		if (games.empty())
		{
			window->pushGui(new GuiMsgBox(window, _("ALL THE GAMES OF THESE COLLECTIONS ARE ALREADY ON THIS DEVICE.")));
			return;
		}

		char size[32];
		if (totalSize >= 1024LL * 1024LL * 1024LL)
			snprintf(size, sizeof(size), "%.2f GB", totalSize / (1024.0 * 1024.0 * 1024.0));
		else
			snprintf(size, sizeof(size), "%.1f MB", totalSize / (1024.0 * 1024.0));

		window->pushGui(new GuiMsgBox(window,
			_("DOWNLOAD") + " " + std::to_string(games.size()) + " " + _("GAMES") + " (" + size + ")?",
			_("YES"), [window, games] { window->pushGui(new GuiRommBatchDownload(window, games)); },
			_("NO"), nullptr));
	});

	for (auto& c : collections)
	{
		auto sw = std::make_shared<SwitchComponent>(window);
		sw->setState(config.collections.find(c.key) != config.collections.cend());

		std::string description = std::to_string(c.games.size()) + " " + _("GAMES");
		if (c.smart)
			description += " - " + _("SMART COLLECTION");

		gui->addWithDescription(collectionDisplayName(c), description, sw);
		switches->push_back(std::make_pair(c.key, sw));
	}

	std::set<std::string> before = config.collections;

	gui->addSaveFunc([window, switches, before]
	{
		// Keep selected collections that are not in the list (not on the server right now)
		std::set<std::string> selected = before;
		for (auto& item : *switches)
		{
			if (item.second->getState())
				selected.insert(item.first);
			else
				selected.erase(item.first);
		}

		if (selected == before)
			return;

		saveSelectedCollections(selected);
		window->postToUiThread([window] { ViewController::reloadAllGames(window, false); });
	});

	window->pushGui(gui);
}

void GuiRommSettings::unpair()
{
	Window* window = mWindow;

	window->pushGui(new GuiMsgBox(window, _("UNPAIR THIS DEVICE FROM ROMM?\nDOWNLOADED GAMES ARE KEPT."), _("YES"), [this]
	{
		RommConfig::clearCredentials(true);
		reopen();
	}, _("NO"), nullptr));
}
