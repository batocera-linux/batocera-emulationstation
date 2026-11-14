#pragma once
#ifndef ES_APP_GUIS_GUI_ROMM_SETTINGS_H
#define ES_APP_GUIS_GUI_ROMM_SETTINGS_H

#include "guis/GuiSettings.h"
#include "romm/RommConfig.h"
#include <memory>

class SwitchComponent;

class GuiRommSettings : public GuiSettings
{
public:
	GuiRommSettings(Window* window);

	static void open(Window* window);

	// SYSTEM SETTINGS > ENABLE ROMM INTEGRATION changed (UI thread)
	static void onIntegrationToggled(Window* window);

private:
	Romm::RommConfig currentConfig();   // saved credentials + values being edited
	void reopen();

	void pairDevice();
	void enterPairingCode();
	void openSystems();
	void openCollections();
	void openMediaSelection(bool fromLibrary); // SYNC FROM LIBRARY / DOWNLOAD ALONG WITH ROM
	void openLibraryManagement(); // media, favorites, play time, cache, maintenance
	void openServerAccount();     // server address, certificate, pairing (once paired)
	void openSyncActions(const std::shared_ptr<class TextComponent>& statusText); // pause / unpause / cancel
	void unpair();

	std::shared_ptr<SwitchComponent> mInsecure;
	bool mWasEnabled;
	bool mWasPaired;
};

#endif
