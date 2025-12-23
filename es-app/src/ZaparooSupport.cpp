#include "ZaparooSupport.h"
#include "HttpReq.h"
#include "Log.h"
#include <stdio.h>
#include <stdlib.h>
#include <mutex>
#include "utils/Randomizer.h"

bool Zaparoo::mChecked = false;
bool Zaparoo::mAvailable = false;

static std::mutex mZaparooLock;

void Zaparoo::invalidateCache()
{
	mChecked = false;
}

void Zaparoo::checkZaparooEnabledAsync(const std::function<void(bool enabled)>& func)
{
	std::thread([func]()
	{
		bool enabled = isZaparooEnabled(3000);
		if (func)
			func(enabled);
	}).detach();
}

bool Zaparoo::isZaparooEnabled(long defaultDelay)
{
	std::unique_lock<std::mutex> lock(mZaparooLock);

	if (mChecked)
		return mAvailable;

	HttpReqOptions options;
	options.connectTimeout = defaultDelay;
	HttpReq req("http://127.0.0.1:7497/health", &options);

	if (req.wait())
	{
		std::string data = req.getContent();
		mAvailable = (data.compare("OK") == 0);
	}
	else
		mAvailable = false;

	mChecked = true;
	return mAvailable;
}

bool Zaparoo::writeZaparooCard(const std::string& name)
{	
	std::string uuid = Randomizer::generateUUID();
	LOG(LogInfo) << "Zaparoo::writeZaparooCard -> " << uuid;

	HttpReqOptions options;
	options.customHeaders = { "Content-Type: application/json" };
	options.dataToPost = "{\"jsonrpc\": \"2.0\",\"id\": \"" + uuid + "\",\"method\": \"readers.write\",\"params\": {\"text\": \"" + name + "\"}}";
	
	HttpReq req("http://127.0.0.1:7497/api/v0.1", &options);
	return req.wait();
}
