#include "guis/GuiRommSaves.h"

#include "romm/RommSaves.h"
#include "romm/RommSync.h"

#include "FileData.h"
#include "SaveStateRepository.h"
#include "SystemData.h"
#include "LocaleES.h"
#include "Log.h"
#include "utils/StringUtil.h"
#include "Window.h"
#include "guis/GuiLoading.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiSettings.h"

#include <ctime>
#include <set>
#include <thread>
#include <memory>
#include <string>

struct SaveCheckResult
{
	Romm::SaveCheck check = Romm::SaveCheck::Ready;
	Romm::SaveConflict conflict;
	std::string message;
};

static void ready(const Romm::SaveJob& job, const std::function<void()>& onReady)
{
	Romm::RommSaves::markChecked(job.path);
	if (onReady)
		onReady();
}

void GuiRommSaves::checkThenRun(Window* window, FileData* game, const std::function<void()>& onReady)
{
	Romm::SaveJob job;
	if (!Romm::RommSaves::prepare(game, job))
	{
		job.path = game->getSourceFileData()->getPath();
		ready(job, onReady);
		return;
	}

	window->pushGui(new GuiLoading<std::shared_ptr<SaveCheckResult>>(window, _("SYNCING SAVES WITH ROMM..."),
		[job](IGuiLoadingHandler*)
		{
			auto result = std::make_shared<SaveCheckResult>();
			result->check = Romm::RommSaves::beforeLaunch(job, result->conflict, result->message);
			return result;
		},
		[window, job, onReady](std::shared_ptr<SaveCheckResult> result)
		{
			switch (result->check)
			{
			case Romm::SaveCheck::Conflict:
				askConflict(window, job, result->conflict, onReady);
				return;

			case Romm::SaveCheck::Offline:
				window->displayNotificationMessage(_("ROMM NOT REACHED: LOCAL SAVE USED"));
				break;

			case Romm::SaveCheck::Failed:
				window->displayNotificationMessage(_("SAVES NOT SYNCED WITH ROMM"));
				break;

			default:
				break;
			}

			ready(job, onReady);
		}));
}

bool GuiRommSaves::syncBeforeChoice(Window* window, FileData* game, const std::function<void()>& next)
{
	Romm::SaveJob job;
	if (game == nullptr || Romm::RommSaves::isChecked(game->getSourceFileData()->getPath()) || !Romm::RommSaves::prepare(game, job))
		return false;

	LOG(LogInfo) << "RomM: saves of " << job.path << " synced before the save state choice";
	checkThenRun(window, game, [game, next]
	{
		if (SaveStateRepository::isEnabled(game))
			game->getSourceFileData()->getSystem()->getSaveStateRepository()->refresh();

		if (next)
			next();
	});

	return true;
}

bool GuiRommSaves::confirmHardcore(Window* window, FileData* game, const std::function<void()>& next)
{
	static std::set<std::string> confirmed; // UI thread

	if (window == nullptr || game == nullptr)
		return false;

	std::string path = game->getSourceFileData()->getPath();
	if (confirmed.erase(path))
		return false;

	Romm::SaveJob job;
	std::string device;
	if (!Romm::RommSaves::prepare(game, job) || !Romm::RommSaves::softcoreSaveForHardcore(job, device))
		return false;

	std::string question = device.empty()
		? _("This in-game save was made outside hardcore mode on this device. Play it in hardcore mode anyway?")
		: Utils::String::format(_("This in-game save was made outside hardcore mode on %s. Play it in hardcore mode anyway?").c_str(), device.c_str());
	window->pushGui(new GuiMsgBox(window, job.name + "\n" + question,
		_("PLAY"), [path, next]
		{
			confirmed.insert(path);
			Romm::RommSaves::markChecked(path);
			if (next)
				next();
		},
		_("CANCEL"), nullptr));

	return true;
}

static void sendInBackground(const Romm::SaveJob& job)
{
	std::thread([job]
	{
		std::string message;
		Romm::RommSaves::afterGame(job, message);
	}).detach();
}

