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

// from RetroArch network/netplay
#define NETPLAY_NICK_LEN         32
#define NETPLAY_HOST_STR_LEN     32
#define NETPLAY_HOST_LONGSTR_LEN 256
#define DISCOVERY_QUERY_MAGIC    0x52414E51 /* RANQ */
#define DISCOVERY_RESPONSE_MAGIC 0x52414E53 /* RANS */

struct ad_packet
{
	uint32_t header;
	int32_t  content_crc;
	int32_t  port;
	uint32_t has_password;
	char     nick[NETPLAY_NICK_LEN];
	char     frontend[NETPLAY_HOST_STR_LEN];
	char     core[NETPLAY_HOST_STR_LEN];
	char     core_version[NETPLAY_HOST_STR_LEN];
	char     retroarch_version[NETPLAY_HOST_STR_LEN];
	char     content[NETPLAY_HOST_LONGSTR_LEN];
	char     subsystem_name[NETPLAY_HOST_LONGSTR_LEN];
};

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
	std::string mitm_session;
	bool		fixed;
	std::string retroarch_version;
	int         port;

	bool		isCrcValid;
	bool		coreExists;
};


class GuiNetPlay : public GuiComponent 
{
public:
	GuiNetPlay(Window* window);
	~GuiNetPlay();

	void update(int deltaTime) override;
	void render(const Transform4x4f &parentTrans) override;
	void onSizeChanged() override;
	bool input(InputConfig* config, Input input) override;
	std::vector<HelpPrompt> getHelpPrompts() override;

	bool hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult = nullptr) override;
	bool onMouseClick(int button, bool pressed, int x, int y);

private:
	void startRequest();
	bool populateFromJson(const std::string json);
	bool populateFromLan();
	void launchGame(LobbyAppEntry entry);
	void lanLobbyRequest();

	FileData* getFileData(const std::string gameInfo, bool crc = true, std::string coreName = "");
	bool coreExists(FileData* file, std::string core_name);

	int								mLanLobbySocket;
	NinePatchComponent				mBackground;
	ComponentGrid					mGrid;

	std::shared_ptr<TextComponent>	mTitle;
	std::shared_ptr<TextComponent>	mSubtitle;

	std::shared_ptr<ComponentList>	mList;

	std::shared_ptr<ComponentGrid>	mHeaderGrid;
	std::shared_ptr<ComponentGrid>	mButtonGrid;

	BusyComponent					mBusyAnim;

	std::unique_ptr<HttpReq>		mLobbyRequest;
};
