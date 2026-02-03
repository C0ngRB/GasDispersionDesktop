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
#include <algorithm>
#include <cmath>

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

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("GasDispersionDesktop - Scrollable UI + Terrain Preview + Overlay");
    resize(1400, 800);

    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* splitter = new QSplitter(Qt::Horizontal, central);

    auto* leftInner = new QWidget();
    auto* leftLayout = new QVBoxLayout(leftInner);

    leftScroll_ = new QScrollArea(splitter);
    leftScroll_->setWidgetResizable(true);
    leftScroll_->setWidget(leftInner);

    auto* gbTerrain = new QGroupBox("Terrain Mode", leftInner);
    auto* terrainForm = new QFormLayout(gbTerrain);

    cbTerrainMode_ = new QComboBox(gbTerrain);
    cbTerrainMode_->addItem("Flat (default)");
    cbTerrainMode_->addItem("DEM (GeoTIFF)");
    cbTerrainMode_->addItem("Procedural (Gaussian Hill)");

    terrainForm->addRow("Mode", cbTerrainMode_);

    sbFlatZ_ = makeDsb(-10000.0, 10000.0, 1.0, 0.0, 2);
    terrainForm->addRow("Flat z0 (m)", sbFlatZ_);

    sbProcBaseZ_ = makeDsb(-10000.0, 10000.0, 1.0, 0.0, 2);
    sbProcPeakA_ = makeDsb(0.0, 5000.0, 1.0, 50.0, 2);
    sbProcSigma_ = makeDsb(1.0, 20000.0, 10.0, 200.0, 2);
    terrainForm->addRow("Proc baseZ (m)", sbProcBaseZ_);
    terrainForm->addRow("Proc peak A (m)", sbProcPeakA_);
    terrainForm->addRow("Proc sigma (m)", sbProcSigma_);

    btnPreviewTerrain_ = new QPushButton("Preview Terrain", gbTerrain);
    terrainForm->addRow(btnPreviewTerrain_);

    auto* gbDem = new QGroupBox("DEM (GeoTIFF -> meta+bin)", leftInner);
    auto* demForm = new QFormLayout(gbDem);

    leDemTif_ = new QLineEdit(gbDem);
    leDemTif_->setPlaceholderText("Select GeoTIFF DEM (.tif, projected meters)");

    btnPickDem_ = new QPushButton("Pick...", gbDem);
    btnConvertLoadDem_ = new QPushButton("Convert+Load", gbDem);

    auto* demRow = new QWidget(gbDem);
    auto* demRowLayout = new QHBoxLayout(demRow);
    demRowLayout->addWidget(leDemTif_, 1);
    demRowLayout->addWidget(btnPickDem_);
    demRowLayout->addWidget(btnConvertLoadDem_);
    demForm->addRow("GeoTIFF", demRow);

    sbDemStride_ = makeSsb(1, 16, 1, 2);
    demForm->addRow("Stride (DEM downsample)", sbDemStride_);

    demInfo_ = new QLabel("DEM: not loaded", gbDem);
    demInfo_->setWordWrap(true);
    demForm->addRow("Info", demInfo_);

    connect(btnPickDem_, &QPushButton::clicked, this, &MainWindow::onPickDemClicked);
    connect(btnConvertLoadDem_, &QPushButton::clicked, this, &MainWindow::onConvertLoadDemClicked);

    auto* gbDomain = new QGroupBox("XY Domain (used when NOT DEM)", leftInner);
    auto* domainForm = new QFormLayout(gbDomain);

    sbX0_ = makeDsb(-1e12, 1e12, 10.0, 0.0, 2);
    sbY0_ = makeDsb(-1e12, 1e12, 10.0, 0.0, 2);
    sbLx_ = makeDsb(10.0, 1e7, 10.0, 200.0, 1);
    sbLy_ = makeDsb(10.0, 1e7, 10.0, 100.0, 1);
    sbDx_ = makeDsb(0.1, 1000.0, 0.1, 2.0, 2);
    sbDy_ = makeDsb(0.1, 1000.0, 0.1, 2.0, 2);

    domainInfo_ = new QLabel("", gbDomain);
    domainInfo_->setWordWrap(true);

    domainForm->addRow("x0 (m)", sbX0_);
    domainForm->addRow("y0 (m)", sbY0_);
    domainForm->addRow("Lx (m)", sbLx_);
    domainForm->addRow("Ly (m)", sbLy_);
    domainForm->addRow("dx (m)", sbDx_);
    domainForm->addRow("dy (m)", sbDy_);
    domainForm->addRow("Derived", domainInfo_);

    updateDomainInfo();

    auto* gbZ = new QGroupBox("Vertical Domain", leftInner);
    auto* zForm = new QFormLayout(gbZ);

    sbDz_ = makeDsb(0.1, 100.0, 0.1, 2.0, 2);
    sbZTopMargin_ = makeDsb(1.0, 5000.0, 1.0, 200.0, 1);
    sbNzMax_ = makeSsb(10, 2000, 10, 300);
    sbZSlice_ = makeDsb(-1e12, 1e12, 1.0, 0.0, 2);

    zForm->addRow("dz (m)", sbDz_);
    zForm->addRow("zTop margin (m)", sbZTopMargin_);
    zForm->addRow("Nz max", sbNzMax_);
    zForm->addRow("zSlice (m)", sbZSlice_);

    auto* gbPhys = new QGroupBox("Wind & Physics", leftInner);
    auto* physForm = new QFormLayout(gbPhys);

    sbWindSpeed_ = makeDsb(0.0, 50.0, 0.1, 2.0, 3);
    sbWindDir_   = makeDsb(-180.0, 180.0, 1.0, 0.0, 1);
    sbK_         = makeDsb(0.0, 1e5, 0.1, 1.0, 4);
    sbDecay_     = makeDsb(0.0, 10.0, 0.001, 0.0, 4);

    physForm->addRow("Wind speed (m/s)", sbWindSpeed_);
    physForm->addRow("Wind dir (deg)", sbWindDir_);
    physForm->addRow("K (m^2/s)", sbK_);
    physForm->addRow("decay k (1/s)", sbDecay_);

    auto* gbTime = new QGroupBox("Time & Output", leftInner);
    auto* timeForm = new QFormLayout(gbTime);

    sbTotalTime_ = makeDsb(1.0, 36000.0, 10.0, 60.0, 1);
    sbDt_        = makeDsb(1e-4, 10.0, 0.01, 0.05, 4);
    cbAutoClampDt_ = new QCheckBox("Auto clamp dt (stable)", gbTime);
    cbAutoClampDt_->setChecked(true);
    cbExportCsv_ = new QCheckBox("Export CSV frames", gbTime);
    cbExportCsv_->setChecked(true);
    sbExportInterval_ = makeDsb(0.01, 10.0, 0.05, 0.20, 2);

    timeForm->addRow("Total (s)", sbTotalTime_);
    timeForm->addRow("dt (s)", sbDt_);
    timeForm->addRow(cbAutoClampDt_);
    timeForm->addRow(cbExportCsv_);
    timeForm->addRow("Export interval (s)", sbExportInterval_);

    auto* gbSrc = new QGroupBox("3D Source", leftInner);
    auto* srcForm = new QFormLayout(gbSrc);

    sbSrcX_ = makeDsb(-1e12, 1e12, 1.0, 0.0, 2);
    sbSrcY_ = makeDsb(-1e12, 1e12, 1.0, 0.0, 2);
    sbSrcZ_ = makeDsb(-1e12, 1e12, 1.0, 0.0, 2);
    sbSrcRadius_ = makeDsb(0.0, 5000.0, 0.5, 2.0, 2);
    sbLeak_ = makeDsb(0.0, 1e9, 0.1, 1.0, 6);

    sbAgl_ = makeDsb(0.0, 2000.0, 0.5, 2.0, 2);
    btnSetSrcZFromGround_ = new QPushButton("Set srcZ = ground + AGL", gbSrc);

    srcForm->addRow("srcX (m)", sbSrcX_);
    srcForm->addRow("srcY (m)", sbSrcY_);
    srcForm->addRow("srcZ (m)", sbSrcZ_);
    srcForm->addRow("radius (m)", sbSrcRadius_);
    srcForm->addRow("leakRate", sbLeak_);
    srcForm->addRow("AGL (m)", sbAgl_);
    srcForm->addRow(btnSetSrcZFromGround_);

    connect(btnSetSrcZFromGround_, &QPushButton::clicked, this, &MainWindow::onSetSrcZFromGroundClicked);

    auto* btnRow = new QWidget(leftInner);
    auto* btnLayout = new QHBoxLayout(btnRow);
    btnRun_ = new QPushButton("Run", btnRow);
    btnPause_ = new QPushButton("Pause", btnRow);
    btnReset_ = new QPushButton("Reset", btnRow);
    btnLayout->addWidget(btnRun_);
    btnLayout->addWidget(btnPause_);
    btnLayout->addWidget(btnReset_);

    connect(btnRun_, &QPushButton::clicked, this, &MainWindow::onRunClicked);
    connect(btnPause_, &QPushButton::clicked, this, &MainWindow::onPauseClicked);
    connect(btnReset_, &QPushButton::clicked, this, &MainWindow::onResetClicked);

    log_ = new QPlainTextEdit(leftInner);
    log_->setReadOnly(true);
    log_->setMaximumBlockCount(2000);
    log_->setPlaceholderText("Log...");

    leftLayout->addWidget(gbTerrain);
    leftLayout->addWidget(gbDem);
    leftLayout->addWidget(gbDomain);
    leftLayout->addWidget(gbZ);
    leftLayout->addWidget(gbPhys);
    leftLayout->addWidget(gbTime);
    leftLayout->addWidget(gbSrc);
    leftLayout->addWidget(btnRow);
    leftLayout->addWidget(log_, 1);

    connect(cbTerrainMode_, &QComboBox::currentIndexChanged, this, &MainWindow::onTerrainParamsChanged);
    connect(sbFlatZ_, &QDoubleSpinBox::valueChanged, this, &MainWindow::onTerrainParamsChanged);
    connect(sbProcBaseZ_, &QDoubleSpinBox::valueChanged, this, &MainWindow::onTerrainParamsChanged);
    connect(sbProcPeakA_, &QDoubleSpinBox::valueChanged, this, &MainWindow::onTerrainParamsChanged);
    connect(sbProcSigma_, &QDoubleSpinBox::valueChanged, this, &MainWindow::onTerrainParamsChanged);
    connect(sbX0_, &QDoubleSpinBox::valueChanged, this, &MainWindow::onTerrainParamsChanged);
    connect(sbY0_, &QDoubleSpinBox::valueChanged, this, &MainWindow::onTerrainParamsChanged);
    connect(sbLx_, &QDoubleSpinBox::valueChanged, this, &MainWindow::onTerrainParamsChanged);
    connect(sbLy_, &QDoubleSpinBox::valueChanged, this, &MainWindow::onTerrainParamsChanged);
    connect(sbDx_, &QDoubleSpinBox::valueChanged, this, &MainWindow::onTerrainParamsChanged);
    connect(sbDy_, &QDoubleSpinBox::valueChanged, this, &MainWindow::onTerrainParamsChanged);
    connect(sbDemStride_, &QSpinBox::valueChanged, this, &MainWindow::onTerrainParamsChanged);
    connect(btnPreviewTerrain_, &QPushButton::clicked, this, &MainWindow::onPreviewTerrainClicked);

    auto* right = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(right);

    view_ = new QLabel(right);
    view_->setMinimumSize(900, 600);
    view_->setAlignment(Qt::AlignCenter);
    view_->setText("Preview Terrain, then Run.");

    status_ = new QLabel(right);

    rightLayout->addWidget(view_, 1);
    rightLayout->addWidget(status_);

    splitter->addWidget(leftScroll_);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    auto* centralLayout = new QVBoxLayout(central);
    centralLayout->addWidget(splitter);

    timer_ = new QTimer(this);
    timer_->setInterval(33);
    connect(timer_, &QTimer::timeout, this, &MainWindow::onTick);

    onTerrainParamsChanged();
}

