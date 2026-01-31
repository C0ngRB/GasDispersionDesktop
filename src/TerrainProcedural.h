#pragma once
#include "ITerrain.h"
#include <vector>
#include <cmath>

class TerrainProcedural final : public ITerrain {
public:
    enum class Mode { Flat, GaussianHill, Ridge, MultiGaussian };

    struct Gaussian {
        double x0 = 0.0;
        double y0 = 0.0;
        double A = 50.0;
        double sigma = 200.0;
    };

    void setMode(Mode m) { mode_ = m; }
    void setBaseZ(float z0) { baseZ_ = z0; }
    void setGaussian(const Gaussian& g) { g1_ = g; }
    void setRidge(double x0, double A, double sigmaX) { ridgeX0_ = x0; ridgeA_ = A; ridgeSigmaX_ = sigmaX; }
    void setMultiGaussians(const std::vector<Gaussian>& gs) { gs_ = gs; }

    float height(double x, double y) const override {
        const double zBase = baseZ_;
        switch (mode_) {
        case Mode::Flat:
            return static_cast<float>(zBase);
        case Mode::GaussianHill: {
            const double dx = x - g1_.x0;
            const double dy = y - g1_.y0;
            const double r2 = dx*dx + dy*dy;
            const double s2 = g1_.sigma * g1_.sigma;
            const double h = g1_.A * std::exp(-r2 / s2);
            return static_cast<float>(zBase + h);
        }
        case Mode::Ridge: {
            const double dx = x - ridgeX0_;
            const double s2 = ridgeSigmaX_ * ridgeSigmaX_;
            const double h = ridgeA_ * std::exp(-(dx*dx) / s2);
            return static_cast<float>(zBase + h);
        }
        case Mode::MultiGaussian: {
            double hsum = 0.0;
            for (const auto& g : gs_) {
                const double dx = x - g.x0;
                const double dy = y - g.y0;
                const double r2 = dx*dx + dy*dy;
                const double s2 = g.sigma * g.sigma;
                hsum += g.A * std::exp(-r2 / s2);
            }
            return static_cast<float>(zBase + hsum);
        }
        }
        return static_cast<float>(zBase);
    }

private:
    Mode mode_{Mode::Flat};
    float baseZ_{0.0f};
    Gaussian g1_{};
    double ridgeX0_{0.0};
    double ridgeA_{50.0};
    double ridgeSigmaX_{200.0};
    std::vector<Gaussian> gs_;
};
