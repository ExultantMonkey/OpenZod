#pragma once

#include <random>

namespace OpenZod { namespace Util {
class Random 
{
public:
    static int Int(int min, int max);
    static bool Bool();
    static float Float(float min, float max);
    static double Double(double min, double max);
    static bool OneInX(int x);
private:
    inline static std::mt19937 rng;
    inline static bool seeded;
    static void Init();
};
}}