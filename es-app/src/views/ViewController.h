#pragma once
#ifndef ES_APP_VIEWS_VIEW_CONTROLLER_H
#define ES_APP_VIEWS_VIEW_CONTROLLER_H

#include "renderers/Renderer.h"
#include "FileData.h"
#include "GuiComponent.h"
#include <vector>
#include <functional>

class IGameListView;
class ISimpleGameListView;
class SystemData;
class SystemView;
class Window;

// Used to smoothly transition the camera between multiple views (e.g. from system to system, from gamelist to gamelist).
class ViewController : public GuiComponent
{
public:
	enum ViewMode
	{
		NOTHING,
		START_SCREEN,
		SYSTEM_SELECT,
		GAME_LIST
	};

	static void init(Window* window);
	static void saveState();

	static ViewController* get();

	virtual ~ViewController();

	// Try to completely populate the GameListView map.
	// Caches things so there's no pauses during transitions.
	void preload();

	// If a basic view detected a metadata change, it can request to recreate
	// the current gamelist view (as it may change to be detailed).
	void reloadGameListView(IGameListView* gamelist);
	inline void reloadGameListView(SystemData* system) { reloadGameListView(getGameListView(system).get()); }
	void reloadSystemListViewTheme(SystemData* system);

	void reloadAll(Window* window = nullptr, bool reloadTheme = true); // Reload everything with a theme.  Used when the "ThemeSet" setting changes.

	// Navigation.
	void goToNextGameList();
	void goToPrevGameList();
	void goToGameList(SystemData* system, bool forceImmediate = false);
	bool goToGameList(const std::string& systemName, bool forceImmediate = false);
	void goToSystemView(SystemData* system, bool forceImmediate = false);
	void goToSystemView(std::string& systemName, bool forceImmediate = false, ViewMode mode = SYSTEM_SELECT);
	void goToStart(bool forceImmediate = false);
	void ReloadAndGoToStart();

	void onFileChanged(FileData* file, FileChangeType change);

	// Plays a nice launch effect and launches the game at the end of it.
	// Once the game terminates, plays a return effect.
	void launch(FileData* game, LaunchGameOptions options, Vector3f centerCameraOn = Vector3f(Renderer::getScreenWidth() / 2.0f, Renderer::getScreenHeight() / 2.0f, 0), bool allowCheckLaunchOptions = true);
	void launch(FileData* game, Vector3f centerCameraOn = Vector3f(Renderer::getScreenWidth() / 2.0f, Renderer::getScreenHeight() / 2.0f, 0)) { launch(game, LaunchGameOptions(), centerCameraOn); }

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;

	enum GameListViewType
	{
		AUTOMATIC,
		BASIC,
		DETAILED,
		GRID,
		VIDEO
	};

	struct State
	{
		ViewMode viewing;

		inline SystemData* getSystem() const { assert(viewing == GAME_LIST || viewing == SYSTEM_SELECT); return system; }

	private:
		friend ViewController;
		SystemData* system;
	};

	inline const State& getState() const { return mState; }

	virtual std::vector<HelpPrompt> getHelpPrompts() override;
	virtual HelpStyle getHelpStyle() override;

	std::shared_ptr<IGameListView> getGameListView(SystemData* system, bool loadIfnull = true, const std::function<void()>& createAsPopupAndSetExitFunction = nullptr);
	std::shared_ptr<SystemView> getSystemListView();
	void removeGameListView(SystemData* system);

	void onThemeChanged(const std::shared_ptr<ThemeData>& theme);

	virtual void onShow() override;
	virtual void onScreenSaverActivate();
	virtual void onScreenSaverDeactivate();

	SystemData* getSelectedSystem();
	ViewMode getViewMode();

	static void reloadAllGames(Window* window, bool deleteCurrentGui = false);

	void setActiveView(std::shared_ptr<GuiComponent> view);

private:
	ViewController(Window* window);
	static ViewController* sInstance;

	void playViewTransition(bool forceImmediate);
	bool doLaunchGame(FileData* game, LaunchGameOptions options);
	bool checkLaunchOptions(FileData* game, LaunchGameOptions options, Vector3f center);
	int getSystemId(SystemData* system);
	
	std::shared_ptr<GuiComponent> mCurrentView;
	std::map< SystemData*, std::shared_ptr<IGameListView> > mGameListViews;
	std::shared_ptr<SystemView> mSystemListView;
	
	Transform4x4f mCamera;
	float mFadeOpacity;
	bool mLockInput;
	std::shared_ptr<GuiComponent>	mDeferPlayViewTransitionTo;
	State mState;
};

#endif // ES_APP_VIEWS_VIEW_CONTROLLER_H
