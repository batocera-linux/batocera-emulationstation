#include "Randomizer.h"

#include <random>
#include <sstream>
#include <iomanip>

std::random_device Randomizer::RandomDevice;
Randomizer* Randomizer::Instance = nullptr;

Randomizer::Randomizer() : mMt19937(RandomDevice()) { }

int Randomizer::random(int max)
{
	if (max <= 1)
		return 0;

	if (Instance == nullptr)
		Instance = new Randomizer();

	std::uniform_int_distribution<int> uniformDistribution(0, max - 1);
	return uniformDistribution(Instance->mMt19937);
}


std::string Randomizer::generateUUID()
{
	std::random_device seed_gen;
	std::mt19937 engine(seed_gen());
	std::uniform_int_distribution<> dist(0, 255);

	std::stringstream ss;
	ss << std::hex << std::setfill('0');

	// Format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
	for (int i = 0; i < 16; i++)
	{
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
