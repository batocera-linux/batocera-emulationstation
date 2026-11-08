#pragma once

#include "GuiSettings.h"
#include "RetroAchievements.h"

class FileData;

class GuiRetroAchievements : public GuiSettings 
{
public:
	static void show(Window* window);
	
	bool input(InputConfig* config, Input input) override;
	std::vector<HelpPrompt> getHelpPrompts() override;

	static FileData* getFileData(const std::string& cheevosGameId);

protected:
	GuiRetroAchievements(Window *window, RetroAchievementInfo ra);    
	void	centerWindow();
};

#define RETROACHIEVEMENTS_SOFTCORE_COLOR 0x0B71C1FF
#define RETROACHIEVEMENTS_HARDCORE_COLOR 0xCC9900FF

class RetroAchievementProgress : public GuiComponent
{
public:
	RetroAchievementProgress(Window* window, int valueSoftcore, int valueHardcore, int max, const std::string& label);

	void onSizeChanged() override;
	void render(const Transform4x4f& parentTrans) override;
	void setColor(unsigned int color) override;

private:
	int mValueSoftCore;
	int mValueHardCore;
	int mMax;

	std::shared_ptr<TextComponent> mText;
};
