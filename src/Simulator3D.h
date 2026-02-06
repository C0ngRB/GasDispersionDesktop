#pragma once
#include <vector>
#include <cstdint>
#include <QString>

#include "Grid3D.h"
#include "ITerrain.h"

/**
 * @file Simulator3D.h
 * @brief 三维气体扩散模拟器
 *
 * 【设计目的】
 * 实现基于有限差分法的三维稳态/瞬态气体扩散模拟。
 * 采用欧拉方法求解对流-扩散方程（Advection-Diffusion Equation），
 * 支持平地和复杂地形（通过ITerrain接口）的模拟场景。
 *
 * 【物理模型】
 * 控制方程 - 三维瞬态对流-扩散方程：
 * ∂C/∂t + u·∇C = D·∇²C - k·C + S
 *
 * 其中：
 * - C: 气体浓度 (kg/m³)
 * - u: 风速矢量 (m/s)，由windSpeed和windDir确定
 * - D: 湍流扩散系数 (m²/s)，由参数K控制
 * - k: 一阶衰减系数 (1/s)，用于模拟化学反应或沉降
 * - S: 源项 (kg/m³/s)，由泄漏源定义
 *
 * 【坐标系约定】
 * - 右手笛卡尔坐标系：X(东), Y(北), Z(上)
 * - 原点由Grid3D::x0, y0, z0定义
 * - Z=0 通常接近海平面或地形基准面
 *
 * 【单位】
 * - 距离：米（m）
 * - 时间：秒（s）
 * - 浓度：千克/立方米（kg/m³）
 * - 源强：千克/秒（kg/s）或浓度增量
 * - 扩散系数：平方米/秒（m²/s）
 * - 衰减系数：1/秒（1/s）
 *
 * 【边界条件】
 * - 底部（Z=z0）：固体边界（地形表面），法向速度=0，浓度通量=0
 * - 顶部（Z=zTop）：开放边界，允许气体自由流出
 * - 侧面（X/Y边界）：开放边界，适合平流主导的流动
 *
 * 【求解方法】
 * - 时间离散化：显式欧拉前向差分（条件稳定，需满足CFL条件）
 * - 空间离散化：中心差分（扩散项）+ 上风差分（对流项，保证数值稳定）
 * - 源项处理：每次时间步在源点网格添加浓度
 *
 * 【与地形的交互】
 * - buildSolidMask(): 查询ITerrain获取每个( i,j )处的地形高度
 * - 地形以下的所有网格点标记为固体（solid=1）
 * - 固体单元格不参与浓度计算，气体无法穿透
 *
 * 【数据流】
 * MainWindow
 *   → readSimParams() 读取用户参数
 *   → buildSimulation() 构建Grid3D + 调用Simulator3D::initialize()
 *   → Simulator3D 内部：
 *       - initialize(): 构建网格、固体掩码、预分配内存
 *       - step(): 推进一个时间步（多次迭代以满足稳定性）
 *       - extractSliceXY(): 提取指定高度层的二维浓度切片
 *   → MainWindow::renderTerrainAndSlice() 可视化
 *   → MainWindow::onTick() 导出CSV帧
 */
class Simulator3D {
public:
    /**
     * @brief 模拟参数结构体
     *
     * 【物理参数说明】
     * - totalTime_s: 模拟总时长，决定气体扩散的最远距离（约等于 windSpeed × totalTime）
     * - dt_s: 时间步长，需满足CFL条件：dt < dx / (|u| + 2D/dx)
     * - autoClampDt: 是否自动调整dt以保持数值稳定（推荐开启）
     *
     * 【风场参数】
     * - windSpeed_mps: 风速大小（米/秒），典型值 1-10 m/s（微风到强风）
     * - windDir_deg: 风向（度），0=东，90=北，180=西，270=南
     *   风向定义：风从哪个方向吹来（气象惯例）
     *   例如：东风(windDir=90)表示风从东向西吹
     *
     * 【扩散参数】
     * - K_m2ps: 湍流扩散系数（米²/秒），典型值 0.1-10 m²/s
     *   - K=0: 仅对流（平流主导，气体沿风向形成狭窄羽流）
     *   - K>0: 湍流扩散使气体向横向扩展
     *   - 经验值：稳定大气K较小，不稳定大气K较大
     *
     * - decay_1ps: 一阶衰减系数（1/秒），用于模拟：
     *   - 化学反应（气体分解）
     *   - 干沉降（气体被地面吸收）
     *   - 湿沉降（降水冲刷）
     *   - decay=0: 无衰减，质量守恒
     *
     * 【源项参数】
     * - srcX_m, srcY_m, srcZ_m: 泄漏源在网格中的位置（米）
     *   - srcZ_m 通常设置为 terrain(x,y) + height above ground (AGL)
     * - srcRadius_m: 源的作用半径（米），用于在源点附近添加浓度
     * - leakRate: 泄漏强度（kg/s或浓度单位）
     */
    struct Params {
        double totalTime_s = 60.0;   ///< 模拟总时长（秒）
        double dt_s = 0.05;          ///< 时间步长（秒）
        bool autoClampDt = true;     ///< 自动调整dt保持稳定

