/**
 * @file MainWindow.cpp
 * @brief GasDispersionDesktop 主窗口实现
 *
 * 【实现说明】
 * 实现MainWindow类的所有功能，包括：
 * - UI构建和布局管理
 * - 参数验证和地形预览
 * - 仿真生命周期管理
 * - 可视化渲染（地形+浓度切片）
 * - 数据导出
 *
 * 【UI组件组织】
 * 使用Qt Designer风格的代码布局：
 * 1. 构造函数：创建所有UI组件并建立信号槽连接
 * 2. 地形相关槽函数：处理DEM加载、预览等
 * 3. 仿真控制槽函数：Run/Pause/Reset
 * 4. 渲染函数：buildTerrainPreview, renderTerrainOnly, renderTerrainAndSlice
 * 5. 辅助函数：坐标系转换、单位转换等
 *
 * 【渲染流程】
 * 1. buildTerrainPreview():
 *    - 根据地形模式选择数据源
 *    - 构建二维高程数组
 *    - 计算高程范围（minZ, maxZ）
 *
 * 2. renderTerrainOnly():
 *    - 对每个像素计算对应的网格位置
 *    - 使用hillshade算法渲染地形
 *    - 添加固定padding(0.35)拉远视野
 *
 * 3. renderTerrainAndSlice():
 *    - 调用PlumeEngine::extractSliceXY()获取浓度切片
 *    - 根据背景模式选择底色（地形灰度/纯蓝）
 *    - 使用colorMap()将浓度映射为颜色
 *    - 半透明叠加显示
 *
 * 【坐标系转换】
 * 网格坐标 (i,j) → 世界坐标 (x,y):
 *   x = x0 + dx * i
 *   y = y0 + dy * j
 *
 * 像素坐标 (px,py) → 网格坐标 (i,j):
 *   i = round(px * scale)
 *   j = round(py * scale)
 *
 * 【与PlumeEngine的交互】
 * - buildSimulation(): 创建Grid3D，调用PlumeEngine::initialize()
 * - onTick(): 调用PlumeEngine::step()推进仿真
 * - renderTerrainAndSlice(): 调用PlumeEngine::extractSliceXY()获取数据
 */

#include "MainWindow.h"
#include "ColorMap.h"
#include "ExporterCsv.h"

#include <QtWidgets/QWidget>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QFileDialog>

#include <QDir>
#include <QPainter>
#include <QVariant>

#include <algorithm>
#include <cmath>
#include <limits>

static QDoubleSpinBox* makeDsb(double minV, double maxV, double step, double val, int decimals=3) {
    auto* sb = new QDoubleSpinBox();
    sb->setRange(minV, maxV);
    sb->setDecimals(decimals);
    sb->setSingleStep(step);
    sb->setValue(val);
    return sb;
}
static QSpinBox* makeSsb(int minV, int maxV, int step, int val) {
    auto* sb = new QSpinBox();
    sb->setRange(minV, maxV);
    sb->setSingleStep(step);
    sb->setValue(val);
    return sb;
}

static QImage applyFixedPadding(const QImage& img, double padFrac, const QColor& fill) {
    const int W = img.width();
    const int H = img.height();
    const int padX = (int)std::round(W * padFrac);
    const int padY = (int)std::round(H * padFrac);
    const int CW = W + 2 * padX;
    const int CH = H + 2 * padY;

    QImage canvas(CW, CH, QImage::Format_RGB32);
    canvas.fill(fill);

    QPainter p(&canvas);
    p.drawImage(padX, padY, img);
    return canvas;
}

MainWindow::TerrainMode MainWindow::terrainMode() const {
    return static_cast<TerrainMode>(cbTerrainMode_->currentIndex());
}

const ITerrain* MainWindow::currentTerrain() const {
    const auto mode = terrainMode();
    if (mode == TerrainMode::Dem) {
        if (hasDem_ && dem_.isValid()) return &dem_;
        return nullptr;
    }
    if (mode == TerrainMode::Procedural) return &proc_;
    return &flat_;
}

QString MainWindow::framesDir() const {
    return QDir::current().filePath("outputs/frames");
}

PlumeEngine::ModelType MainWindow::selectedModelType() const
{
    if (!cbModel_)
        return PlumeEngine::ModelType::CfdEulerian;

    const QVariant v = cbModel_->currentData();
    if (!v.isValid())
        return PlumeEngine::ModelType::CfdEulerian;

    const int code = v.toInt();
    if (code < 0 || code > (int)PlumeEngine::ModelType::LagrangianPuff)
        return PlumeEngine::ModelType::CfdEulerian;

    return static_cast<PlumeEngine::ModelType>(code);
}

