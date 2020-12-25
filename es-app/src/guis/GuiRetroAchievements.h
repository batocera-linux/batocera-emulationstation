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

class RetroAchievementProgress : public GuiComponent
{
public:
	RetroAchievementProgress(Window* window, int value, int max, const std::string& label);

	void onSizeChanged() override;
	void render(const Transform4x4f& parentTrans) override;
	void setColor(unsigned int color) override;

private:
	int mValue;
	int mMax;

	std::shared_ptr<TextComponent> mText;
};
