#pragma once
#ifndef ES_APP_GUIS_GUI_ROMM_DOWNLOAD_H
#define ES_APP_GUIS_GUI_ROMM_DOWNLOAD_H

#include "GuiComponent.h"
#include "components/BusyComponent.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class FileData;

// Modal download of a RomM game, with progress. BACK cancels.
// On success the game becomes a local game, then onSuccess runs on the UI thread.
class GuiRommDownload : public GuiComponent
{
public:
	static void downloadThenRun(Window* window, FileData* game, const std::function<void()>& onSuccess = nullptr);

	// Downloads the game system's missing BIOS files that RomM has, then runs onDone (always)
	static void checkBiosThenRun(Window* window, FileData* game, const std::function<void()>& onDone);

	GuiRommDownload(Window* window, FileData* game, const std::function<void()>& onSuccess);
	~GuiRommDownload();

	void render(const Transform4x4f& parentTrans) override;
	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	std::vector<HelpPrompt> getHelpPrompts() override;

private:
	void threadDownload();

	FileData* mGame;
	std::string mPath;
	std::string mName;
	std::function<void()> mOnSuccess;

	BusyComponent mBusyAnim;
	std::thread mThread;

	std::atomic<bool> mCancel;
	std::atomic<bool> mDone;
	bool mSuccess;
	std::string mError;

	std::mutex mMutex;
	std::string mText;
	bool mTextChanged;

	long long mExpectedSize; // from RomM, when the server does not send Content-Length
};

// Downloads several RomM games one after the other (DOWNLOAD SELECTED COLLECTIONS), each with the
// media selected in DOWNLOAD ALONG WITH ROM. BACK cancels: games already downloaded stay.
// The game lists are reloaded once at the end.
class GuiRommBatchDownload : public GuiComponent
{
public:
	GuiRommBatchDownload(Window* window, const std::vector<FileData*>& games);
	~GuiRommBatchDownload();

	void render(const Transform4x4f& parentTrans) override;
	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	std::vector<HelpPrompt> getHelpPrompts() override;

private:
	void threadDownload();

	struct Item { FileData* game; std::string path; std::string name; long long size; };
	std::vector<Item> mItems;

	BusyComponent mBusyAnim;
	std::thread mThread;
	std::atomic<bool> mCancel;
	std::atomic<bool> mDone;
	std::atomic<int> mDownloaded;
	std::atomic<int> mFailed;

	std::mutex mMutex;
	std::string mText;
	bool mTextChanged;
};

#endif