QString MainWindow::selectedModelTag() const
{
    switch (selectedModelType())
    {
    case PlumeEngine::ModelType::CfdEulerian:   return "cfd";
    case PlumeEngine::ModelType::GaussianPlume: return "gauss";
    case PlumeEngine::ModelType::LagrangianPuff:return "puff";
    default:                                   return "cfd";
    }
}

void MainWindow::appendLog(const QString& s) {
    log_->appendPlainText(s);
}

void MainWindow::updateDomainInfo() {
    const double Lx = sbLx_->value();
    const double Ly = sbLy_->value();
    const double dx = std::max(1e-9, sbDx_->value());
    const double dy = std::max(1e-9, sbDy_->value());
    const int Nx = std::max(2, (int)std::floor(Lx/dx) + 1);
    const int Ny = std::max(2, (int)std::floor(Ly/dy) + 1);
    domainInfo_->setText(QString("Nx=%1 Ny=%2 (non-DEM)").arg(Nx).arg(Ny));
}

void MainWindow::enforceSourceCenterIfNeeded() {
    if (!cbAutoCenterSrc_ || !cbAutoCenterSrc_->isChecked()) return;
    if (terrainMode() == TerrainMode::Dem) return;
    if (!terrainPreviewReady_) return;

    const double xMin = tx0_;
    const double yMin = ty0_;
    const double xMax = tx0_ + (tNx_ - 1) * tdx_;
    const double yMax = ty0_ + (tNy_ - 1) * tdy_;
    const double cx = 0.5 * (xMin + xMax);
    const double cy = 0.5 * (yMin + yMax);

    const double x = sbSrcX_->value();
    const double y = sbSrcY_->value();

    const double eps = 1e-9;

    const bool out =
        (x < xMin - eps) || (x > xMax + eps) ||
        (y < yMin - eps) || (y > yMax + eps);

    if (out) {
        sbSrcX_->setValue(cx);
        sbSrcY_->setValue(cy);
        appendLog(QString("[SRC] auto-centered to domain center: (%1, %2)")
                  .arg(cx, 0, 'f', 2).arg(cy, 0, 'f', 2));
    }
}

int MainWindow::zToK(double z) const {
    if (!simReady_) return 0;
    const auto& g = engine_.grid();
    if (g.Nz <= 1) return 0;
    const double z0 = g.z0;
    const double dz = g.dz;
    const int k = (int)std::round((z - z0) / dz);
    return std::clamp(k, 0, g.Nz - 1);
}

double MainWindow::groundAtSource() const {
    const ITerrain* terr = currentTerrain();
    if (!terr) return 0.0;
    return terr->height(sbSrcX_->value(), sbSrcY_->value());
}

