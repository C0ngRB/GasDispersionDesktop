#include "Simulator3D.h"
#include <cmath>
#include <algorithm>

/**
 * @file Simulator3D.cpp
 * @brief 三维气体扩散模拟器实现
 *
 * 【实现说明】
 * 实现基于有限差分法的三维瞬态气体扩散模拟。
 * 采用显示欧拉方法求解对流-扩散方程。
 *
 * 【核心算法】
 * 1. 对流项：上风差分（upwind differencing）
 *    - 保证数值稳定性（无数值振荡）
 *    - 方向由风速分量决定
 *
 * 2. 扩散项：中心差分（central differencing）
 *    - 二阶精度
 *    - 使用湍流扩散系数K
 *
 * 3. 时间推进：显式欧拉方法
 *    - 条件稳定，需满足CFL条件
 *    - 支持自动调整dt保证稳定
 *
 * 【网格索引约定】
 * - i: X方向索引，向右增加
 * - j: Y方向索引，向前增加
 * - k: Z方向索引，向上增加
 *
 * 【物理单位】
 * - 所有长度单位：米（m）
 * - 时间单位：秒（s）
 * - 浓度单位：kg/m³
 *
 * 【稳定性条件】
 * - 对流CFL: |u|·dt/dx ≤ 0.4
 * - 扩散CFL: K·dt/dx² ≤ 0.2
 */
namespace {
    constexpr double kPi = 3.1415926535897932384626433832795;

    /**
     * @brief 角度转弧度
     * @param deg 角度值（度）
     * @return 弧度值
     */
    inline double deg2rad(double deg) {
        return deg * kPi / 180.0;
    }
}

bool Simulator3D::initialize(const Grid3D& grid, const ITerrain* terrain, const Params& p, QString& errOut) {
    errOut.clear();

    // Step 1: 复制网格并验证
    grid_ = grid;
    try { grid_.validate(); }
    catch (const std::exception& e) { errOut = e.what(); return false; }

    // Step 2: 保存地形指针和参数副本
    terrain_ = terrain;
    p_ = p;

    // Step 3: 计算风速分量
    // 将风向角度转换为弧度，然后分解为X/Y分量
    // 风向定义：风从哪个方向吹来（气象惯例）
    const double rad = deg2rad(p_.windDir_deg);
    u_ = p_.windSpeed_mps * std::cos(rad);  // X方向分量
    v_ = p_.windSpeed_mps * std::sin(rad);  // Y方向分量
    w_ = 0.0;                                // 垂直分量（简化模型）

    // Step 4: 预分配浓度数组
    // 使用Ping-Pong策略：C_存储当前时刻，Cnew_存储下一时刻
    const std::size_t n = static_cast<std::size_t>(grid_.Nx) * grid_.Ny * grid_.Nz;
    C_.assign(n, 0.0f);
    Cnew_ = C_;

    // Step 5: 初始化固体掩码
    solid_.assign(n, 0);

    // Step 6: 构建固体掩码（标记地形以下的网格点）
    buildSolidMask();

    // Step 7: 初始化时间和最大浓度
    t_ = 0.0;
    maxC_ = 0.0f;

    return true;
}

void Simulator3D::reset() {
    // 将所有浓度数组置零
    std::fill(C_.begin(), C_.end(), 0.0f);
    std::fill(Cnew_.begin(), Cnew_.end(), 0.0f);
    t_ = 0.0;
    maxC_ = 0.0f;
}

void Simulator3D::buildSolidMask() {
    // 遍历所有网格点，查询地形高度并标记固体
    for (int j = 0; j < grid_.Ny; ++j) {
        const double y = grid_.y(j);
        for (int i = 0; i < grid_.Nx; ++i) {
            const double x = grid_.x(i);
            // 查询地形高度
            const float H = terrain_ ? terrain_->height(x, y) : 0.0f;

            // 标记地形以下的点为固体
            for (int k = 0; k < grid_.Nz; ++k) {
                const double z = grid_.z(k);
                solid_[grid_.idx(i,j,k)] = (z <= H) ? 1 : 0;
            }
        }
    }
}

