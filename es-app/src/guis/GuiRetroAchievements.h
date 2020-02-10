#pragma once

#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/BusyComponent.h"
#include "GuiSettings.h"
#include "ApiSystem.h"

class GuiRetroAchievements : public GuiSettings 
{
public:
	static void show(Window* window);

protected:
	GuiRetroAchievements(Window *window, RetroAchievementInfo ra);    
};