double MainWindow::sliceZ() const {
    if (cbFollowSlice_->isChecked())
        return sbSrcZ_->value();
    return sbZSlice_->value();
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("GasDispersionDesktop (CFD + Gaussian + Puff)");
    resize(1400, 800);

    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* root = new QVBoxLayout(central);

    auto* splitter = new QSplitter(Qt::Horizontal, central);
    root->addWidget(splitter, 1);

    leftScroll_ = new QScrollArea(splitter);
    leftScroll_->setWidgetResizable(true);

    auto* leftInner = new QWidget();
    leftScroll_->setWidget(leftInner);

    auto* leftLayout = new QVBoxLayout(leftInner);
    leftLayout->setSpacing(8);

    auto* right = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(6, 6, 6, 6);

    view_ = new QLabel();
    view_->setMinimumSize(640, 520);
    view_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    view_->setAlignment(Qt::AlignCenter);
    view_->setText("Click Preview Terrain.");

    status_ = new QLabel();
    status_->setText("Ready.");

    rightLayout->addWidget(view_, 1);
    rightLayout->addWidget(status_, 0);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    leftScroll_->setMinimumWidth(360);
    leftInner->setMinimumWidth(340);
    splitter->setCollapsible(0, false);
    splitter->setSizes(QList<int>{400, 1000});
    splitter->setHandleWidth(8);

    auto* gbTerrain = new QGroupBox("Terrain", leftInner);
    auto* terrainForm = new QFormLayout(gbTerrain);

    cbTerrainMode_ = new QComboBox(gbTerrain);
    cbTerrainMode_->addItem("Flat");
    cbTerrainMode_->addItem("DEM");
    cbTerrainMode_->addItem("Procedural");
    cbTerrainMode_->setCurrentIndex(0);
    terrainForm->addRow("Mode", cbTerrainMode_);

    leDemTif_ = new QLineEdit(gbTerrain);
    btnPickDem_ = new QPushButton("Pick DEM GeoTIFF", gbTerrain);
    btnConvertLoadDem_ = new QPushButton("Convert+Load DEM", gbTerrain);
    terrainForm->addRow("DEM tif", leDemTif_);
    terrainForm->addRow(btnPickDem_);
    terrainForm->addRow(btnConvertLoadDem_);

    sbFlatZ_ = makeDsb(-1000, 10000, 1.0, 0.0, 2);
    terrainForm->addRow("Flat Z", sbFlatZ_);

    sbProcBaseZ_ = makeDsb(-1000, 10000, 1.0, 0.0, 2);
    sbProcPeakA_ = makeDsb(0, 10000, 1.0, 80.0, 2);
    sbProcSigma_ = makeDsb(1, 10000, 1.0, 50.0, 2);
    terrainForm->addRow("Proc base Z", sbProcBaseZ_);
    terrainForm->addRow("Proc peak A", sbProcPeakA_);
    terrainForm->addRow("Proc sigma", sbProcSigma_);

    btnPreviewTerrain_ = new QPushButton("Preview Terrain", gbTerrain);
    terrainForm->addRow(btnPreviewTerrain_);

    leftLayout->addWidget(gbTerrain);

    auto* gbDomain = new QGroupBox("Domain (Cartesian)", leftInner);
    auto* domForm = new QFormLayout(gbDomain);

    sbX0_ = makeDsb(-1e6, 1e6, 1.0, 0.0, 2);
    sbY0_ = makeDsb(-1e6, 1e6, 1.0, 0.0, 2);
    sbLx_ = makeDsb(1.0, 1e6, 1.0, 200.0, 2);
    sbLy_ = makeDsb(1.0, 1e6, 1.0, 200.0, 2);
    sbDx_ = makeDsb(0.1, 1000.0, 0.1, 1.0, 2);
    sbDy_ = makeDsb(0.1, 1000.0, 0.1, 1.0, 2);
    domainInfo_ = new QLabel("Nx=? Ny=?", gbDomain);

    domForm->addRow("x0", sbX0_);
    domForm->addRow("y0", sbY0_);
    domForm->addRow("Lx", sbLx_);
    domForm->addRow("Ly", sbLy_);
    domForm->addRow("dx", sbDx_);
    domForm->addRow("dy", sbDy_);
    domForm->addRow(domainInfo_);

    leftLayout->addWidget(gbDomain);

    auto* gbZ = new QGroupBox("Vertical", leftInner);
    auto* zForm = new QFormLayout(gbZ);

    sbDz_ = makeDsb(0.1, 1000.0, 0.1, 1.0, 2);
    sbZTopMargin_ = makeDsb(0.0, 20000.0, 1.0, 50.0, 2);
    sbNzMax_ = makeSsb(2, 2000, 1, 120);

    sbZSlice_ = makeDsb(-1000.0, 20000.0, 0.5, 2.0, 2);
    cbFollowSlice_ = new QCheckBox("Follow slice Z = srcZ", gbZ);
    cbFollowSlice_->setChecked(true);

    zForm->addRow("dz", sbDz_);
    zForm->addRow("Z top margin", sbZTopMargin_);
    zForm->addRow("Nz max", sbNzMax_);
    zForm->addRow("Z slice", sbZSlice_);
    zForm->addRow(cbFollowSlice_);

    leftLayout->addWidget(gbZ);

    auto* gbPhys = new QGroupBox("Physics", leftInner);
    auto* physForm = new QFormLayout(gbPhys);

    cbModel_ = new QComboBox(gbPhys);
    cbModel_->addItem("CFD (Eulerian)", (int)PlumeEngine::ModelType::CfdEulerian);
    cbModel_->addItem("Gaussian plume (ALOHA-like)", (int)PlumeEngine::ModelType::GaussianPlume);
    cbModel_->addItem("Gaussian puff (CALPUFF-like)", (int)PlumeEngine::ModelType::LagrangianPuff);
    cbModel_->setCurrentIndex(0);

    physForm->addRow("Model", cbModel_);

    sbWindSpeed_ = makeDsb(0.0, 100.0, 0.1, 2.0, 2);
    sbWindDir_   = makeDsb(0.0, 360.0, 1.0, 0.0, 1);
    sbK_         = makeDsb(0.0, 1000.0, 0.01, 2.0, 4);
    sbDecay_     = makeDsb(0.0, 10.0, 0.001, 0.0, 6);

    physForm->addRow("Wind speed (m/s)", sbWindSpeed_);
    physForm->addRow("Wind dir (deg)", sbWindDir_);
    physForm->addRow("K (m^2/s)", sbK_);
    physForm->addRow("Decay (1/s)", sbDecay_);

    leftLayout->addWidget(gbPhys);

    auto* gbTime = new QGroupBox("Time & Output", leftInner);
    auto* timeForm = new QFormLayout(gbTime);

    sbTotalTime_ = makeDsb(1.0, 36000.0, 10.0, 60.0, 1);
    sbDt_        = makeDsb(1e-4, 10.0, 0.01, 0.05, 4);
    cbAutoClampDt_ = new QCheckBox("Auto clamp dt (stable)", gbTime);
    cbAutoClampDt_->setChecked(true);
    cbExportCsv_ = new QCheckBox("Export CSV frames", gbTime);
    cbExportCsv_->setChecked(true);
    sbExportInterval_ = makeDsb(0.01, 10.0, 0.05, 0.20, 2);

    cbExportTwoSlices_ = new QCheckBox("Export TWO slices per frame (zSlice + AGL)", gbTime);
    cbExportTwoSlices_->setChecked(true);

    timeForm->addRow("Total (s)", sbTotalTime_);
    timeForm->addRow("dt (s)", sbDt_);
    timeForm->addRow(cbAutoClampDt_);
    timeForm->addRow(cbExportCsv_);
    timeForm->addRow("Export interval (s)", sbExportInterval_);
    timeForm->addRow(cbExportTwoSlices_);

    leftLayout->addWidget(gbTime);

    auto* gbViz = new QGroupBox("Visualization", leftInner);
    auto* vizForm = new QFormLayout(gbViz);

    cbBgMode_ = new QComboBox(gbViz);
    cbBgMode_->addItem("Terrain gray");
    cbBgMode_->addItem("Solid blue");
    vizForm->addRow("Background", cbBgMode_);

    sbRelCut_ = makeDsb(0.0, 1.0, 0.01, 0.05, 3);
    vizForm->addRow("RelCut", sbRelCut_);

    leftLayout->addWidget(gbViz);

    auto* gbSrc = new QGroupBox("3D Source", leftInner);
    auto* srcForm = new QFormLayout(gbSrc);

    sbSrcX_ = makeDsb(-1e6, 1e6, 1.0, 100.0, 2);
    sbSrcY_ = makeDsb(-1e6, 1e6, 1.0, 100.0, 2);
    sbSrcZ_ = makeDsb(-1000.0, 20000.0, 0.5, 2.0, 2);
    sbSrcR_ = makeDsb(0.1, 10000.0, 0.1, 2.0, 2);
    sbLeakRate_ = makeDsb(0.0, 1e9, 0.1, 1.0, 3);

    btnSetSrcZFromGround_ = new QPushButton("Set srcZ=ground+1m", gbSrc);

    cbAutoCenterSrc_ = new QCheckBox("Auto center source if out-of-domain (non-DEM)", gbSrc);
    cbAutoCenterSrc_->setChecked(true);

    srcForm->addRow("Src X", sbSrcX_);
    srcForm->addRow("Src Y", sbSrcY_);
    srcForm->addRow("Src Z", sbSrcZ_);
    srcForm->addRow("Src R", sbSrcR_);
    srcForm->addRow("Leak rate", sbLeakRate_);
    srcForm->addRow(btnSetSrcZFromGround_);
    srcForm->addRow(cbAutoCenterSrc_);

    leftLayout->addWidget(gbSrc);

    auto* gbCtrl = new QGroupBox("Control", leftInner);
    auto* ctrlLayout = new QVBoxLayout(gbCtrl);

    auto* btnRow = new QWidget(gbCtrl);
    auto* btnRowLayout = new QHBoxLayout(btnRow);
    btnRowLayout->setContentsMargins(0, 0, 0, 0);

    btnRun_ = new QPushButton("Run", gbCtrl);
    btnPause_ = new QPushButton("Pause", gbCtrl);
    btnReset_ = new QPushButton("Reset", gbCtrl);

    btnRowLayout->addWidget(btnRun_);
    btnRowLayout->addWidget(btnPause_);
    btnRowLayout->addWidget(btnReset_);

    ctrlLayout->addWidget(btnRow);

    log_ = new QPlainTextEdit(gbCtrl);
    log_->setReadOnly(true);
    log_->setMinimumHeight(160);
    ctrlLayout->addWidget(log_);

    leftLayout->addWidget(gbCtrl);
    leftLayout->addStretch(1);

    flat_.setZ0(sbFlatZ_->value());

    TerrainProcedural::Gaussian g;
    g.A = sbProcPeakA_->value();
    g.sigma = sbProcSigma_->value();
    proc_.setGaussian(g);

    connect(btnPickDem_, &QPushButton::clicked, this, &MainWindow::onPickDemClicked);
    connect(btnConvertLoadDem_, &QPushButton::clicked, this, &MainWindow::onConvertLoadDemClicked);
    connect(btnPreviewTerrain_, &QPushButton::clicked, this, &MainWindow::onPreviewTerrainClicked);

    connect(cbTerrainMode_, &QComboBox::currentIndexChanged, this, &MainWindow::onTerrainParamsChanged);

    connect(cbModel_, &QComboBox::currentIndexChanged, this, &MainWindow::onModelChanged);

    connect(sbFlatZ_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::onTerrainParamsChanged);
    connect(sbProcBaseZ_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::onTerrainParamsChanged);
    connect(sbProcPeakA_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::onTerrainParamsChanged);
    connect(sbProcSigma_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::onTerrainParamsChanged);

    connect(sbX0_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::onTerrainParamsChanged);
    connect(sbY0_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::onTerrainParamsChanged);
    connect(sbLx_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::onTerrainParamsChanged);
    connect(sbLy_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::onTerrainParamsChanged);
    connect(sbDx_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::onTerrainParamsChanged);
    connect(sbDy_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::onTerrainParamsChanged);

    connect(btnSetSrcZFromGround_, &QPushButton::clicked, this, &MainWindow::onSetSrcZFromGroundClicked);

    connect(btnRun_, &QPushButton::clicked, this, &MainWindow::onRunClicked);
    connect(btnPause_, &QPushButton::clicked, this, &MainWindow::onPauseClicked);
    connect(btnReset_, &QPushButton::clicked, this, &MainWindow::onResetClicked);

    connect(cbFollowSlice_, &QCheckBox::toggled, this, &MainWindow::onFollowSliceToggled);

    timer_ = new QTimer(this);
    timer_->setInterval(15);
    connect(timer_, &QTimer::timeout, this, &MainWindow::onTick);

    updateDomainInfo();
    appendLog("[Init] ready. Click Preview Terrain first.");
}

