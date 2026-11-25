#include "guis/GuiRommDownload.h"

#include "romm/RommDownloader.h"
#include "romm/RommBios.h"
#include "romm/RommCatalog.h"
#include "views/ViewController.h"
#include "Log.h"
#include "guis/GuiLoading.h"
#include "SystemData.h"
#include "FileData.h"
#include "InputConfig.h"
#include "LocaleES.h"
#include "Window.h"
#include "guis/GuiMsgBox.h"
#include "renderers/Renderer.h"
#include "utils/StringUtil.h"

#include <algorithm>
#include <cstdio>

// Short first line ("SYNCING FROM ROMM  45%"), then the game and the sizes.
// "\r\n" makes BusyComponent use several lines (with "\n" alone it shows only one, truncated).
static std::string progressText(const std::string& name, int percent, long long bytes, long long total);

// The download box keeps one size (BusyComponent::setFixedTextWidth): long names are shortened so the
// text stays on its two lines
static std::string shortName(const std::string& name, size_t maxBytes = 40)
{
	if (name.size() <= maxBytes)
		return name;

	size_t cut = maxBytes - 3;
	while (cut > 0 && ((unsigned char)name[cut] & 0xC0) == 0x80) // not in the middle of a UTF-8 character
		cut--;

	return name.substr(0, cut) + "...";
}

static void fixBoxSize(BusyComponent& busy)
{
	busy.setFixedTextWidth(Renderer::getScreenWidth() * 0.6f);
}

static std::string formatSize(long long bytes)
{
	char buf[32];
	if (bytes >= 1024LL * 1024LL * 1024LL)
		snprintf(buf, sizeof(buf), "%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
	else
		snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024.0));

	return buf;
}

static std::string progressText(const std::string& name, int percent, long long bytes, long long total)
{
	if (percent == -2) // downloaded, being installed (PS3 PSN games)
		return _("INSTALLING") + "\r\n" + name;

	std::string text = _("SYNCING FROM ROMM") + "  " + std::to_string(percent < 0 ? 0 : percent) + "%\r\n" + name;

	if (bytes > 0 && total > 0)
		text += "  (" + formatSize(bytes) + " / " + formatSize(total) + ")";
	else if (bytes > 0)
		text += "  (" + formatSize(bytes) + ")";

	return text;
}

// ---------------------------------------------------------------------------
// GuiRommBatchDownload
// ---------------------------------------------------------------------------

GuiRommBatchDownload::GuiRommBatchDownload(Window* window, const std::vector<FileData*>& games)
	: GuiComponent(window), mBusyAnim(window), mCancel(false), mDone(false), mDownloaded(0), mFailed(0), mTextChanged(false)
{
	for (auto game : games)
	{
		FileData* source = game->getSourceFileData();

		Item item;
		item.game = source;
		item.path = source->getPath();
		item.name = shortName(source->getName());

		Romm::RemoteGame remote;
		item.size = Romm::RommCatalog::getRemoteGame(item.path, remote) ? remote.size : 0;

		mItems.push_back(item);
	}

	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	mText = _("SYNCING FROM ROMM") + "\r\n" + std::to_string(mItems.size()) + " " + _("GAMES");
	mBusyAnim.setText(mText);
	mBusyAnim.setSize(mSize);
	fixBoxSize(mBusyAnim);

	mThread = std::thread(&GuiRommBatchDownload::threadDownload, this);
}

GuiRommBatchDownload::~GuiRommBatchDownload()
{
	mCancel = true;
	if (mThread.joinable())
		mThread.join();
}

void GuiRommBatchDownload::threadDownload()
{
	const int total = (int)mItems.size();

	for (int n = 0; n < total && !mCancel; n++)
	{
		const Item& item = mItems[n];
		std::string counter = _("SYNCING FROM ROMM") + "  " + std::to_string(n + 1) + "/" + std::to_string(total);

		std::string error;

		bool ok = Romm::downloadGame(item.path, [this, &item, &counter](int percent, long long bytes)
		{
			if (item.size > 0)
				percent = (int)std::min(100LL, bytes * 100 / item.size);

			std::string text = counter + "\r\n" + item.name + "  " + std::to_string(percent < 0 ? 0 : percent) + "%";
			if (bytes > 0)
				text += "  (" + formatSize(bytes) + (item.size > 0 ? " / " + formatSize(item.size) : std::string()) + ")";

			std::unique_lock<std::mutex> lock(mMutex);
			mText = text;
			mTextChanged = true;
		}, &mCancel, error);

		if (ok)
		{
			mDownloaded++;

			// Media moved next to the rom, metadata saved: on the UI thread (this screen is modal, the
			// game lists cannot be reloaded meanwhile)
			FileData* game = item.game;
			mWindow->postToUiThread([game]() { Romm::onGameDownloaded(game, false); });
		}
		else if (!mCancel)
		{
			mFailed++;
			LOG(LogWarning) << "RomM: batch download failed for " << item.path << " : " << error;
		}
	}

	mDone = true;
}

void GuiRommBatchDownload::render(const Transform4x4f& parentTrans)
{
	if (!mVisible)
		return;

	Transform4x4f trans = parentTrans * getTransform();
	renderChildren(trans);
	Renderer::setMatrix(trans);
	mBusyAnim.render(trans);
}

bool GuiRommBatchDownload::input(InputConfig* config, Input input)
{
	if (config->isMappedTo(BUTTON_BACK, input) && input.value != 0)
	{
		mCancel = true;
		return true;
	}

	return true; // modal
}

std::vector<HelpPrompt> GuiRommBatchDownload::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("CANCEL")));
	return prompts;
}

