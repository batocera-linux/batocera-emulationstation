#include "GuiNetPlaySettings.h"
#include "SystemConf.h"
#include "ThreadedHasher.h"
#include "components/OptionListComponent.h"
#include "components/SwitchComponent.h"
#include "GuiHashStart.h"
#include <unordered_map>

static std::unordered_map<std::string, std::string> communityRelayServers =
{
	{"seoul",		"138.2.119.137"},
};

GuiNetPlaySettings::GuiNetPlaySettings(Window* window, int selectItem) : GuiSettings(window, _("NETPLAY SETTINGS").c_str())
{
	std::string port = SystemConf::getInstance()->get("global.netplay.port");
	if (port.empty())
		SystemConf::getInstance()->set("global.netplay.port", "55435");

	addGroup(_("SETTINGS"));

	auto enableNetplay = std::make_shared<SwitchComponent>(mWindow);
	enableNetplay->setState(SystemConf::getInstance()->getBool("global.netplay"));
	addWithLabel(_("ENABLE NETPLAY"), enableNetplay);
	addInputTextRow(_("NICKNAME"), "global.netplay.nickname", false);

	addGroup(_("OPTIONS"));

	addInputTextRow(_("PORT"), "global.netplay.port", false);

	addRelayServerOptions(selectItem);

	addSwitch(_("AUTOMATICALLY CREATE LOBBY"), _("Automatically creates a Netplay lobby when starting a game."), "NetPlayAutomaticallyCreateLobby", true, nullptr);
	addSwitch(_("SHOW RELAY SERVER GAMES ONLY"), _("Relay server games have a higher chance of successful entry."), "NetPlayShowOnlyRelayServerGames", true, nullptr);
	addSwitch(_("SHOW UNAVAILABLE GAMES"), _("Show rooms for games not present on this machine."), "NetPlayShowMissingGames", true, nullptr);

	addGroup(_("GAME INDEXES"));

	addSwitch(_("INDEX NEW GAMES AT STARTUP"), "NetPlayCheckIndexesAtStart", true);
	addEntry(_("INDEX GAMES"), true, [this]
	{
		if (ThreadedHasher::checkCloseIfRunning(mWindow))
			mWindow->pushGui(new GuiHashStart(mWindow, ThreadedHasher::HASH_NETPLAY_CRC));
	});

	Window* wnd = mWindow;
	addSaveFunc([wnd, enableNetplay]
	{
		if (SystemConf::getInstance()->setBool("global.netplay", enableNetplay->getState()))
		{
			if (!ThreadedHasher::isRunning() && enableNetplay->getState())
			{
				ThreadedHasher::start(wnd, ThreadedHasher::HASH_NETPLAY_CRC, false, true);
			}
		}
	});
}

void GuiNetPlaySettings::addRelayServerOptions(int selectItem)
{
	const std::string customRelayServerAddress = SystemConf::getInstance()->get("global.netplay.customserver");
	std::string selectedRelayServer = SystemConf::getInstance()->get("global.netplay.relay");
	if (selectedRelayServer == "custom")
	{
		for (const auto& kv : communityRelayServers)
		{
			if (kv.second != customRelayServerAddress)
				continue;

			selectedRelayServer = kv.first;
			break;
		}
	}

	auto relayServer = std::make_shared<OptionListComponent<std::string>>(mWindow, _("USE RELAY SERVER"), false);
	relayServer->add(_("NONE"), "", selectedRelayServer.empty());

	// Official RetroArch relay servers
	relayServer->add(_("NEW YORK"), "nyc", selectedRelayServer == "nyc");
	relayServer->add(_("MADRID"), "madrid", selectedRelayServer == "madrid");
	relayServer->add(_("MONTREAL"), "montreal", selectedRelayServer == "montreal");
	relayServer->add(_("SAO PAULO"), "saopaulo", selectedRelayServer == "saopaulo");

	// User Community relay servers
	relayServer->add(_("SEOUL"), "seoul", selectedRelayServer == "seoul");

	// Custom relay server
	relayServer->add(_("CUSTOM"), "custom", selectedRelayServer == "custom");

	if (!relayServer->hasSelection())
		relayServer->selectFirstItem();

	addWithLabel(_("USE RELAY SERVER"), relayServer, selectItem == 1);

	relayServer->setSelectedChangedCallback([this](const std::string& newRelayServer)
	{
		std::string originalCustomServerAddress = SystemConf::getInstance()->get("global.netplay.customserver");
		std::string backupCustomServerAddress = SystemConf::getInstance()->get("global.netplay.customserver.backup");

		if (communityRelayServers.find(newRelayServer) != communityRelayServers.end())
		{
			SystemConf::getInstance()->set("global.netplay.relay", "custom");
			SystemConf::getInstance()->set("global.netplay.customserver", communityRelayServers[newRelayServer]);

			if (!std::any_of(communityRelayServers.begin(), communityRelayServers.end(), [&](const auto& kv) { return kv.second == originalCustomServerAddress; }))
				SystemConf::getInstance()->set("global.netplay.customserver.backup", originalCustomServerAddress);
		}
		else if (newRelayServer == "custom")
		{
			SystemConf::getInstance()->set("global.netplay.relay", newRelayServer);

			if (!backupCustomServerAddress.empty())
				SystemConf::getInstance()->set("global.netplay.customserver", backupCustomServerAddress);
			else if (std::any_of(communityRelayServers.begin(), communityRelayServers.end(), [&](const auto& kv) { return kv.second == originalCustomServerAddress; }))
				SystemConf::getInstance()->set("global.netplay.customserver", "");
		}
		else
		{
			SystemConf::getInstance()->set("global.netplay.relay", newRelayServer);
		}
		
		Window* pw = mWindow;
		delete this;
		pw->pushGui(new GuiNetPlaySettings(pw, 1));
	});

	if (selectedRelayServer == "custom" && std::find_if(communityRelayServers.begin(), communityRelayServers.end(), [&](const auto& kv) { return kv.second == customRelayServerAddress; }) == communityRelayServers.end())
		addInputTextRow(_("CUSTOM RELAY SERVER"), "global.netplay.customserver", false);
}