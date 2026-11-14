#include "guis/GuiRommPairing.h"

#include "romm/RommAuth.h"
#include "components/ButtonComponent.h"
#include "components/MenuComponent.h"
#include "components/QrCodeComponent.h"
#include "components/TextComponent.h"
#include "InputConfig.h"
#include "LocaleES.h"
#include "ThemeData.h"
#include "Window.h"
#include "renderers/Renderer.h"

#include <chrono>
#include <memory>
#include <vector>

GuiRommPairing::GuiRommPairing(Window* window, const Romm::RommConfig& config, const std::function<void(bool)>& onClose)
	: GuiComponent(window), mBackground(window, ":/frame.png"), mGrid(window, Vector2i(1, 4)),
	mConfig(config), mOnClose(onClose), mCancel(false),
	mPhase(Phase::STARTING), mShownPhase(Phase::STARTING), mClosing(false)
{
	auto theme = ThemeData::getMenuTheme();

	mBackground.setImagePath(theme->Background.path);
	mBackground.setEdgeColor(theme->Background.color);
	mBackground.setCenterColor(theme->Background.centerColor);
	mBackground.setCornerSize(theme->Background.cornerSize);

	mTitle = std::make_shared<TextComponent>(mWindow, _("PAIR WITH ROMM"), theme->Title.font, theme->Title.color, ALIGN_CENTER);
	mGrid.setEntry(mTitle, Vector2i(0, 0), false, true);

	mQrCode = std::make_shared<QrCodeComponent>(mWindow);
	mGrid.setEntry(mQrCode, Vector2i(0, 1), false, true); // the grid gives it the remaining height

	mMessage = std::make_shared<TextComponent>(mWindow, _("CONTACTING THE SERVER..."), theme->Text.font, theme->Text.color, ALIGN_CENTER);
	mGrid.setEntry(mMessage, Vector2i(0, 2), false, true);

	std::vector<std::shared_ptr<ButtonComponent>> buttons;
	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("CANCEL"), _("CANCEL"), [this] { closeGui(); }));
	mButtonGrid = makeButtonGrid(mWindow, buttons);
	mGrid.setEntry(mButtonGrid, Vector2i(0, 3), true, false, Vector2i(1, 1), GridFlags::BORDER_TOP);

	float width = Renderer::getScreenWidth() * (Renderer::isSmallScreen() ? 0.95f : 0.6f);
	float height = Renderer::getScreenHeight() * (Renderer::isSmallScreen() ? 0.95f : 0.85f);
	setSize(width, height);
	setPosition((Renderer::getScreenWidth() - mSize.x()) / 2.0f, (Renderer::getScreenHeight() - mSize.y()) / 2.0f);

	addChild(&mBackground);
	addChild(&mGrid);

	mThread = std::thread(&GuiRommPairing::threadPairing, this);
}

GuiRommPairing::~GuiRommPairing()
{
	mCancel = true;

	// The worker sleeps and polls in 100 ms steps, and HTTP waits check the flag every 20 ms
	if (mThread.joinable())
		mThread.join();
}

void GuiRommPairing::onSizeChanged()
{
	GuiComponent::onSizeChanged();

	mGrid.setSize(mSize);

	// Keep the text rows tight: the QR code (row 1) gets all the remaining height
	float titleHeight = mTitle->getFont()->getHeight() * 1.5f;
	float messageHeight = mMessage->getFont()->getHeight() * 4.4f; // 4 lines of text
	float buttonsHeight = mButtonGrid->getSize().y();

	mGrid.setRowHeightPerc(0, titleHeight / mSize.y(), false);
	mGrid.setRowHeightPerc(2, messageHeight / mSize.y(), false);
	mGrid.setRowHeightPerc(3, buttonsHeight / mSize.y(), false);
	mGrid.onSizeChanged();

	mBackground.fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));
}

