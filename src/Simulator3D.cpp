#include "Simulator3D.h"
#include <cmath>
#include <algorithm>

static inline double deg2rad(double deg) {
    constexpr double kPi = 3.1415926535897932384626433832795;
    return deg * kPi / 180.0;
}

bool Simulator3D::initialize(const Grid3D& grid, const TerrainDem& dem, const Params& p, QString& errOut) {
    errOut.clear();

    grid_ = grid;
    try { grid_.validate(); }
    catch (const std::exception& e) { errOut = e.what(); return false; }

    dem_ = &dem;
    p_ = p;

    const double rad = deg2rad(p_.windDir_deg);
    u_ = p_.windSpeed_mps * std::cos(rad);
    v_ = p_.windSpeed_mps * std::sin(rad);
    w_ = 0.0;

    const std::size_t n = static_cast<std::size_t>(grid_.Nx) * grid_.Ny * grid_.Nz;
    C_.assign(n, 0.0f);
    Cnew_ = C_;
    solid_.assign(n, 0);

    buildSolidMask(errOut);
    if (!errOut.isEmpty()) return false;

    t_ = 0.0;
    maxC_ = 0.0f;
    return true;
}

void Simulator3D::reset() {
    std::fill(C_.begin(), C_.end(), 0.0f);
    std::fill(Cnew_.begin(), Cnew_.end(), 0.0f);
    t_ = 0.0;
    maxC_ = 0.0f;
}

void Simulator3D::buildSolidMask(QString& errOut) {
    errOut.clear();
    if (!dem_) { errOut = "DEM is null"; return; }

    for (int j = 0; j < grid_.Ny; ++j) {
        const double y = grid_.y(j);
        for (int i = 0; i < grid_.Nx; ++i) {
            const double x = grid_.x(i);
            const float H = dem_->sampleBilinear(x, y);

            for (int k = 0; k < grid_.Nz; ++k) {
                const double z = grid_.z(k);
                solid_[grid_.idx(i,j,k)] = (z <= H) ? 1 : 0;
            }
        }
    }
}

float Simulator3D::sampleC(int i, int j, int k) const {
    if (i < 0 || i >= grid_.Nx || j < 0 || j >= grid_.Ny || k < 0 || k >= grid_.Nz) return 0.0f;
    if (isSolid(i,j,k)) return C_[grid_.idx(i,j,k)];
    return C_[grid_.idx(i,j,k)];
}

double Simulator3D::stableDt() const {
    const double eps = 1e-12;
    const double cfl = 0.4;
    const double diff = 0.2;

    double dt_adv = 1e9;
    if (std::abs(u_) > eps) dt_adv = std::min(dt_adv, cfl * grid_.dx / std::abs(u_));
    if (std::abs(v_) > eps) dt_adv = std::min(dt_adv, cfl * grid_.dy / std::abs(v_));
    if (std::abs(w_) > eps) dt_adv = std::min(dt_adv, cfl * grid_.dz / std::abs(w_));

    double dt_diff = 1e9;
    if (p_.K_m2ps > eps) {
        const double h2 = std::min({grid_.dx*grid_.dx, grid_.dy*grid_.dy, grid_.dz*grid_.dz});
        dt_diff = diff * h2 / (6.0 * p_.K_m2ps);
    }

    return std::min(dt_adv, dt_diff);
}