void MainWindow::onPickDemClicked() {
    const QString f = QFileDialog::getOpenFileName(this, "Pick DEM GeoTIFF", QDir::currentPath(), "GeoTIFF (*.tif *.tiff)");
    if (!f.isEmpty()) leDemTif_->setText(f);
}

void MainWindow::onConvertLoadDemClicked() {
    const QString tifPath = leDemTif_->text().trimmed();
    if (tifPath.isEmpty()) { appendLog("[DEM] path empty."); return; }

    const QString outDir = QDir::current().filePath("outputs/dem_cache");
    QDir().mkpath(outDir);

    const QString cmd = QString("python tools/dem_convert.py --in \"%1\" --out_dir \"%2\"").arg(tifPath).arg(outDir);
    appendLog("[DEM] convert: " + cmd);

    const int code = std::system(cmd.toLocal8Bit().constData());
    if (code != 0) {
        appendLog("[DEM] convert failed. Ensure rasterio or gdal is installed (conda-forge).");
        return;
    }

    const QString metaPath = QDir(outDir).filePath("dem_meta.json");
    const QString binPath  = QDir(outDir).filePath("dem_data.bin");

    QString err;
    if (!dem_.load(metaPath, binPath, err)) {
        appendLog("[DEM] load failed: " + err);
        return;
    }

    hasDem_ = true;
    appendLog("[DEM] loaded OK.");
}

