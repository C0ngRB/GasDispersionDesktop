#pragma once
#include "ITerrain.h"

class TerrainFlat final : public ITerrain {
public:
    explicit TerrainFlat(float z0 = 0.0f) : z0_(z0) {}
    void setZ0(float z0) { z0_ = z0; }
    float height(double, double) const override { return z0_; }
private:
    float z0_{0.0f};
};
