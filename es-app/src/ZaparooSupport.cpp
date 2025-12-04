#include "ZaparooSupport.h"
#include "HttpReq.h"
#include "Log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <sys/wait.h>

Zaparoo* Zaparoo::sInstance = nullptr;

Zaparoo::Zaparoo()
{
}

Zaparoo* Zaparoo::getInstance()
{
	if (Zaparoo::sInstance == nullptr)
		Zaparoo::sInstance = new Zaparoo();

	return Zaparoo::sInstance;
}

bool Zaparoo::executeScript(const std::string command)
{	
	LOG(LogInfo) << "Zaparoo::executeScript -> " << command;

	if (system(command.c_str()) == 0)
		return true;
	
	LOG(LogError) << "Error executing " << command;
	return false;
}

bool Zaparoo::isZaparooEnabled()
{
    HttpReq req("http://127.0.0.1:7497/health");

    if (req.wait()) {
        std::string data = req.getContent();
        return data.compare("OK") == 0;
    }
    return false;
}

bool Zaparoo::writeZaparooCard(std::string name)
{
	return executeScript("/userdata/system/zaparoo -write \"" + name + "\"");
}