void MainWindow::onTerrainParamsChanged() {
    flat_.setZ0(sbFlatZ_->value());

    TerrainProcedural::Gaussian g;
    g.A = sbProcPeakA_->value();
    g.sigma = sbProcSigma_->value();
    proc_.setGaussian(g);

    updateDomainInfo();
    terrainPreviewReady_ = false;
    simReady_ = false;
    running_ = false;
    timer_->stop();
    appendLog("[Params] changed. Need Preview Terrain.");
}

void MainWindow::onPreviewTerrainClicked() {
    QString err;
    if (!buildTerrainPreview(err)) {
        appendLog("[Preview] failed: " + err);
        return;
    }

    enforceSourceCenterIfNeeded();
    renderTerrainOnly();
    appendLog("[Preview] OK.");
}

void MainWindow::onFollowSliceToggled(bool) {
    if (!simReady_) {
        renderTerrainOnly();
        return;
    }
    renderTerrainAndSlice();
}

void MainWindow::onSetSrcZFromGroundClicked() {
    const ITerrain* terr = currentTerrain();
    if (!terr) { appendLog("[Src] no terrain."); return; }
    const double gx = sbSrcX_->value();
    const double gy = sbSrcY_->value();
    const double gz = terr->height(gx, gy) + 1.0;
    sbSrcZ_->setValue(gz);
    appendLog("[Src] srcZ set to ground+1m.");
}