void GuiRommBatchDownload::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	{
		std::unique_lock<std::mutex> lock(mMutex);
		if (mTextChanged)
		{
			mBusyAnim.setText(mText);
			mBusyAnim.setSize(mSize);
			mTextChanged = false;
		}
	}

	mBusyAnim.update(deltaTime);

	if (!mDone)
		return;

	Window* window = mWindow;
	int downloaded = mDownloaded;
	int failed = mFailed;
	bool cancelled = mCancel;

	// The UI-thread work posted for each game runs before the reload posted below
	delete this;

	std::string text = _("GAMES DOWNLOADED") + ": " + std::to_string(downloaded);
	if (failed > 0)
		text += "\n" + _("FAILED DOWNLOADS") + ": " + std::to_string(failed);
	if (cancelled)
		text += "\n" + _("DOWNLOAD CANCELLED");

	window->pushGui(new GuiMsgBox(window, text, _("OK"), [window, downloaded]
	{
		if (downloaded > 0)
			window->postToUiThread([window] { ViewController::reloadAllGames(window, false); });
	}));
}

void GuiRommDownload::downloadThenRun(Window* window, FileData* game, const std::function<void()>& onSuccess)
{
	window->pushGui(new GuiRommDownload(window, game, onSuccess));
}

void GuiRommDownload::checkBiosThenRun(Window* window, FileData* game, const std::function<void()>& onDone)
{
	std::string system = game->getSourceFileData()->getSystem()->getName();

	window->pushGui(new GuiLoading<Romm::BiosDownloadResult>(window, _("CHECKING BIOS FILES..."),
		[system](IGuiLoadingHandler* gui)
		{
			return Romm::downloadMissingBios(Romm::findMissingBios(system), [gui](const std::string& file)
			{
				gui->setText(_("BIOS FROM ROMM") + "\r\n" + file);
			});
		},
		[window, game, onDone](Romm::BiosDownloadResult result)
		{
			if (result.downloaded > 0)
				window->displayNotificationMessage(_("BIOS FILES DOWNLOADED FROM ROMM") + " (" + std::to_string(result.downloaded) + ")");

			Romm::markBiosChecked(game); // the launch below must not check again
			if (onDone)
				onDone();
		}));
}

GuiRommDownload::GuiRommDownload(Window* window, FileData* game, const std::function<void()>& onSuccess)
	: GuiComponent(window), mGame(game), mOnSuccess(onSuccess), mBusyAnim(window),
	mCancel(false), mDone(false), mSuccess(false), mTextChanged(false), mExpectedSize(0)
{
	mPath = game->getSourceFileData()->getPath();
	mName = shortName(game->getName());

	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	Romm::RemoteGame remote;
	if (Romm::RommCatalog::getRemoteGame(mPath, remote))
		mExpectedSize = remote.size;

	mText = progressText(mName, 0, 0, mExpectedSize);
	mBusyAnim.setText(mText);
	mBusyAnim.setSize(mSize);
	fixBoxSize(mBusyAnim);

	mThread = std::thread(&GuiRommDownload::threadDownload, this);
}

GuiRommDownload::~GuiRommDownload()
{
	mCancel = true;

	// The download loop checks the cancel flag every 20 ms, so this join is short
	if (mThread.joinable())
		mThread.join();
}

void GuiRommDownload::threadDownload()
{
	std::string error;

	bool ok = Romm::downloadGame(mPath, [this](int percent, long long bytes)
	{
		// The size RomM reported at sync time does not depend on the server sending Content-Length
		if (mExpectedSize > 0)
			percent = (int)std::min(100LL, bytes * 100 / mExpectedSize);

		std::string text = progressText(mName, percent, bytes, mExpectedSize);

		std::unique_lock<std::mutex> lock(mMutex);
		if (text != mText)
		{
			mText = text;
			mTextChanged = true;
		}
	}, &mCancel, error);

	{
		std::unique_lock<std::mutex> lock(mMutex);
		mSuccess = ok;
		mError = error;
	}

	mDone = true;
}

void GuiRommDownload::render(const Transform4x4f& parentTrans)
{
	if (!mVisible)
		return;

	Transform4x4f trans = parentTrans * getTransform();
	renderChildren(trans);
	Renderer::setMatrix(trans);
	mBusyAnim.render(trans);
}

bool GuiRommDownload::input(InputConfig* config, Input input)
{
	if (config->isMappedTo(BUTTON_BACK, input) && input.value != 0)
	{
		mCancel = true;
		return true;
	}

	return true; // modal
}

std::vector<HelpPrompt> GuiRommDownload::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("CANCEL")));
	return prompts;
}

void GuiRommDownload::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	{
		std::unique_lock<std::mutex> lock(mMutex);
		if (mTextChanged)
		{
			mBusyAnim.setText(mText);
			mBusyAnim.setSize(mSize);
			mTextChanged = false;
		}
	}

	mBusyAnim.update(deltaTime);

	if (!mDone)
		return;

	bool success;
	std::string error;
	{
		std::unique_lock<std::mutex> lock(mMutex);
		success = mSuccess;
		error = mError;
	}

	Window* window = mWindow;
	FileData* game = mGame;
	auto onSuccess = mOnSuccess;
	bool cancelled = mCancel;

	delete this; // joins the (finished) worker

	if (success)
	{
		Romm::onGameDownloaded(game);

		if (onSuccess)
			onSuccess();
	}
	else if (!cancelled)
		window->pushGui(new GuiMsgBox(window, error, _("OK"), nullptr, GuiMsgBoxIcon::ICON_ERROR));
}