void GuiRommPairing::setPhase(Phase phase, const std::string& error)
{
	std::unique_lock<std::mutex> lock(mMutex);
	mPhase = phase;
	mError = error;
}

void GuiRommPairing::threadPairing()
{
	Romm::PairingRequest request = Romm::startDevicePairing(mConfig, &mCancel);
	if (mCancel)
		return;

	if (!request.ok)
	{
		setPhase(Phase::FAILED, request.error);
		return;
	}

	{
		std::unique_lock<std::mutex> lock(mMutex);
		mUserCode = request.userCode;
		mVerificationUrl = request.verificationUrl;
		mPageUrl = request.pairingPageUrl;
		mPhase = Phase::WAITING;
	}

	auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(request.expiresIn);
	int interval = request.interval;
	int transientErrors = 0;

	while (!mCancel && std::chrono::steady_clock::now() < deadline)
	{
		for (int i = 0; i < interval * 10 && !mCancel; i++)
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

		if (mCancel)
			return;

		std::string error;
		Romm::PollState state = Romm::pollDevicePairing(mConfig, request.deviceCode, error, &mCancel);

		switch (state)
		{
		case Romm::PollState::PENDING:
			transientErrors = 0;
			break;

		case Romm::PollState::SLOW_DOWN:
			interval += 5;
			break;

		case Romm::PollState::APPROVED:
			setPhase(Phase::APPROVED);
			return;

		case Romm::PollState::DENIED:
			setPhase(Phase::FAILED, _("THE PAIRING REQUEST WAS DENIED"));
			return;

		case Romm::PollState::EXPIRED:
			setPhase(Phase::FAILED, _("THE PAIRING CODE EXPIRED"));
			return;

		case Romm::PollState::FAILED:
			if (mCancel)
				return;

			// Wi-Fi hiccups happen : give up only after several errors in a row
			if (++transientErrors >= 3)
			{
				setPhase(Phase::FAILED, error);
				return;
			}
			break;
		}
	}

	if (!mCancel)
		setPhase(Phase::FAILED, _("THE PAIRING CODE EXPIRED"));
}

void GuiRommPairing::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	Phase phase;
	std::string error, userCode, verificationUrl, pageUrl;
	{
		std::unique_lock<std::mutex> lock(mMutex);
		phase = mPhase;
		error = mError;
		userCode = mUserCode;
		verificationUrl = mVerificationUrl;
		pageUrl = mPageUrl;
	}

	if (phase == mShownPhase)
		return;

	mShownPhase = phase;

	switch (phase)
	{
	case Phase::WAITING:
		mQrCode->setText(verificationUrl);
		mMessage->setText(
			_("SCAN THE CODE WITH YOUR PHONE, OR OPEN") + "\n" + pageUrl + "\n" +
			_("AND ENTER THE CODE") + " " + userCode + "\n" +
			_("WAITING FOR APPROVAL..."));
		break;

	case Phase::APPROVED:
	{
		Window* window = mWindow;
		auto onClose = mOnClose;
		delete this;

		window->displayNotificationMessage(_("PAIRED WITH ROMM"));
		if (onClose)
			onClose(true);

		return;
	}

	case Phase::FAILED:
		mQrCode->setText("");
		mMessage->setText(_("PAIRING FAILED") + "\n" + error);
		break;

	default:
		break;
	}
}

void GuiRommPairing::closeGui()
{
	if (mClosing)
		return;

	mClosing = true;

	auto onClose = mOnClose;
	delete this;

	if (onClose)
		onClose(false);
}

bool GuiRommPairing::input(InputConfig* config, Input input)
{
	if (config->isMappedTo(BUTTON_BACK, input) && input.value != 0)
	{
		closeGui();
		return true;
	}

	return GuiComponent::input(config, input);
}

std::vector<HelpPrompt> GuiRommPairing::getHelpPrompts()
{
	// The CANCEL button already provides the prompt (BACK also cancels, as in other dialogs)
	return mGrid.getHelpPrompts();
}
