#include "Random.h"

#include <chrono>

#include <spdlog/spdlog.h>

int OpenZod::Util::Random::Int(int min, int max)
{
    if(!seeded)
    {
        Init();
    }
    std::uniform_int_distribution<int> distribution(min, max);
    return distribution(rng);
}

bool OpenZod::Util::Random::Bool()
{
    return Int(0, 1) == 1;
}

float OpenZod::Util::Random::Float(float min, float max)
{
    if (!seeded)
    {
        Init();
    }
    std::uniform_real_distribution<float> distribution(min, max);
    return distribution(rng);
}

double OpenZod::Util::Random::Double(double min, double max)
{
    if (!seeded) 
    {
        Init();
    }
    std::uniform_real_distribution<double> distribution(min, max);
    return distribution(rng);
}

bool OpenZod::Util::Random::OneInX(int x)
{
    if (!seeded) 
    {
        Init();
    }
    std::uniform_int_distribution<int> distribution(1, x);
    return distribution(rng) == 1;
}

void OpenZod::Util::Random::Init()
{
    unsigned int seed = static_cast<unsigned int>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    spdlog::info("Seeding RNG with {}", seed);
    rng.seed(seed);
    seeded = true;
}