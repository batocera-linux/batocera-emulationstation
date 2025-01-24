#include "QuickResume.h"
#include <SystemConf.h>
#include "utils/FileSystemUtil.h"

namespace QuickResume
{    
    const std::string shutdownFlag = "/var/run/shutdown.flag";

    bool setQuickResume(std::string quickResumeCommand, std::string quickResumePath)
    {
        bool configSaved = false;

        if (quickResumeEnabled())
        {
            if (!quickResumeCommand.empty() && !quickResumePath.empty())
            {
                SystemConf::getInstance()->set("global.bootgame.path", quickResumePath);
                SystemConf::getInstance()->set("global.bootgame.cmd", quickResumeCommand);
                configSaved = SystemConf::getInstance()->saveSystemConf();
            }
        }

        return configSaved;
    }

    bool clearQuickResume()
    {
        bool configSaved = false;

        if (quickResumeEnabled())
        {
            SystemConf::getInstance()->set("global.bootgame.path", "");
            SystemConf::getInstance()->set("global.bootgame.cmd", "");
            configSaved = SystemConf::getInstance()->saveSystemConf();
        }

        return configSaved;
    }

    bool postLaunchConditionalClean()
    {
        bool configSaved = false;

        if (quickResumeEnabled() && !shutDownInProgress())
        {
            configSaved = clearQuickResume();
        }

        return configSaved;
    }

    bool shutDownInProgress()
    {
        return Utils::FileSystem::exists(shutdownFlag);
    }

    bool quickResumeEnabled()
    {
        return SystemConf::getInstance()->getBool("global.quickresume") == true;
    }
}