float Simulator3D::sampleC(int i, int j, int k) const {
    // 边界检查：超出范围返回0（零梯度边界条件）
    if (i < 0 || i >= grid_.Nx || j < 0 || j >= grid_.Ny || k < 0 || k >= grid_.Nz) return 0.0f;
    // 返回浓度值（固体点的值始终为0）
    return C_[grid_.idx(i,j,k)];
}

double Simulator3D::stableDt() const {
    const double eps = 1e-12;
    const double cfl = 0.4;   // 对流CFL数（小于1保证稳定）
    const double diff = 0.2;  // 扩散CFL数（小于0.25保证稳定）

    // Step 1: 计算对流稳定时间步长
    // 对流CFL条件：dt <= CFL * dx / |u|
    double dt_adv = 1e9;
    if (std::abs(u_) > eps) dt_adv = std::min(dt_adv, cfl * grid_.dx / std::abs(u_));
    if (std::abs(v_) > eps) dt_adv = std::min(dt_adv, cfl * grid_.dy / std::abs(v_));
    if (std::abs(w_) > eps) dt_adv = std::min(dt_adv, cfl * grid_.dz / std::abs(w_));

    // Step 2: 计算扩散稳定时间步长
    // 扩散CFL条件：dt <= 0.25 * dx² / K
    double dt_diff = 1e9;
    if (p_.K_m2ps > eps) {
        // 取三个方向中最小的时间步长
        const double h2 = std::min({grid_.dx*grid_.dx, grid_.dy*grid_.dy, grid_.dz*grid_.dz});
        dt_diff = diff * h2 / (6.0 * p_.K_m2ps);  // 系数6=2*(1+1+1)，三维扩散
    }

    // Step 3: 返回两者的较小值
    return std::min(dt_adv, dt_diff);
}