        double windSpeed_mps = 2.0;  ///< 风速大小（米/秒）
        double windDir_deg = 0.0;    ///< 风向（度，从正北顺时针，0=东）

        double K_m2ps = 1.0;         ///< 湍流扩散系数（米²/秒）
        double decay_1ps = 0.0;     ///< 一阶衰减系数（1/秒）

        double srcX_m = 0.0;        ///< 源点X坐标（米）
        double srcY_m = 0.0;        ///< 源点Y坐标（米）
        double srcZ_m = 0.0;        ///< 源点Z坐标（米，相对于海平面）
        double srcRadius_m = 2.0;    ///< 源作用半径（米）
        double leakRate = 1.0;       ///< 源强（kg/s或等效浓度单位）
    };

    Simulator3D() = default;

    /**
     * @brief 初始化模拟器
     * @param grid 计算网格定义
     * @param terrain 地形数据接口（用于构建固体掩码）
     * @param p 模拟参数
     * @param errOut 错误信息输出
     * @return true=成功，false=失败（错误信息见errOut）
     *
     * 【初始化步骤】
     * 1. 验证网格参数有效性（Grid3D::validate()）
     * 2. 复制网格和参数副本
     * 3. 构建固体掩码：标记地形以下的网格点为固体
     * 4. 预分配浓度数组 C_ 和 Cnew_（大小 = Nx×Ny×Nz）
     * 5. 初始化时间为0，最大浓度为0
     *
     * 【调用时机】
     * 在MainWindow用户点击"Run"后调用，
     * 前提是地形预览已构建（buildTerrainPreview成功）
     */
    bool initialize(const Grid3D& grid, const ITerrain* terrain, const Params& p, QString& errOut);

    /**
     * @brief 重置模拟状态
     *
     * 【重置内容】
     * - 时间 t 归零
     * - 浓度场 C_ 全部置零
     * - 最大浓度 maxC_ 归零
     * - 不释放内存，便于重新开始模拟
     *
     * 【调用时机】
     * - 用户点击"Reset"按钮
     * - 修改参数后需要重新开始
     */
    void reset();

    /**
     * @brief 推进一个时间步
     * @return 推进后的模拟时间（秒）
     *
     * 【内部算法】
     * 1. 检查是否达到总时间（若达到返回当前时间，不推进）
     * 2. 根据autoClampDt决定是否调整dt
     * 3. 执行若干次子步迭代（每个子步为物理时间的dt）
     *    - 对每个非固体网格点：
     *      · 计算对流通量（上风差分，保证稳定性）
     *      · 计算扩散通量（中心差分）
     *      · 添加衰减项
     *    - 在源点网格添加泄漏浓度
     *    - 交换新旧浓度数组
     * 4. 更新最大浓度
     *
     * 【稳定性条件】
     * 显式方法需满足CFL条件：
     * - 对流CFL: |u|·dt/dx ≤ 1
     * - 扩散CFL: D·dt/dx² ≤ 0.25
     *
     * autoClampDt 会自动调整dt以满足上述条件
     */
    double step();

