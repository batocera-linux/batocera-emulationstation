#pragma once

#include <random>
#include <string>
#include <cstring>

class Randomizer
{
public:
	static int random(int max);
	static std::string generateUUID();

private:
	Randomizer();

	static std::random_device RandomDevice;
	static Randomizer*	Instance;
	std::mt19937		mMt19937;
};
