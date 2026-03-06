#pragma once
/**
 * @file MainWindow.h
 * @brief GasDispersionDesktop 主窗口
 *
 * 【设计目的】
 * 提供Qt GUI界面，让用户能够：
 * 1. 选择和配置地形（Flat/DEM/Procedural）
 * 2. 设置模拟参数（风场、扩散、源项、时间）
 * 3. 预览地形和气体扩散结果
 * 4. 导出仿真数据（CSV格式）
 *
 * 【UI布局】
 * 采用水平分割布局（QSplitter）：
 * - 左侧：可滚动的参数配置面板（QScrollArea）
 *   · Terrain分组：地形模式选择和参数
 *   · Domain分组：笛卡尔网格参数
 *   · Vertical分组：垂直网格和切片设置
 *   · Physics分组：风场和扩散参数
 *   · Time & Output分组：时间步长和导出设置
 *   · Visualization分组：可视化模式选择
 *   · 3D Source分组：泄漏源位置和强度
 *   · Control分组：运行/暂停/重置按钮和日志
 *
 * - 右侧：可视化显示区域
 *   · 上部：地形/浓度切片渲染视图（QLabel）
 *   · 底部：状态栏，显示当前模拟状态
 *
 * 【坐标系】
 * 与Simulator3D一致：
 * - X: 右东
 * - Y: 向前北（地图惯例）
 * - Z: 向上
 * - 单位：米
 *
 * 【数据流】
 * 用户操作 → UI控件 → MainWindow成员变量
 *                                    ↓
 *                         buildSimulation()
 *                                    ↓
 *                         PlumeEngine::initialize()
 *                                    ↓
 *                         onTick() [定时器回调]
 *                                    ↓
 *                         PlumeEngine::step()
 *                                    ↓
 *                         PlumeEngine::extractSliceXY()
 *                                    ↓
 *                         renderTerrainAndSlice()
 *                                    ↓
 *                         QLabel显示
 *
 * 【关键功能】
 * - 地形预览：点击Preview Terrain后显示地形渲染
 * - 仿真控制：Run/Pause/Reset按钮
 * - 自动居中：非DEM模式下自动将源点移到域中心
 * - 双切片导出：同时导出zslice和agl两种高度的浓度
 * - 背景模式：可选地形灰度背景或纯蓝背景
 * - 模型选择：CFD / Gaussian plume / Gaussian puff
 */

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QLabel>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QScrollArea>
#include <QTimer>

#include <vector>

#include "TerrainDem.h"
#include "TerrainFlat.h"
#include "TerrainProcedural.h"
#include "PlumeEngine.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    enum class TerrainMode
    {
        Flat = 0,
        Dem = 1,
        Procedural = 2,
    };

    TerrainMode terrainMode() const;
    const ITerrain *currentTerrain() const;

    QString framesDir() const;

    PlumeEngine::ModelType selectedModelType() const;
    QString selectedModelTag() const;

    void appendLog(const QString &s);

    void updateDomainInfo();
    void enforceSourceCenterIfNeeded();

    bool buildTerrainPreview(QString &err);
    bool buildSimulation(QString &err);

    void renderTerrainOnly();
    void renderTerrainAndSlice();

    int zToK(double zWorld) const;
    double groundAtSource() const;
    double sliceZ() const;

private slots:
    void onPickDemClicked();
    void onConvertLoadDemClicked();
    void onPreviewTerrainClicked();

    void onTerrainParamsChanged();
    void onFollowSliceToggled(bool);

    void onSetSrcZFromGroundClicked();

    void onRunClicked();
    void onPauseClicked();
    void onResetClicked();
    void onModelChanged(int);
    void onTick();

private:
    // left UI
    QScrollArea *leftScroll_ = nullptr;
    QComboBox *cbTerrainMode_ = nullptr;

    QDoubleSpinBox *sbFlatZ_ = nullptr;
    QDoubleSpinBox *sbProcBaseZ_ = nullptr;
    QDoubleSpinBox *sbProcPeakA_ = nullptr;
    QDoubleSpinBox *sbProcSigma_ = nullptr;

    QLineEdit *leDemTif_ = nullptr;
    QPushButton *btnPickDem_ = nullptr;
    QPushButton *btnConvertLoadDem_ = nullptr;
    QPushButton *btnPreviewTerrain_ = nullptr;

    QDoubleSpinBox *sbX0_ = nullptr;
    QDoubleSpinBox *sbY0_ = nullptr;
    QDoubleSpinBox *sbLx_ = nullptr;
    QDoubleSpinBox *sbLy_ = nullptr;
    QDoubleSpinBox *sbDx_ = nullptr;
    QDoubleSpinBox *sbDy_ = nullptr;
    QLabel *domainInfo_ = nullptr;

    QDoubleSpinBox *sbDz_ = nullptr;
    QDoubleSpinBox *sbZTopMargin_ = nullptr;
    QSpinBox *sbNzMax_ = nullptr;

    QDoubleSpinBox *sbZSlice_ = nullptr;
    QCheckBox *cbFollowSlice_ = nullptr;

    QComboBox *cbModel_ = nullptr;
    QDoubleSpinBox *sbWindSpeed_ = nullptr;
    QDoubleSpinBox *sbWindDir_ = nullptr;
    QDoubleSpinBox *sbK_ = nullptr;
    QDoubleSpinBox *sbDecay_ = nullptr;

    QDoubleSpinBox *sbTotalTime_ = nullptr;
    QDoubleSpinBox *sbDt_ = nullptr;
    QCheckBox *cbAutoClampDt_ = nullptr;
    QCheckBox *cbExportCsv_ = nullptr;
    QDoubleSpinBox *sbExportInterval_ = nullptr;
    QCheckBox *cbExportTwoSlices_ = nullptr;

    QComboBox *cbBgMode_ = nullptr;
    QDoubleSpinBox *sbRelCut_ = nullptr;

    QDoubleSpinBox *sbSrcX_ = nullptr;
    QDoubleSpinBox *sbSrcY_ = nullptr;
    QDoubleSpinBox *sbSrcZ_ = nullptr;
    QDoubleSpinBox *sbSrcR_ = nullptr;
    QDoubleSpinBox *sbLeakRate_ = nullptr;
    QPushButton *btnSetSrcZFromGround_ = nullptr;
    QCheckBox *cbAutoCenterSrc_ = nullptr;

    QPushButton *btnRun_ = nullptr;
    QPushButton *btnPause_ = nullptr;
    QPushButton *btnReset_ = nullptr;

    QPlainTextEdit *log_ = nullptr;

    // right UI
    QLabel *view_ = nullptr;
    QLabel *status_ = nullptr;

    QTimer *timer_ = nullptr;

    // terrain data
    TerrainFlat flat_;
    TerrainProcedural proc_;
    TerrainDem dem_;
    bool hasDem_ = false;

    bool terrainPreviewReady_ = false;
    int tNx_ = 0, tNy_ = 0;
    double tx0_ = 0, ty0_ = 0;
    double tdx_ = 1, tdy_ = 1;
    std::vector<float> terrainZ_;
    double tMin_ = 0, tMax_ = 0;

    // simulation state
    PlumeEngine engine_;
    bool simReady_ = false;
    bool running_ = false;

    double nextExportT_ = 0.0;
    int exportFrameIdx_ = 0;
};