    /**
     * @brief 获取当前稳定时间步长
     * @return 在当前风速和网格下保持数值稳定的最dt（秒）
     *
     * 【计算公式】
     * 基于CFL条件的理论最大值：
     * dt_stable = min(dx/|u|, dx²/(4D), dy²/(4D), dz²/(4D))
     *
     * 【用途】
     * - 给用户建议合理的时间步长
     * - autoClampDt模式会自动采用此值
     */
    double stableDt() const;

    /// @brief 获取计算网格（只读）
    const Grid3D& grid() const { return grid_; }

    /// @brief 获取模拟参数（只读）
    const Params& params() const { return p_; }

    /// @brief 获取当前模拟时间（秒）
    double time() const { return t_; }

    /// @brief 获取当前浓度场的最大浓度值
    float maxC() const { return maxC_; }

    /**
     * @brief 提取指定高度层的二维浓度切片
     * @param zIndex Z方向网格索引 [0, Nz-1]
     * @param outSlice 输出的二维浓度数组（大小 Nx×Ny）
     * @param outMax 输出的切片最大浓度值
     *
     * 【使用场景】
     * MainWindow::renderTerrainAndSlice() 用于可视化
     * MainWindow::onTick() 用于导出CSV帧
     *
     * 【数据组织】
     * outSlice[i + Nx*j] 表示 (i,j,zIndex) 处的浓度
     */
    void extractSliceXY(int zIndex, std::vector<float>& outSlice, float& outMax) const;

    /**
     * @brief 查询指定位置的地形高度
     * @param x X坐标（米）
     * @param y Y坐标（米）
     * @return 地形高度（米，相对于海平面）
     *
     * 【封装说明】
     * 封装ITerrain::height()接口，提供空指针安全处理
     */
    float groundZ(double x, double y) const {
        return terrain_ ? terrain_->height(x, y) : 0.0f;
    }

private:
    Grid3D grid_;                 ///< 计算网格副本
    Params p_;                    ///< 模拟参数副本
    const ITerrain* terrain_{nullptr};  ///< 地形数据接口指针

    double t_{0.0};              ///< 当前模拟时间（秒）

    double u_{0.0}, v_{0.0}, w_{0.0};  ///< 风速分量（米/秒）
                                    // u: X方向分量 = windSpeed * cos(windDir)
                                    // v: Y方向分量 = windSpeed * sin(windDir)
                                    // w: 垂直分量 = 0（简化模型，不考虑垂直风）

    std::vector<float> C_;       ///< 当前时刻的浓度场（kg/m³）
                                // 存储顺序：idx(i,j,k) = i + Nx*(j + Ny*k)

    std::vector<float> Cnew_;    ///< 下一时刻的浓度场（用于交替更新）
                                // 使用Ping-Pong策略避免额外数组复制

    std::vector<std::uint8_t> solid_;  ///< 固体掩码（1=固体，0=流体）
                                // 地形以下的网格点标记为固体

    float maxC_{0.0f};           ///< 当前浓度场的最大浓度值（用于归一化显示）

    /**
     * @brief 判断指定网格是否为固体
     * @param i X方向索引
     * @param j Y方向索引
     * @param k Z方向索引
     * @return true=固体，false=流体
     *
     * 【使用场景】
     * step() 函数中对每个网格点调用，跳过固体单元格的计算
     */
    inline bool isSolid(int i, int j, int k) const {
        return solid_[grid_.idx(i,j,k)] != 0;
    }

    /**
     * @brief 安全获取浓度值（考虑边界）
     * @param i X方向索引
     * @param j Y方向索引
     * @param k Z方向索引
     * @return 浓度值，边界外返回0
     *
     * 【边界处理】
     * - 索引超出范围：返回0（零梯度边界条件）
     * - 固体单元格：调用者应已检查isSolid()
     */
    float sampleC(int i, int j, int k) const;

    /**
     * @brief 构建固体掩码
     *
     * 【算法说明】
     * 遍历所有 (i,j,k) 网格点：
     * 1. 查询ITerrain获取地形高度 h = terrain(x(i), y(j))
     * 2. 如果 z(k) < h，标记为固体（气体无法穿透地形）
     *
     * 【优化】
     * - 同一 (i,j) 处的所有 k 中，z < terrain 的点连续
     * - 可优化为二分查找或预计算阈值数组
     */
    void buildSolidMask();
};