void GuiRommSaves::afterGame(Window* window, FileData* game, bool startedFromAutosave)
{
	Romm::SaveJob job;
	if (!Romm::RommSaves::prepare(game, job))
		return;

	Romm::SaveConflict replaced;
	if (startedFromAutosave || window == nullptr || !Romm::RommSaves::replacedCloudAutosave(job, replaced))
	{
		sendInBackground(job);
		return;
	}

	// Once ES is back on screen
	std::string other = replaced.currentDevice.empty() ? _("another device") : replaced.currentDevice;
	window->postToUiThread([window, job, other]
	{
		window->pushGui(new GuiMsgBox(window, job.name + "\n" +
			Utils::String::format(_("This session replaced the autosave from %s without starting from it. Which autosave do you keep?").c_str(), other.c_str()),
			_("THIS SESSION'S"), [job] { sendInBackground(job); },
			_("THE OTHER ONE"), [job]
			{
				std::thread([job]
				{
					std::string message;
					if (!Romm::RommSaves::restoreHeldSlot(job, "auto", message))
						LOG(LogWarning) << "RomM: autosave of " << job.path << " not put back: " << message;
					Romm::RommSaves::afterGame(job, message);
				}).detach();
			}));
	});
}

static void runChoice(Window* window, const Romm::SaveJob& job, Romm::SaveChoice choice, const std::function<void()>& onReady)
{
	window->pushGui(new GuiLoading<std::shared_ptr<SaveCheckResult>>(window, _("SYNCING SAVES WITH ROMM..."),
		[job, choice](IGuiLoadingHandler*)
		{
			auto result = std::make_shared<SaveCheckResult>();
			result->check = Romm::RommSaves::resolve(job, choice, result->message) ? Romm::SaveCheck::Ready : Romm::SaveCheck::Failed;
			return result;
		},
		[window, job, onReady](std::shared_ptr<SaveCheckResult> result)
		{
			if (result->check == Romm::SaveCheck::Ready)
			{
				ready(job, onReady);
				return;
			}

			window->pushGui(new GuiMsgBox(window, _("THE SAVE COULD NOT BE SYNCED WITH ROMM.") + "\n" + result->message,
				_("PLAY ANYWAY"), [job, onReady] { ready(job, onReady); },
				_("CANCEL"), nullptr));
		}));
}

// RomM's UTC date, in local time
static std::string localDate(const std::string& iso)
{
	long long utc = Romm::parseIsoUtc(iso);
	if (utc < 0)
		return "";

	time_t t = (time_t)utc;
	char buf[32];
	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", localtime(&t));
	return buf;
}

// close() deletes the menu and its callback: next is copied first
static void closeThen(GuiSettings* menu, std::function<void()> next)
{
	menu->close();
	next();
}

void GuiRommSaves::askConflict(Window* window, const Romm::SaveJob& job, const Romm::SaveConflict& conflict, const std::function<void()>& onReady)
{
	std::string cloud = conflict.currentDevice.empty() ? _("another device") : conflict.currentDevice;
	std::string when = localDate(conflict.currentDate);
	if (!when.empty())
		cloud += ", " + when;

	auto menu = new GuiSettings(window, conflict.hardcore ? _("REPLACE A HARDCORE SAVE?") : _("SAVE CONFLICT"));
	menu->setSubTitle(job.name);

	if (conflict.hardcore)
	{
		menu->addWithDescription(_("REPLACE THE HARDCORE SAVE"), _("The hardcore save stays in RomM's history."), nullptr, [=]
		{
			closeThen(menu, [=] { runChoice(window, job, Romm::SaveChoice::ApproveHardcore, onReady); });
		});
	}
	else
	{
		std::string local = conflict.localDate.empty() ? "" : " (" + conflict.localDate + ")";

		menu->addWithDescription(_("USE THE CLOUD SAVE"), Utils::String::format(_("From %s. Yours is kept as a backup.").c_str(), cloud.c_str()), nullptr, [=]
		{
			closeThen(menu, [=] { runChoice(window, job, Romm::SaveChoice::UseServer, onReady); });
		});

		menu->addWithDescription(_("KEEP THIS DEVICE'S SAVE") + local, _("It becomes the latest; the other one stays in history."), nullptr, [=]
		{
			closeThen(menu, [=] { runChoice(window, job, Romm::SaveChoice::KeepLocal, onReady); });
		});

		menu->addWithDescription(_("KEEP BOTH"), _("This device's save goes on in a channel of its own."), nullptr, [=]
		{
			closeThen(menu, [=] { runChoice(window, job, Romm::SaveChoice::KeepBoth, onReady); });
		});
	}

	if (conflict.hardcore)
	{
		menu->addWithDescription(_("USE THE CLOUD SAVE"), Utils::String::format(_("From %s.").c_str(), cloud.c_str()), nullptr, [=]
		{
			closeThen(menu, [=] { runChoice(window, job, Romm::SaveChoice::UseServer, onReady); });
		});
	}

	// Asked again next time. A button, so it stays in view
	menu->getMenu().addButton(_("PLAY WITHOUT SYNCING"), _("PLAY"), [=]
	{
		closeThen(menu, [=] { ready(job, onReady); });
	});

	window->pushGui(menu);
}
