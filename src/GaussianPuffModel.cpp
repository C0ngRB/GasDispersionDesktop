#include "GaussianPuffModel.h"

#include <algorithm>
#include <cmath>

static inline double deg2rad(double d) { return d * M_PI / 180.0; }

bool GaussianPuffModel::initialize(const Grid3D &g, const ITerrain *terr, const Params &p, QString &err)
{
    (void)err;
    grid_ = g;
    terr_ = terr;
    params_ = p;
    t_ = 0.0;
    lastReleaseT_ = 0.0;
    puffs_.clear();

    const double th = deg2rad(params_.windDir_deg);
    u_ = params_.windSpeed_mps * std::cos(th);
    v_ = params_.windSpeed_mps * std::sin(th);

    if (params_.K_m2ps <= 0.0)
        params_.K_m2ps = 1e-6;

    if (params_.releaseInterval_s <= 0.0)
        params_.releaseInterval_s = params_.dt_s;

    return true;
}

void GaussianPuffModel::reset()
{
    t_ = 0.0;
    lastReleaseT_ = 0.0;
    puffs_.clear();
}

double GaussianPuffModel::time() const { return t_; }
const Grid3D &GaussianPuffModel::grid() const { return grid_; }
GaussianPuffModel::Params GaussianPuffModel::params() const { return params_; }

double GaussianPuffModel::terrainHeight(double x, double y) const
{
    if (!terr_)
        return 0.0;
    return terr_->height(x, y);
}

void GaussianPuffModel::emitPuff()
{
    Puff pf;
    pf.x = params_.srcX_m;
    pf.y = params_.srcY_m;
    pf.z = params_.srcZ_m;
    pf.t_emit = t_;
    pf.mass = params_.leakRate_kgps * params_.releaseInterval_s;

    pf.sigma0 = std::max(0.5 * params_.srcRadius_m, 0.1);

    puffs_.push_back(pf);
}

void GaussianPuffModel::step()
{
    t_ += params_.dt_s;

    while (t_ - lastReleaseT_ >= params_.releaseInterval_s - 1e-12)
    {
        lastReleaseT_ += params_.releaseInterval_s;
        emitPuff();
    }

    const double lam = params_.decay_1ps;
    for (auto &pf : puffs_)
    {
        const double age = t_ - pf.t_emit;
        if (age < 0.0)
            continue;

        pf.x = params_.srcX_m + u_ * age;
        pf.y = params_.srcY_m + v_ * age;

        if (lam > 0.0)
            pf.mass_eff = pf.mass * std::exp(-lam * age);
        else
            pf.mass_eff = pf.mass;
    }

    const double maxAge = std::max(params_.totalTime_s, 60.0);
    puffs_.erase(std::remove_if(puffs_.begin(), puffs_.end(),
                               [&](const Puff &pf)
                               {
                                   return (t_ - pf.t_emit) > maxAge;
                               }),
                 puffs_.end());
}

double GaussianPuffModel::puffSigma(const Puff &pf, double age) const
{
    const double s2 = pf.sigma0 * pf.sigma0 + 2.0 * params_.K_m2ps * std::max(0.0, age);
    return std::sqrt(std::max(1e-12, s2));
}

double GaussianPuffModel::puffContribution(const Puff &pf, double x, double y, double z) const
{
    const double age = t_ - pf.t_emit;
    if (age <= 0.0)
        return 0.0;

    const double h = terrainHeight(x, y);
    if (z < h)
        return 0.0;

    const double h0 = terrainHeight(params_.srcX_m, params_.srcY_m);

    const double sig = puffSigma(pf, age);

    const double dx = x - pf.x;
    const double dy = y - pf.y;
    const double dz = z - pf.z;

    const double inv2s2 = 1.0 / (2.0 * sig * sig);

    auto core = [&](double dzRel)
    {
        const double r2 = dx * dx + dy * dy + dzRel * dzRel;
        const double norm = pf.mass_eff / (std::pow(2.0 * M_PI, 1.5) * sig * sig * sig);
        return norm * std::exp(-r2 * inv2s2);
    };

    const double z_mirror = 2.0 * h0 - pf.z;
    const double dz_m = z - z_mirror;

    const double c = core(dz) + core(dz_m);

    return std::max(0.0, c);
}

void GaussianPuffModel::extractSliceXY(int k, std::vector<float> &out, float &maxC) const
{
    const int Nx = grid_.Nx;
    const int Ny = grid_.Ny;
    out.assign(Nx * Ny, 0.0f);
    maxC = 0.0f;

    const double z = grid_.z0 + k * grid_.dz;

    for (int j = 0; j < Ny; ++j)
    {
        const double y = grid_.y0 + j * grid_.dy;
        for (int i = 0; i < Nx; ++i)
        {
            const double x = grid_.x0 + i * grid_.dx;

            double c = 0.0;
            for (const auto &pf : puffs_)
                c += puffContribution(pf, x, y, z);

            const float cf = (float)c;
            out[j * Nx + i] = cf;
            maxC = std::max(maxC, cf);
        }
    }
}