void MainWindow::onPickDemClicked() {
    const QString tifPath = QFileDialog::getOpenFileName(this, "Select DEM GeoTIFF", QDir::currentPath(), "GeoTIFF (*.tif *.tiff)");
    if (!tifPath.isEmpty()) leDemTif_->setText(tifPath);
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

    QString err;
    const QString metaPath = QDir(outDir).filePath("dem_meta.json");
    const QString binPath  = QDir(outDir).filePath("dem_data.bin");

    if (!dem_.load(metaPath, binPath, err)) {
        appendLog("[DEM] load failed: " + err);
        return;
    }

    hasDem_ = true;
    const auto& m = dem_.meta();
    demInfo_->setText(QString("EPSG:%1 | %2x%3 | dx=%4 dy=%5 | z=[%6,%7]")
                      .arg(m.epsg).arg(m.width).arg(m.height)
                      .arg(m.dx).arg(m.dy)
                      .arg(m.z_min, 0, 'f', 2)
                      .arg(m.z_max, 0, 'f', 2));

    const double cx = (dem_.minX() + dem_.maxX()) * 0.5;
    const double cy = (dem_.minY() + dem_.maxY()) * 0.5;
    sbSrcX_->setValue(cx);
    sbSrcY_->setValue(cy);

    const float gz = dem_.height(cx, cy);
    sbSrcZ_->setValue(gz + sbAgl_->value());
    sbZSlice_->setValue(gz + sbAgl_->value());

    appendLog("[DEM] loaded OK.");

    cbTerrainMode_->setCurrentIndex((int)TerrainMode::Dem);
    onTerrainParamsChanged();
}

