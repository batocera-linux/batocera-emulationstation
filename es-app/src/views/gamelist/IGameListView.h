#pragma once
#ifndef ES_APP_VIEWS_GAME_LIST_IGAME_LIST_VIEW_H
#define ES_APP_VIEWS_GAME_LIST_IGAME_LIST_VIEW_H

#include "renderers/Renderer.h"
#include "FileData.h"
#include "GuiComponent.h"

class ThemeData;
class Window;

// This is an interface that defines the minimum for a GameListView.
class IGameListView : public GuiComponent
{
public:
	IGameListView(Window* window, FolderData* root) : GuiComponent(window), mRoot(root)
		{ setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight()); }

	virtual ~IGameListView() {}

	// Called when a new file is added, a file is removed, a file's metadata changes, or a file's children are sorted.
	// NOTE: FILE_SORTED is only reported for the topmost FileData, where the sort started.
	//       Since sorts are recursive, that FileData's children probably changed too.
	virtual void onFileChanged(FileData* file, FileChangeType change) = 0;
	
	// Called whenever the theme changes.
	virtual void onThemeChanged(const std::shared_ptr<ThemeData>& theme) = 0;

	void setTheme(const std::shared_ptr<ThemeData>& theme);
	inline const std::shared_ptr<ThemeData>& getTheme() const { return mTheme; }

	virtual FileData* getCursor() = 0;
	virtual void setCursor(FileData*) = 0;

	virtual void remove(FileData* game) = 0;

	virtual const char* getName() const = 0;
	virtual void launch(FileData* game) = 0;

	virtual HelpStyle getHelpStyle() override;

	void render(const Transform4x4f& parentTrans) override;
	virtual void setThemeName(std::string name);

	virtual std::vector<std::string> getEntriesLetters() = 0;
	virtual std::vector<FileData*> getFileDataEntries() = 0;
	bool hasFileDataEntry(FileData* file);

	virtual void repopulate() = 0;

protected:
	FolderData* mRoot;
	std::shared_ptr<ThemeData> mTheme;
	std::string mCustomThemeName;
};

#endif // ES_APP_VIEWS_GAME_LIST_IGAME_LIST_VIEW_H