bool MainWindow::buildTerrainPreview(QString& err) {
    const ITerrain* terr = currentTerrain();
    if (!terr || !terr->isValid()) { err = "terrain invalid"; return false; }

    if (terrainMode() == TerrainMode::Dem) {
        const int stride = 4;
        tdx_ = std::abs(dem_.meta().dx) * stride;
        tdy_ = std::abs(dem_.meta().dy) * stride;
        tNx_ = std::max(2, dem_.meta().width / stride);
        tNy_ = std::max(2, dem_.meta().height / stride);

        tx0_ = dem_.minX();
        ty0_ = dem_.maxY();
    } else {
        const double x0 = sbX0_->value();
        const double y0 = sbY0_->value();
        const double Lx = sbLx_->value();
        const double Ly = sbLy_->value();
        const double dx = std::max(1e-9, sbDx_->value());
        const double dy = std::max(1e-9, sbDy_->value());

        tNx_ = std::max(2, (int)std::floor(Lx / dx) + 1);
        tNy_ = std::max(2, (int)std::floor(Ly / dy) + 1);

        tdx_ = dx;
        tdy_ = dy;
        tx0_ = x0;
        ty0_ = y0;
    }

    terrainZ_.assign(tNx_ * tNy_, 0.0f);

    tMin_ = std::numeric_limits<double>::infinity();
    tMax_ = -std::numeric_limits<double>::infinity();

    for (int j = 0; j < tNy_; ++j) {
        const double y = ty0_ + j * tdy_;
        for (int i = 0; i < tNx_; ++i) {
            const double x = tx0_ + i * tdx_;
            const double z = terr->height(x, y);
            terrainZ_[j * tNx_ + i] = (float)z;
            tMin_ = std::min(tMin_, z);
            tMax_ = std::max(tMax_, z);
        }
    }

    if (!std::isfinite(tMin_) || !std::isfinite(tMax_)) {
        err = "terrain z invalid";
        return false;
    }

    terrainPreviewReady_ = true;
    return true;
}

bool MainWindow::buildSimulation(QString& err) {
    if (!terrainPreviewReady_) { err = "preview terrain first"; return false; }

    const ITerrain* terr = currentTerrain();
    if (!terr || !terr->isValid()) { err = "terrain invalid"; return false; }

    Grid3D g;
    g.Nx = tNx_;
    g.Ny = tNy_;
    g.x0 = tx0_;
    g.y0 = ty0_;
    g.dx = tdx_;
    g.dy = tdy_;

    const double dz = std::max(1e-6, sbDz_->value());
    const double zTopMargin = sbZTopMargin_->value();
    const int NzMax = sbNzMax_->value();

    g.z0 = tMin_;
    const double zTop = tMax_ + zTopMargin;
    const int Nz = std::max(2, (int)std::ceil((zTop - g.z0) / dz) + 1);
    g.dz = dz;
    g.Nz = std::min(Nz, NzMax);

    PlumeEngine::Params p;
    p.windSpeed_mps = sbWindSpeed_->value();
    p.windDir_deg = sbWindDir_->value();
    p.K_m2ps = sbK_->value();
    p.decay_1ps = sbDecay_->value();
    p.srcX_m = sbSrcX_->value();
    p.srcY_m = sbSrcY_->value();
    p.srcZ_m = sbSrcZ_->value();
    p.srcRadius_m = sbSrcR_->value();
    p.leakRate_kgps = sbLeakRate_->value();
    p.totalTime_s = sbTotalTime_->value();
    p.dt_s = sbDt_->value();
    p.autoClampDt = cbAutoClampDt_->isChecked();

    engine_.setModel(selectedModelType());

    QString e;
    if (!engine_.initialize(g, terr, p, e)) {
        err = e;
        return false;
    }

    simReady_ = true;
    running_ = false;
    timer_->stop();
    nextExportT_ = 0.0;
    exportFrameIdx_ = 0;

    appendLog(QString("[Sim] built: %1x%2x%3 model=%4").arg(g.Nx).arg(g.Ny).arg(g.Nz).arg(selectedModelTag()));
    return true;
}