void MainWindow::onTerrainParamsChanged() {
    updateDomainInfo();

    flat_.setZ0((float)sbFlatZ_->value());

    const double x0 = sbX0_->value();
    const double y0 = sbY0_->value();
    const double Lx = sbLx_->value();
    const double Ly = sbLy_->value();
    const double xc = x0 + 0.5 * Lx;
    const double yc = y0 + 0.5 * Ly;

    proc_.setMode(TerrainProcedural::Mode::GaussianHill);
    proc_.setBaseZ((float)sbProcBaseZ_->value());
    proc_.setGaussian(TerrainProcedural::Gaussian{(double)xc, (double)yc, sbProcPeakA_->value(), sbProcSigma_->value()});

    QString err;
    if (!buildTerrainPreview(err)) {
        terrainPreviewReady_ = false;
        view_->setText("Terrain preview not available.\n" + err);
        status_->setText(err);
        return;
    }

    renderTerrainOnly();
}

void MainWindow::onPreviewTerrainClicked() {
    onTerrainParamsChanged();
}

bool MainWindow::buildTerrainPreview(QString& errOut) {
    errOut.clear();
    const ITerrain* terr = currentTerrain();
    if (!terr) { errOut = "Terrain is null (DEM not loaded?)"; return false; }
    if (!terr->isValid()) { errOut = "Terrain invalid"; return false; }

    if (terrainMode() == TerrainMode::Dem) {
        if (!hasDem_) { errOut = "DEM mode selected but DEM not loaded"; return false; }

        const auto& m = dem_.meta();
        const int stride = std::max(1, sbDemStride_->value());

        tNx_ = std::max(2, m.width / stride);
        tNy_ = std::max(2, m.height / stride);

        tdx_ = std::abs(m.dx) * stride;
        tdy_ = std::abs(m.dy) * stride;

        tx0_ = std::min(dem_.minX(), dem_.maxX());
        ty0_ = std::min(dem_.minY(), dem_.maxY());
    } else {
        const double Lx = sbLx_->value();
        const double Ly = sbLy_->value();
        tdx_ = std::max(1e-9, sbDx_->value());
        tdy_ = std::max(1e-9, sbDy_->value());
        tNx_ = std::max(2, (int)std::floor(Lx/tdx_) + 1);
        tNy_ = std::max(2, (int)std::floor(Ly/tdy_) + 1);
        tx0_ = sbX0_->value();
        ty0_ = sbY0_->value();
    }

    terrainXY_.assign((std::size_t)tNx_ * tNy_, 0.0f);
    tMin_ = std::numeric_limits<float>::infinity();
    tMax_ = -std::numeric_limits<float>::infinity();

    for (int j = 0; j < tNy_; ++j) {
        const double y = ty0_ + tdy_ * j;
        for (int i = 0; i < tNx_; ++i) {
            const double x = tx0_ + tdx_ * i;
            const float h = terr->height(x, y);
            terrainXY_[(std::size_t)i + (std::size_t)j * tNx_] = h;
            tMin_ = std::min(tMin_, h);
            tMax_ = std::max(tMax_, h);
        }
    }

    terrainPreviewReady_ = true;
    return true;
}

