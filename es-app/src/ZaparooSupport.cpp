#include "ZaparooSupport.h"
#include "HttpReq.h"
#include "Log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <sys/wait.h>

#include <random>
#include <sstream>
#include <iomanip>

std::string generateUUID() {
    std::random_device seed_gen;
    std::mt19937 engine(seed_gen());
    std::uniform_int_distribution<> dist(0, 255);
    
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    
    // Format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    for (int i = 0; i < 16; i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            ss << '-';
        
        int byte = dist(engine);
        
        // Version 4 UUID (set version bits)
        if (i == 6)
            byte = (byte & 0x0F) | 0x40;
        // Variant bits
        if (i == 8)
            byte = (byte & 0x3F) | 0x80;
            
        ss << std::setw(2) << byte;
    }
    
    return ss.str();
}

static HttpReqOptions getHttpOptionsZaparoo()
{
	HttpReqOptions options;
    
	return options;
}


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

bool Zaparoo::isZaparooEnabled()
{
    HttpReqOptions options = getHttpOptionsZaparoo();
    options.connectTimeout = 250L;
    HttpReq req("http://127.0.0.1:7497/health", &options);

    if (req.wait()) {
        std::string data = req.getContent();
        return data.compare("OK") == 0;
    }
    return false;
}

bool Zaparoo::writeZaparooCard(std::string name)
{
    HttpReqOptions options = getHttpOptionsZaparoo();
    std::string uuid = generateUUID();
    LOG(LogInfo) << "Zaparoo::writeZaparooCard -> " << uuid;
    options.customHeaders = {"Content-Type: application/json"};
    options.dataToPost = "{\"jsonrpc\": \"2.0\",\"id\": \"" + uuid + "\",\"method\": \"readers.write\",\"params\": {\"text\": \"" + name + "\"}}";
    HttpReq req("http://127.0.0.1:7497/api/v0.1", &options);
    return req.wait();
}
