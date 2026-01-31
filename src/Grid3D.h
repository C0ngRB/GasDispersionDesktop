#pragma once
#include <algorithm>
#include <cstddef>
#include <stdexcept>

struct Grid3D {
    int Nx = 0, Ny = 0, Nz = 0;
    double x0 = 0.0, y0 = 0.0, z0 = 0.0;
    double dx = 1.0, dy = 1.0, dz = 1.0;

    double cellVolume() const { return dx * dy * dz; }

    inline std::size_t idx(int i, int j, int k) const {
        return static_cast<std::size_t>(i)
             + static_cast<std::size_t>(Nx) * (static_cast<std::size_t>(j)
             + static_cast<std::size_t>(Ny) * static_cast<std::size_t>(k));
    }

    void validate() const {
        if (Nx <= 1 || Ny <= 1 || Nz <= 1) throw std::runtime_error("Grid3D: Nx/Ny/Nz must be > 1");
        if (dx <= 0 || dy <= 0 || dz <= 0) throw std::runtime_error("Grid3D: dx/dy/dz must be > 0");
    }

    double x(int i) const { return x0 + dx * i; }
    double y(int j) const { return y0 + dy * j; }
    double z(int k) const { return z0 + dz * k; }
};
