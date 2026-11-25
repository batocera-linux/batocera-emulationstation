#pragma once
#ifndef ES_APP_GUIS_GUI_ROMM_SAVES_H
#define ES_APP_GUIS_GUI_ROMM_SAVES_H

#include <functional>

class FileData;
class Window;

namespace Romm { struct SaveJob; struct SaveConflict; }

class GuiRommSaves
{
public:
	// Syncs the game's saves, asks a conflict, then onReady (not called if the player cancels)
	static void checkThenRun(Window* window, FileData* game, const std::function<void()>& onReady);

	// Syncs before ES lists the game's save states, then next(). False: nothing to sync
	static bool syncBeforeChoice(Window* window, FileData* game, const std::function<void()>& next);

	// Pushes in the background; first asks which autosave to keep if the session replaced another device's
	static void afterGame(Window* window, FileData* game, bool startedFromAutosave);

	// Confirms a hardcore session on a softcore save. True when asked: next runs if they play
	static bool confirmHardcore(Window* window, FileData* game, const std::function<void()>& next);

	// The conflict dialog
	static void askConflict(Window* window, const Romm::SaveJob& job, const Romm::SaveConflict& conflict, const std::function<void()>& onReady);
};

#endif
