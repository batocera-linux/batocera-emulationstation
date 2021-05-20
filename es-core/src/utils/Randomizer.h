#pragma once

#include <random>

class Randomizer
{
public:
	static int random(int max);

private:
	Randomizer();

	static std::random_device RandomDevice;
	static Randomizer*	Instance;
	std::mt19937		mMt19937;
};