double Simulator3D::step() {
    double dt = p_.dt_s;
    const double dtStable = stableDt();
    if (p_.autoClampDt && dtStable > 0 && dt > dtStable) dt = dtStable;

    const double K = p_.K_m2ps;
    const double kdecay = p_.decay_1ps;

    const double r = std::max(0.0, p_.srcRadius_m);
    const double r2 = r * r;

    int nSrcCells = 0;
    for (int k = 0; k < grid_.Nz; ++k) {
        const double z = grid_.z(k);
        for (int j = 0; j < grid_.Ny; ++j) {
            const double y = grid_.y(j);
            for (int i = 0; i < grid_.Nx; ++i) {
                const double x = grid_.x(i);
                const double d2 = (x - p_.srcX_m)*(x - p_.srcX_m)
                                + (y - p_.srcY_m)*(y - p_.srcY_m)
                                + (z - p_.srcZ_m)*(z - p_.srcZ_m);
                if (d2 <= r2 && !isSolid(i,j,k)) ++nSrcCells;
            }
        }
    }

    const float sourceAdd = (nSrcCells > 0)
        ? static_cast<float>((p_.leakRate * dt) / (grid_.cellVolume() * nSrcCells))
        : 0.0f;

    maxC_ = 0.0f;

    for (int k = 0; k < grid_.Nz; ++k) {
        for (int j = 0; j < grid_.Ny; ++j) {
            for (int i = 0; i < grid_.Nx; ++i) {
                const std::size_t id = grid_.idx(i,j,k);

                if (solid_[id]) {
                    Cnew_[id] = 0.0f;
                    continue;
                }

                const float Cc = C_[id];

                float dCdx = 0.0f;
                if (u_ >= 0) dCdx = (Cc - sampleC(i-1,j,k)) / static_cast<float>(grid_.dx);
                else         dCdx = (sampleC(i+1,j,k) - Cc) / static_cast<float>(grid_.dx);

                float dCdy = 0.0f;
                if (v_ >= 0) dCdy = (Cc - sampleC(i,j-1,k)) / static_cast<float>(grid_.dy);
                else         dCdy = (sampleC(i,j+1,k) - Cc) / static_cast<float>(grid_.dy);

                float dCdz = 0.0f;
                if (w_ >= 0) dCdz = (Cc - sampleC(i,j,k-1)) / static_cast<float>(grid_.dz);
                else         dCdz = (sampleC(i,j,k+1) - Cc) / static_cast<float>(grid_.dz);

                const float d2Cdx2 = (sampleC(i+1,j,k) - 2.0f*Cc + sampleC(i-1,j,k)) / static_cast<float>(grid_.dx*grid_.dx);
                const float d2Cdy2 = (sampleC(i,j+1,k) - 2.0f*Cc + sampleC(i,j-1,k)) / static_cast<float>(grid_.dy*grid_.dy);
                const float d2Cdz2 = (sampleC(i,j,k+1) - 2.0f*Cc + sampleC(i,j,k-1)) / static_cast<float>(grid_.dz*grid_.dz);

                const float adv = static_cast<float>(-u_*dCdx - v_*dCdy - w_*dCdz);
                const float dif = static_cast<float>( K * (d2Cdx2 + d2Cdy2 + d2Cdz2) );
                const float dec = static_cast<float>(-kdecay * Cc);

                float Cn = Cc + static_cast<float>(dt) * (adv + dif + dec);

                if (sourceAdd > 0.0f) {
                    const double x = grid_.x(i);
                    const double y = grid_.y(j);
                    const double z = grid_.z(k);
                    const double d2 = (x - p_.srcX_m)*(x - p_.srcX_m)
                                    + (y - p_.srcY_m)*(y - p_.srcY_m)
                                    + (z - p_.srcZ_m)*(z - p_.srcZ_m);
                    if (d2 <= r2) Cn += sourceAdd;
                }

                if (Cn < 0.0f) Cn = 0.0f;
                Cnew_[id] = Cn;
                if (Cn > maxC_) maxC_ = Cn;
            }
        }
    }

    C_.swap(Cnew_);
    t_ += dt;
    return t_;
}

void Simulator3D::extractSliceXY(int zIndex, std::vector<float>& outSlice, float& outMax) const {
    zIndex = std::clamp(zIndex, 0, grid_.Nz - 1);
    outSlice.assign(static_cast<std::size_t>(grid_.Nx) * grid_.Ny, 0.0f);
    outMax = 0.0f;

    for (int j = 0; j < grid_.Ny; ++j) {
        for (int i = 0; i < grid_.Nx; ++i) {
            const float v = C_[grid_.idx(i,j,zIndex)];
            outSlice[static_cast<std::size_t>(i) + static_cast<std::size_t>(j) * grid_.Nx] = v;
            outMax = std::max(outMax, v);
        }
    }
}