static inline float clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }

void MainWindow::renderTerrainOnly() {
    if (!terrainPreviewReady_) return;

    const int Nx = tNx_;
    const int Ny = tNy_;

    const int maxW = 950, maxH = 650;
    const double sx = (double)Nx / maxW;
    const double sy = (double)Ny / maxH;
    const double s = std::max(1.0, std::max(sx, sy));
    const int W = std::max(1, (int)std::round(Nx / s));
    const int H = std::max(1, (int)std::round(Ny / s));

    QImage img(W, H, QImage::Format_RGB32);

    const float denom = std::max(1e-6f, tMax_ - tMin_);

    const double lx = -1.0, ly = -1.0, lz = 1.0;
    const double ln = std::sqrt(lx*lx + ly*ly + lz*lz);
    const double Lx = lx/ln, Ly = ly/ln, Lz = lz/ln;

    auto Hxy = [&](int i, int j)->float {
        i = std::clamp(i, 0, Nx-1);
        j = std::clamp(j, 0, Ny-1);
        return terrainXY_[(std::size_t)i + (std::size_t)j * Nx];
    };

    for (int y = 0; y < H; ++y) {
        int j = std::clamp((int)std::round(y * s), 0, Ny - 1);
        for (int x = 0; x < W; ++x) {
            int i = std::clamp((int)std::round(x * s), 0, Nx - 1);

            const float h = Hxy(i,j);
            const float n = (h - tMin_) / denom;

            const float dzdx = (Hxy(i+1,j) - Hxy(i-1,j)) / (float)(2.0 * tdx_);
            const float dzdy = (Hxy(i,j+1) - Hxy(i,j-1)) / (float)(2.0 * tdy_);

            double nx = -dzdx, ny = -dzdy, nz = 1.0;
            const double nn = std::sqrt(nx*nx + ny*ny + nz*nz);
            nx /= nn; ny /= nn; nz /= nn;

            const double intensity = std::max(0.0, nx*Lx + ny*Ly + nz*Lz);
            const double shade = 0.45 + 0.55 * intensity;

            int g = (int)std::round((40.0 + 180.0 * n) * shade);
            g = std::clamp(g, 0, 255);

            img.setPixelColor(x, H - 1 - y, QColor(g, g, g));
        }
    }

    view_->setPixmap(QPixmap::fromImage(img).scaled(view_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    status_->setText(QString("TERRAIN | mode=%1 | z=[%2,%3] | Nx=%4 Ny=%5")
                     .arg(cbTerrainMode_->currentText())
                     .arg(tMin_, 0, 'f', 2)
                     .arg(tMax_, 0, 'f', 2)
                     .arg(Nx).arg(Ny));
}

Simulator3D::Params MainWindow::readSimParams() const {
    Simulator3D::Params p;
    p.totalTime_s = sbTotalTime_->value();
    p.dt_s = sbDt_->value();
    p.autoClampDt = cbAutoClampDt_->isChecked();

    p.windSpeed_mps = sbWindSpeed_->value();
    p.windDir_deg = sbWindDir_->value();

    p.K_m2ps = sbK_->value();
    p.decay_1ps = sbDecay_->value();

    p.srcX_m = sbSrcX_->value();
    p.srcY_m = sbSrcY_->value();
    p.srcZ_m = sbSrcZ_->value();
    p.srcRadius_m = sbSrcRadius_->value();
    p.leakRate = sbLeak_->value();
    return p;
}

bool MainWindow::buildSimulation(QString& errOut) {
    errOut.clear();
    const ITerrain* terr = currentTerrain();
    if (!terr) { errOut = "Terrain null (DEM not loaded?)"; return false; }
    if (!terr->isValid()) { errOut = "Terrain invalid"; return false; }

    if (!terrainPreviewReady_) {
        if (!buildTerrainPreview(errOut)) return false;
    }

    Grid3D g;
    g.Nx = tNx_;
    g.Ny = tNy_;
    g.x0 = tx0_;
    g.y0 = ty0_;
    g.dx = tdx_;
    g.dy = tdy_;

    const double dz = sbDz_->value();
    const double z0 = tMin_;
    const double zTop = (double)tMax_ + sbZTopMargin_->value();

    g.z0 = z0;
    g.dz = dz;

    int Nz = (int)std::floor((zTop - g.z0) / g.dz) + 1;
    Nz = std::clamp(Nz, 2, sbNzMax_->value());
    g.Nz = Nz;

    const auto p = readSimParams();

    if (!sim_.initialize(g, terr, p, errOut)) return false;

    simReady_ = true;
    running_ = false;
    nextExportT_ = 0.0;
    frameId_ = 0;

    appendLog(QString("[SIM] grid Nx=%1 Ny=%2 Nz=%3 | dx=%4 dy=%5 dz=%6")
              .arg(g.Nx).arg(g.Ny).arg(g.Nz).arg(g.dx).arg(g.dy).arg(g.dz));
    appendLog(QString("[SIM] stable dt <= %1").arg(sim_.stableDt(), 0, 'g', 6));

    return true;
}

void MainWindow::renderTerrainAndSlice() {
    if (!simReady_ || !terrainPreviewReady_) { renderTerrainOnly(); return; }

    const auto& g = sim_.grid();
    const double zSlice = sbZSlice_->value();
    int k = (int)std::round((zSlice - g.z0) / g.dz);
    k = std::clamp(k, 0, g.Nz - 1);

    sim_.extractSliceXY(k, slice_, sliceMax_);
    const int Nx = g.Nx;
    const int Ny = g.Ny;

    const int maxW = 950, maxH = 650;
    const double sx = (double)Nx / maxW;
    const double sy = (double)Ny / maxH;
    const double s = std::max(1.0, std::max(sx, sy));
    const int W = std::max(1, (int)std::round(Nx / s));
    const int H = std::max(1, (int)std::round(Ny / s));

    QImage img(W, H, QImage::Format_RGB32);

    const float denomH = std::max(1e-6f, tMax_ - tMin_);
    const float maxC = std::max(1e-12f, sliceMax_);

    const double lx = -1.0, ly = -1.0, lz = 1.0;
    const double ln = std::sqrt(lx*lx + ly*ly + lz*lz);
    const double Lx = lx/ln, Ly = ly/ln, Lz = lz/ln;

    auto Hxy = [&](int i, int j)->float {
        i = std::clamp(i, 0, Nx-1);
        j = std::clamp(j, 0, Ny-1);
        return terrainXY_[(std::size_t)i + (std::size_t)j * Nx];
    };

    for (int y = 0; y < H; ++y) {
        int j = std::clamp((int)std::round(y * s), 0, Ny - 1);
        for (int x = 0; x < W; ++x) {
            int i = std::clamp((int)std::round(x * s), 0, Nx - 1);

            const float h = Hxy(i,j);
            const float hn = (h - tMin_) / denomH;

            const float dzdx = (Hxy(i+1,j) - Hxy(i-1,j)) / (float)(2.0 * g.dx);
            const float dzdy = (Hxy(i,j+1) - Hxy(i,j-1)) / (float)(2.0 * g.dy);

            double nx = -dzdx, ny = -dzdy, nz = 1.0;
            const double nn = std::sqrt(nx*nx + ny*ny + nz*nz);
            nx/=nn; ny/=nn; nz/=nn;
            const double intensity = std::max(0.0, nx*Lx + ny*Ly + nz*Lz);
            const double shade = 0.45 + 0.55 * intensity;

            int bg = (int)std::round((40.0 + 180.0 * hn) * shade);
            bg = std::clamp(bg, 0, 255);

            double r = bg, gch = bg, b = bg;

            const float c = slice_[(std::size_t)i + (std::size_t)j * Nx];
            if (c > 0.0f) {
                const float cn = clamp01(c / maxC);
                QColor cc = colorMap(cn);
                const double a = 0.15 + 0.75 * std::sqrt(cn);
                r = (1.0 - a) * r + a * cc.red();
                gch = (1.0 - a) * gch + a * cc.green();
                b = (1.0 - a) * b + a * cc.blue();
            }

            img.setPixelColor(x, H - 1 - y, QColor((int)r, (int)gch, (int)b));
        }
    }

    view_->setPixmap(QPixmap::fromImage(img).scaled(view_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    status_->setText(QString("SIM | t=%.3f s | zSlice=%.2f (k=%1/%2) | maxC=%.6g | terrain[%3,%4]")
                     .arg(sim_.time(), 0, 'f', 3)
                     .arg(g.z(k), 0, 'f', 2)
                     .arg(k).arg(g.Nz - 1)
                     .arg(sliceMax_, 0, 'g', 6)
                     .arg(tMin_, 0, 'f', 2)
                     .arg(tMax_, 0, 'f', 2));
}

void MainWindow::onSetSrcZFromGroundClicked() {
    const ITerrain* terr = currentTerrain();
    if (!terr || !terr->isValid()) { appendLog("[SRC] terrain invalid."); return; }
    const double x = sbSrcX_->value();
    const double y = sbSrcY_->value();
    const float gz = terr->height(x, y);
    const double agl = sbAgl_->value();
    sbSrcZ_->setValue(gz + agl);
    appendLog(QString("[SRC] srcZ = ground(%.2f)+AGL(%.2f)=%.2f").arg(gz).arg(agl).arg(gz+agl));
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
    if (simReady_) sim_.reset();
    nextExportT_ = 0.0;
    frameId_ = 0;
    renderTerrainAndSlice();
    appendLog("[Reset] done.");
}

void MainWindow::onTick() {
    if (!running_ || !simReady_) return;

    const auto p = sim_.params();
    if (sim_.time() >= p.totalTime_s) {
        running_ = false;
        timer_->stop();
        appendLog("[Done] reached total time.");
        return;
    }

    for (int s = 0; s < 2; ++s) sim_.step();

    renderTerrainAndSlice();

    if (cbExportCsv_->isChecked()) {
        const double t = sim_.time();
        if (t + 1e-12 >= nextExportT_) {
            const QString dir = framesDir();
            QDir().mkpath(dir);

            const auto& g = sim_.grid();
            const double zSlice = sbZSlice_->value();
            int k = (int)std::round((zSlice - g.z0) / g.dz);
            k = std::clamp(k, 0, g.Nz - 1);

            std::vector<float> grid2d;
            float mx = 0.0f;
            sim_.extractSliceXY(k, grid2d, mx);

            ExporterCsv::FrameMeta meta;
            meta.epsg = (terrainMode() == TerrainMode::Dem && hasDem_) ? dem_.meta().epsg : 0;
            meta.origin_x = g.x0;
            meta.origin_y = g.y0;
            meta.dx = g.dx;
            meta.dy = g.dy;
            meta.z  = g.z(k);
            meta.Nx = g.Nx;
            meta.Ny = g.Ny;
            meta.t  = t;

            const QString path = QDir(dir).filePath(QString("frame_%1.csv").arg(frameId_, 4, 10, QLatin1Char('0')));
            QString err;
            if (!ExporterCsv::writeGridFrame(path, meta, grid2d, err)) {
                appendLog("[CSV] write failed: " + err);
            } else {
                appendLog("[CSV] wrote: " + path);
            }

            frameId_++;
            nextExportT_ += sbExportInterval_->value();
        }
    }
}
