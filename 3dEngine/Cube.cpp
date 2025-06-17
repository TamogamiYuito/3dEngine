#include "Cube.hpp"
#include <cmath>

int gIdx(double v) { return (int)std::round(v / (2 * HALF)); }
double gPos(int g) { return g * 2.0 * HALF; }
