#include "GuiNetPlaySettings.h"
#include "SystemConf.h"
#include "ThreadedHasher.h"
#include "components/SwitchComponent.h"
#include "GuiHashStart.h"

GuiNetPlaySettings::GuiNetPlaySettings(Window* window) : GuiSettings(window, _("NETPLAY SETTINGS").c_str())
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
	addOptionList(_("USE RELAY SERVER"), { { _("NONE"), "" },{ _("NEW YORK") , "nyc" },{ _("MADRID") , "madrid" },{ _("MONTREAL") , "montreal" },{ _("SAO PAULO") , "saopaulo" },{ _("CUSTOM") , "custom" } }, "global.netplay.relay", false);
	addInputTextRow(_("CUSTOM RELAY SERVER"), "global.netplay.customserver", false);

#ifndef WIN32
	auto enableHotspot = std::make_shared<SwitchComponent>(mWindow);
	enableHotspot->setState(SystemConf::getInstance()->getBool("global.netplay.hotspot"));
	addWithDescription(_("AUTOMATICALLY USE HOTSPOT FOR LOCAL NETPLAY"), _("Creates a hotspot when hosting, or connects when joining.") + _U("\n\uF071 ") + _("CAUTION: This may drain battery faster."), enableHotspot);
	addSaveFunc([enableHotspot] {
		if (enableHotspot->getState() != SystemConf::getInstance()->getBool("global.netplay.hotspot"))
			SystemConf::getInstance()->setBool("global.netplay.hotspot", enableHotspot->getState());
	});
#endif

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