void MainWindow::renderTerrainOnly() {
    if (!terrainPreviewReady_) return;

    const int W = 800;
    const int H = 600;

    QImage img(W, H, QImage::Format_RGB32);
    img.fill(Qt::black);

    const double xMin = tx0_;
    const double yMin = ty0_;
    const double xMax = tx0_ + (tNx_ - 1) * tdx_;
    const double yMax = ty0_ + (tNy_ - 1) * tdy_;

    const double padFrac = 0.35;
    const double dx = xMax - xMin;
    const double dy = yMax - yMin;

    const double xMinP = xMin - padFrac * dx;
    const double xMaxP = xMax + padFrac * dx;
    const double yMinP = yMin - padFrac * dy;
    const double yMaxP = yMax + padFrac * dy;

    const double sx = (W - 1) / (xMaxP - xMinP);
    const double sy = (H - 1) / (yMaxP - yMinP);

    for (int py = 0; py < H; ++py) {
        const double y = yMaxP - py / sy;
        for (int px = 0; px < W; ++px) {
            const double x = xMinP + px / sx;

            const ITerrain* terr = currentTerrain();
            double z = 0.0;
            if (terr) z = terr->height(x, y);

            const double zn = (z - tMin_) / std::max(1e-9, (tMax_ - tMin_));
            const int base = (int)std::round(zn * 200.0 + 30.0);

            double zx = 0.0, zy = 0.0;
            if (terr) {
                const double eps = 0.5 * std::min(tdx_, tdy_);
                zx = terr->height(x + eps, y) - terr->height(x - eps, y);
                zy = terr->height(x, y + eps) - terr->height(x, y - eps);
            }

            double nx = -zx;
            double ny = -zy;
            double nz = 2.0;
            const double nlen = std::sqrt(nx * nx + ny * ny + nz * nz);
            nx /= nlen; ny /= nlen; nz /= nlen;

            const double lx = 0.6, ly = 0.4, lz = 0.7;
            const double llen = std::sqrt(lx * lx + ly * ly + lz * lz);
            const double lxn = lx / llen, lyn = ly / llen, lzn = lz / llen;

            const double diff = std::clamp(nx * lxn + ny * lyn + nz * lzn, 0.0, 1.0);
            int v = (int)std::round(base * (0.6 + 0.6 * diff));
            v = std::clamp(v, 0, 255);

            img.setPixel(px, py, qRgb(v, v, v));
        }
    }

    {
        QPainter p(&img);
        p.setPen(QPen(Qt::red, 2));
        const double sxw = (sbSrcX_->value() - xMinP) * sx;
        const double syw = (yMaxP - sbSrcY_->value()) * sy;
        p.drawEllipse(QPointF(sxw, syw), 6, 6);
        p.drawLine(QPointF(sxw - 10, syw), QPointF(sxw + 10, syw));
        p.drawLine(QPointF(sxw, syw - 10), QPointF(sxw, syw + 10));
    }

    const QImage padded = applyFixedPadding(img, 0.0, Qt::black);
    view_->setPixmap(QPixmap::fromImage(padded));
    status_->setText(QString("Terrain preview. tMin=%1 tMax=%2").arg(tMin_).arg(tMax_));
}

void MainWindow::renderTerrainAndSlice() {
    if (!simReady_) return;

    const double zEff = sliceZ();
    const int k = zToK(zEff);

    std::vector<float> slice;
    float smax = 0.0f;
    engine_.extractSliceXY(k, slice, smax);

    const auto& g = engine_.grid();

    const int W = 800;
    const int H = 600;

    QImage img(W, H, QImage::Format_RGB32);
    img.fill(Qt::black);

    const double xMin = g.x0;
    const double yMin = g.y0;
    const double xMax = g.x0 + (g.Nx - 1) * g.dx;
    const double yMax = g.y0 + (g.Ny - 1) * g.dy;

    const double padFrac = 0.35;
    const double dx = xMax - xMin;
    const double dy = yMax - yMin;

    const double xMinP = xMin - padFrac * dx;
    const double xMaxP = xMax + padFrac * dx;
    const double yMinP = yMin - padFrac * dy;
    const double yMaxP = yMax + padFrac * dy;

    const double sx = (W - 1) / (xMaxP - xMinP);
    const double sy = (H - 1) / (yMaxP - yMinP);

    const int bgMode = cbBgMode_->currentIndex();
    const double relCut = sbRelCut_->value();

    for (int py = 0; py < H; ++py) {
        const double y = yMaxP - py / sy;
        for (int px = 0; px < W; ++px) {
            const double x = xMinP + px / sx;

            int i = (int)std::round((x - g.x0) / g.dx);
            int j = (int)std::round((y - g.y0) / g.dy);

            if (i < 0 || i >= g.Nx || j < 0 || j >= g.Ny) {
                img.setPixel(px, py, qRgb(0, 0, 0));
                continue;
            }

            QColor bg;
            if (bgMode == 0) {
                const ITerrain* terr = currentTerrain();
                double zt = terr ? terr->height(x, y) : 0.0;
                const double zn = (zt - tMin_) / std::max(1e-9, (tMax_ - tMin_));
                int v = (int)std::round(zn * 200.0 + 30.0);
                v = std::clamp(v, 0, 255);
                bg = QColor(v, v, v);
            } else {
                bg = QColor(0, 30, 120);
            }

            const float c = slice[j * g.Nx + i];
            const float maxC = std::max(1e-12f, smax);
            const float cCut = (float)(relCut * maxC);

            QColor out = bg;
            if (c > cCut) {
                float cn = (c - cCut) / std::max(1e-12f, (maxC - cCut));
                cn = std::clamp(cn, 0.0f, 1.0f);

                const QColor cc = colorMap(cn);

                const float a = 0.15f + 0.75f * std::sqrt(cn);

                const int r = (int)std::round((1 - a) * bg.red() + a * cc.red());
                const int g0 = (int)std::round((1 - a) * bg.green() + a * cc.green());
                const int b = (int)std::round((1 - a) * bg.blue() + a * cc.blue());
                out = QColor(r, g0, b);
            }

            img.setPixelColor(px, py, out);
        }
    }

    {
        QPainter p(&img);
        p.setPen(QPen(Qt::red, 2));
        const double sxw = (sbSrcX_->value() - xMinP) * sx;
        const double syw = (yMaxP - sbSrcY_->value()) * sy;
        p.drawEllipse(QPointF(sxw, syw), 6, 6);
        p.drawLine(QPointF(sxw - 10, syw), QPointF(sxw + 10, syw));
        p.drawLine(QPointF(sxw, syw - 10), QPointF(sxw, syw + 10));
    }

    view_->setPixmap(QPixmap::fromImage(img));
    status_->setText(QString("t=%1s z=%2 (k=%3) max=%4 model=%5")
                         .arg(engine_.time(), 0, 'f', 2)
                         .arg(zEff, 0, 'f', 2)
                         .arg(k)
                         .arg(smax, 0, 'g', 3)
                         .arg(selectedModelTag()));
}

