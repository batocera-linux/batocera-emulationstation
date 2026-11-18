#pragma once
#ifndef ES_APP_GUIS_GUI_ROMM_PAIRING_H
#define ES_APP_GUIS_GUI_ROMM_PAIRING_H

#include "GuiComponent.h"
#include "components/ComponentGrid.h"
#include "components/NinePatchComponent.h"
#include "romm/RommConfig.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <memory>
#include <vector>

class TextComponent;
class ButtonComponent;
class QrCodeComponent;

// RomM device pairing (RomM 5.0+): shows a QR code of the approval page (it carries the
// code) plus the address and code as text, and polls the server until approved.
class GuiRommPairing : public GuiComponent
{
public:
	GuiRommPairing(Window* window, const Romm::RommConfig& config, const std::function<void(bool paired)>& onClose);
	~GuiRommPairing();

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void onSizeChanged() override;
	std::vector<HelpPrompt> getHelpPrompts() override;

private:
	enum class Phase { STARTING, WAITING, APPROVED, FAILED };

	void threadPairing();
	void setPhase(Phase phase, const std::string& error = "");
	void closeGui();

	NinePatchComponent mBackground;
	ComponentGrid mGrid;
	std::shared_ptr<TextComponent> mTitle;
	std::shared_ptr<QrCodeComponent> mQrCode;
	std::shared_ptr<TextComponent> mMessage;
	std::shared_ptr<ComponentGrid> mButtonGrid;

	Romm::RommConfig mConfig;
	std::function<void(bool)> mOnClose;

	std::thread mThread;
	std::atomic<bool> mCancel;

	std::mutex mMutex;
	Phase mPhase;
	Phase mShownPhase;
	std::string mError;
	std::string mUserCode;
	std::string mVerificationUrl;
	std::string mPageUrl;
	bool mClosing;
};

#endif
