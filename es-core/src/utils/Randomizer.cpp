#include "Randomizer.h"

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