void MainWindow::onRunClicked() {
    if (!simReady_) {
        QString err;
        if (!buildSimulation(err)) {
            appendLog("[Run] buildSimulation failed: " + err);
            return;
        }
    }
    running_ = true;
    timer_->start();
    appendLog("[Run] start.");
}

void MainWindow::onPauseClicked() {
    running_ = false;
    timer_->stop();
    appendLog("[Pause] stopped.");
}

void MainWindow::onResetClicked() {
    running_ = false;
    timer_->stop();
    if (simReady_) engine_.reset();
    simReady_ = false;
    appendLog("[Reset] done.");
    renderTerrainOnly();
}

void MainWindow::onModelChanged(int) {
    onResetClicked();
    appendLog(QString("[Model] switched to %1.").arg(selectedModelTag()));
}

void MainWindow::onTick() {
    if (!running_ || !simReady_) return;

    engine_.step();

    const auto p = engine_.params();
    if (engine_.time() >= p.totalTime_s - 1e-12) {
        running_ = false;
        timer_->stop();
        appendLog("[Run] finished.");
        return;
    }

    if (cbExportCsv_->isChecked()) {
        const double t = engine_.time();
        if (t >= nextExportT_ - 1e-12) {
            QDir().mkpath(framesDir());

            const auto& g = engine_.grid();

            auto writeSlice = [&](const QString &suffix, double zWorld) {
                int k = zToK(zWorld);

                std::vector<float> slice;
                float smax = 0.0f;
                engine_.extractSliceXY(k, slice, smax);

                ExporterCsv::FrameMeta meta;
                meta.epsg = 0;
                meta.origin_x = g.x0;
                meta.origin_y = g.y0;
                meta.dx = g.dx;
                meta.dy = g.dy;
                meta.z = zWorld;
                meta.Nx = g.Nx;
                meta.Ny = g.Ny;
                meta.t = t;

                const int frameIdx = exportFrameIdx_++;
                const QString fn = QString("frame_%1_%2_%3.csv")
                                       .arg(frameIdx, 4, 10, QChar('0'))
                                       .arg(selectedModelTag())
                                       .arg(suffix);

                QString err;
                const QString outPath = QDir(framesDir()).filePath(fn);
                if (!ExporterCsv::writeGridFrame(outPath, meta, slice, err)) {
                    appendLog("[Export] failed: " + err);
                } else {
                    appendLog("[Export] " + outPath);
                }
            };

            const double zSlice = sliceZ();
            writeSlice("zslice", zSlice);

            if (cbExportTwoSlices_->isChecked()) {
                const double agl = sbZSlice_->value();
                const double zAGL = groundAtSource() + agl;
                writeSlice("agl", zAGL);
            }

            nextExportT_ += sbExportInterval_->value();
        }
    }

    renderTerrainAndSlice();
}
