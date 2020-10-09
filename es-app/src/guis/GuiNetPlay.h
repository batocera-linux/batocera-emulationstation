#pragma once

#include "GuiComponent.h"
#include "components/ComponentGrid.h"
#include "components/ComponentList.h"
#include "components/BusyComponent.h"
#include "components/NinePatchComponent.h"
#include "components/TextComponent.h"
#include <memory>
#include <unordered_set>

class HttpReq;
class FileData;

struct LobbyAppEntry
{
	FileData*   fileData;
	std::string username;
	std::string game_crc;
	std::string mitm_ip;
	std::string subsystem_name;
	std::string frontend;
	std::string created;
	std::string core_version;
	std::string ip;
	std::string updated;
	std::string country;
	int         host_method;
	bool		has_password;
	std::string game_name;
	bool		has_spectate_password;
	std::string core_name;
	int         mitm_port;
	bool		fixed;
	std::string retroarch_version;
	int         port;

	bool		isCrcValid;
	bool		coreExists;
};


class GuiNetPlay : public GuiComponent 
{
public:
	GuiNetPlay(Window *window);

	void update(int deltaTime) override;
	void render(const Transform4x4f &parentTrans) override;
	void onSizeChanged() override;
	bool input(InputConfig* config, Input input) override;
	std::vector<HelpPrompt> getHelpPrompts() override;

private:
	void startRequest();
	bool populateFromJson(const std::string json);
	void launchGame(LobbyAppEntry entry);

	FileData* getFileData(const std::string gameInfo, bool crc = true, std::string coreName = "");
	bool coreExists(FileData* file, std::string core_name);

	NinePatchComponent				mBackground;
	ComponentGrid					mGrid;

	std::shared_ptr<TextComponent>	mTitle;
	std::shared_ptr<TextComponent>	mSubtitle;

	std::shared_ptr<ComponentList>	mList;

	std::shared_ptr<ComponentGrid>	mHeaderGrid;
	std::shared_ptr<ComponentGrid>	mButtonGrid;

	BusyComponent					mBusyAnim;

	std::unique_ptr<HttpReq> mLobbyRequest;
};
