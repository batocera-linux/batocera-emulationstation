#pragma once

#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/BusyComponent.h"
#include "GuiSettings.h"
#include "RetroAchievements.h"

class GuiGameAchievements : public GuiSettings
{
public:
	static void show(Window* window, int gameId);

protected:
	GuiGameAchievements(Window *window, GameInfoAndUserProgress ra);

	void	centerWindow();
};
