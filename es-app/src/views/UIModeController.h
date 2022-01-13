#pragma once
#ifndef ES_APP_VIEWS_UI_MODE_CONTROLLER_H
#define ES_APP_VIEWS_UI_MODE_CONTROLLER_H

#include <vector>
#include <string>

#include "Settings.h"

class FileData;
class InputConfig;
class ViewController;

struct Input;

class UIModeController : public ISettingsChangedEvent
{
public:
	static UIModeController* getInstance();

	// Monitor input for UI mode change, returns true (consumes input) when UI mode change is triggered.
	bool listen(InputConfig* config, Input input);
	
	// Get the current Passphrase as a (unicode) formatted, comma-separated, string.
	std::string getFormattedPassKeyStr();

	// Check for change in UI mode.
	void monitorUIMode();

	bool isUIModeFull();
	bool isUIModeKid();
	bool isUIModeKiosk();
	bool isUIModeBasic();

	inline std::vector<std::string> getUIModes() { return mUIModes; };

	void onSettingChanged(const std::string& name) override;

private:
	UIModeController();
	bool inputIsMatch(InputConfig * config, Input input);
	bool isValidInput(InputConfig * config, Input input);
	void logInput(InputConfig * config, Input input);
	
	// Return UI mode to 'FULL'
	void unlockUIMode(); 

	static UIModeController * sInstance;
	const std::vector<std::string> mUIModes = { "Full", /*"Basic", */"Kiosk", "Kid" };
	
	// default passkeyseq = "uuddlrlrba", as defined in the setting 'UIMode_passkey'.
	std::string mPassKeySequence;
	int mPassKeyCounter;
	const std::vector<std::string> mInputVals = { "up", "down", "left", "right", "a", "b", "x", "y" };
	
	std::string mCurrentUIMode;
	bool mUIModeChanged;
};

#endif // ES_APP_VIEWS_UI_MODE_CONTROLLER_H
