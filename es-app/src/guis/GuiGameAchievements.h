#pragma once

#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/BusyComponent.h"
#include "GuiSettings.h"
#include "RetroAchievements.h"
#include "GuiRetroAchievements.h"

class GuiGameAchievements : public GuiSettings
{
public:
	static void show(Window* window, int gameId);
	void	render(const Transform4x4f& parentTrans) override;

protected:
	GuiGameAchievements(Window *window, GameInfoAndUserProgress ra);

	void	centerWindow();

	std::shared_ptr<RetroAchievementProgress> mProgress;
};
