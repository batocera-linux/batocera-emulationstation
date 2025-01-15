#include "guis/knulli/Pico8Installer.h"
#include "components/OptionListComponent.h"
#include "components/SliderComponent.h"
#include "components/SwitchComponent.h"
#include "guis/GuiMsgBox.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "SystemConf.h"
#include "ApiSystem.h"
#include "InputManager.h"
#include "AudioManager.h"
#include <SDL_events.h>
#include <algorithm>
#include "utils/Platform.h"
#include "Paths.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include <stdio.h>
#include <sys/wait.h>

int Pico8Installer::install()
{
	if (hasInstaller()) {
		int result = system("/usr/bin/knulli-install-pico8");
		return WEXITSTATUS(result);
	}
	// Installer is missing
	return 2;
}


bool Pico8Installer::hasInstaller() {

	std::string extensionZip = "zip";

	std::string pico8Folder = Paths::getRootPath() + "/roms/pico8";
    auto dirContent = Utils::FileSystem::getDirContent(pico8Folder);

    for (auto file : dirContent) {

		std::string fileName = Utils::FileSystem::getFileName(file);
		std::string fileNameLower = Utils::String::toLower(fileName);
		std::string extension = Utils::FileSystem::getExtension(file);
		std::string extensionLower = Utils::String::toLower(extension);

		if (extensionZip.compare(extensionLower)
			&& fileNameLower.find("pico-8") != std::string::npos
			&& fileNameLower.find("raspi") != std::string::npos) {
			return true;
		}
	}
    return false;

}
