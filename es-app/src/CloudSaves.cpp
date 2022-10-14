#include "CloudSaves.h"

#include "SystemConf.h"
#include "guis/GuiLoading.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiSaveState.h"
#include "SaveStateRepository.h"
#include "platform.h"

void guiSaveStateLoad(Window* window, FileData* game)
{
  GuiComponent* guiComp = window->peekGui();
  GuiSaveState* guiSaveState = dynamic_cast<GuiSaveState*>(guiComp);
  if (guiSaveState && CloudSaves::getInstance().isSupported(game)) {
    auto callback = [](GuiComponent* guiComp) {
      GuiSaveState* guiSaveState = dynamic_cast<GuiSaveState*>(guiComp);
      if (guiSaveState)
        guiSaveState->loadGridAndCenter();
    };
    CloudSaves::getInstance().load(window, game, guiSaveState, callback);
  }  
}

void CloudSaves::load(Window* window, FileData *game, GuiComponent* guiComp, const std::function<void(GuiComponent*)>& callback)
{
  guiComp->setVisible(false);
	auto loading = new GuiLoading<bool>(window, _("LOADING PLEASE WAIT"),
	[this, window, game, guiComp, callback](auto gui) {
		std::string sysName = game->getSourceFileData()->getSystem()->getName();
		int exitCode = runSystemCommand("ra_rclone.sh get \""+sysName+"\" \""+game->getPath()+"\"", "", nullptr);
		if (exitCode != 0)
			window->pushGui(new GuiMsgBox(window, _("ERROR LOADING FROM CLOUD"), _("OK")));
    guiComp->setVisible(true);
    callback(guiComp);
		return true;
	});
	window->pushGui(loading);
}

void CloudSaves::save(Window* window, FileData* game)
{
	auto loading = new GuiLoading<bool>(window, _("SAVING PLEASE WAIT"),
	[this, window, game](auto gui) {
		std::string sysName = game->getSourceFileData()->getSystem()->getName();
		int exitCode = runSystemCommand("ra_rclone.sh set \""+sysName+"\" \""+game->getPath()+"\"", "", nullptr);
		if (exitCode != 0)
			window->pushGui(new GuiMsgBox(window, _("ERROR SAVING TO CLOUD"), _("OK")));
		else
			window->pushGui(new GuiMsgBox(window, _("SAVED TO CLOUD"), _("OK")));
		return true;
	});
	window->pushGui(loading);
}

bool CloudSaves::isSupported(FileData* game)
{
  SystemData* system = game->getSourceFileData()->getSystem();
	bool canCloudSave = system->isFeatureSupported(
		game->getEmulator(true),
		game->getEmulator(true),
		EmulatorFeatures::cloudsave);
  canCloudSave = canCloudSave && 
    SystemConf::getInstance()->get(system->getName() + ".cloudsave") == "1";
	canCloudSave = canCloudSave && SaveStateRepository::isEnabled(game);
  return canCloudSave;
}