double Simulator3D::step() {
    // Step 1: 确定时间步长
    double dt = p_.dt_s;
    const double dtStable = stableDt();
    // 如果启用自动调整，选择稳定的最小dt
    if (p_.autoClampDt && dtStable > 0 && dt > dtStable) dt = dtStable;

    // Step 2: 获取物理参数
    const double K = p_.K_m2ps;        // 扩散系数
    const double kdecay = p_.decay_1ps; // 衰减系数

    // Step 3: 计算源区域内的流体网格数
    const double r = std::max(0.0, p_.srcRadius_m);
    const double r2 = r * r;

    int nSrcCells = 0;
    for (int k = 0; k < grid_.Nz; ++k) {
        const double z = grid_.z(k);
        for (int j = 0; j < grid_.Ny; ++j) {
            const double y = grid_.y(j);
            for (int i = 0; i < grid_.Nx; ++i) {
                const double x = grid_.x(i);
                // 计算到源点的距离平方
                const double d2 = (x - p_.srcX_m)*(x - p_.srcX_m)
                                + (y - p_.srcY_m)*(y - p_.srcY_m)
                                + (z - p_.srcZ_m)*(z - p_.srcZ_m);
                // 如果在源半径内且不是固体，计数
                if (d2 <= r2 && !isSolid(i,j,k)) ++nSrcCells;
            }
        }
    }

    // Step 4: 计算每个源网格单元的浓度增量
    // 质量守恒：总源强 = 浓度增量 × 单元体积 × 源网格数
    const float sourceAdd = (nSrcCells > 0)
        ? static_cast<float>((p_.leakRate * dt) / (grid_.cellVolume() * nSrcCells))
        : 0.0f;

    // Step 5: 遍历所有网格点，更新浓度
    maxC_ = 0.0f;

    for (int k = 0; k < grid_.Nz; ++k) {
        for (int j = 0; j < grid_.Ny; ++j) {
            for (int i = 0; i < grid_.Nx; ++i) {
                const std::size_t id = grid_.idx(i,j,k);

                // 固体点浓度始终为0
                if (solid_[id]) {
                    Cnew_[id] = 0.0f;
                    continue;
                }

                const float Cc = C_[id];

                // ===== 对流通量：上风差分 =====
                // 风速为正时，用下游值；风速为负时，用上游值
                // 这样保证信息沿风向传递，数值稳定
                float dCdx = 0.0f;
                if (u_ >= 0) dCdx = (Cc - sampleC(i-1,j,k)) / static_cast<float>(grid_.dx);
                else         dCdx = (sampleC(i+1,j,k) - Cc) / static_cast<float>(grid_.dx);

                float dCdy = 0.0f;
                if (v_ >= 0) dCdy = (Cc - sampleC(i,j-1,k)) / static_cast<float>(grid_.dy);
                else         dCdy = (sampleC(i,j+1,k) - Cc) / static_cast<float>(grid_.dy);

                float dCdz = 0.0f;
                if (w_ >= 0) dCdz = (Cc - sampleC(i,j,k-1)) / static_cast<float>(grid_.dz);
                else         dCdz = (sampleC(i,j,k+1) - Cc) / static_cast<float>(grid_.dz);

                // ===== 扩散通量：中心差分 =====
                // 二阶导数中心差分：d²C/dx² ≈ (C(i+1) - 2C(i) + C(i-1)) / dx²
                const float d2Cdx2 = (sampleC(i+1,j,k) - 2.0f*Cc + sampleC(i-1,j,k)) / static_cast<float>(grid_.dx*grid_.dx);
                const float d2Cdy2 = (sampleC(i,j+1,k) - 2.0f*Cc + sampleC(i,j-1,k)) / static_cast<float>(grid_.dy*grid_.dy);
                const float d2Cdz2 = (sampleC(i,j,k+1) - 2.0f*Cc + sampleC(i,j,k-1)) / static_cast<float>(grid_.dz*grid_.dz);

                // ===== 计算各项贡献 =====
                // 对流项：-u·∇C (负号表示浓度沿梯度反方向变化)
                const float adv = static_cast<float>(-u_*dCdx - v_*dCdy - w_*dCdz);
                // 扩散项：K·∇²C
                const float dif = static_cast<float>( K * (d2Cdx2 + d2Cdy2 + d2Cdz2) );
                // 衰减项：-k·C
                const float dec = static_cast<float>(-kdecay * Cc);

                // ===== 欧拉时间推进 =====
                // C(n+1) = C(n) + dt * (对流 + 扩散 + 衰减)
                float Cn = Cc + static_cast<float>(dt) * (adv + dif + dec);

                // ===== 添加源项 =====
                if (sourceAdd > 0.0f) {
                    const double x = grid_.x(i);
                    const double y = grid_.y(j);
                    const double z = grid_.z(k);
                    const double d2 = (x - p_.srcX_m)*(x - p_.srcX_m)
                                    + (y - p_.srcY_m)*(y - p_.srcY_m)
                                    + (z - p_.srcZ_m)*(z - p_.srcZ_m);
                    if (d2 <= r2) Cn += sourceAdd;
                }

                // 浓度不能为负
                if (Cn < 0.0f) Cn = 0.0f;

                Cnew_[id] = Cn;
                if (Cn > maxC_) maxC_ = Cn;
            }
        }
    }

    // Step 6: Ping-Pong交换指针，更新时间
    C_.swap(Cnew_);
    t_ += dt;

    return t_;
}

void Simulator3D::extractSliceXY(int zIndex, std::vector<float>& outSlice, float& outMax) const {
    // Step 1: 钳制zIndex到有效范围
    zIndex = std::clamp(zIndex, 0, grid_.Nz - 1);

    // Step 2: 分配输出数组
    outSlice.assign(static_cast<std::size_t>(grid_.Nx) * grid_.Ny, 0.0f);
    outMax = 0.0f;

    // Step 3: 提取指定高度的切片
    for (int j = 0; j < grid_.Ny; ++j) {
        for (int i = 0; i < grid_.Nx; ++i) {
            const float v = C_[grid_.idx(i,j,zIndex)];
            outSlice[static_cast<std::size_t>(i) + static_cast<std::size_t>(j) * grid_.Nx] = v;
            outMax = std::max(outMax, v);
        }
    }
}
