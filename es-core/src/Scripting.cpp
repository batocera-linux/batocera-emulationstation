#include "Scripting.h"
#include "Log.h"
#include "platform.h"
#include "utils/FileSystemUtil.h"

namespace Scripting
{
	void fireEvent(const std::string& eventName, const std::string& arg1, const std::string& arg2, const std::string& arg3)
	{
		LOG(LogDebug) << "fireEvent: " << eventName << " " << arg1 << " " << arg2 << " " << arg3;

        std::list<std::string> scriptDirList;
        std::string test;

        // check in homepath
		test = Utils::FileSystem::getEsConfigPath() + "/scripts/" + eventName;
        if(Utils::FileSystem::exists(test))
            scriptDirList.push_back(test);

		// check in getSharedConfigPath ( or exe path )
		test = Utils::FileSystem::getSharedConfigPath() + "/scripts/" + eventName;
		if (Utils::FileSystem::exists(test))
			scriptDirList.push_back(test);

        for(std::list<std::string>::const_iterator dirIt = scriptDirList.cbegin(); dirIt != scriptDirList.cend(); ++dirIt) {
            std::list<std::string> scripts = Utils::FileSystem::getDirContent(*dirIt);
            for (std::list<std::string>::const_iterator it = scripts.cbegin(); it != scripts.cend(); ++it) {
                std::string script = *it;

                for (auto arg : { arg1, arg2, arg3 })
                {
                    if (arg.empty())
                        break;

                    script += " \"" + arg + "\"";
                }
                LOG(LogDebug) << "  executing: " << script;
                runSystemCommand(script, "", nullptr);
            }
        }
	}

} // Scripting::
