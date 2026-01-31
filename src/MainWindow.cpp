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

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("GasDispersionDesktop - Terrain DEM + 3D Source (Internal CFD Scalar)");
    resize(1300, 760);

    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* splitter = new QSplitter(Qt::Horizontal, central);

    auto* left = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(left);

    auto* gbDem = new QGroupBox("Terrain DEM (GeoTIFF -> meta+bin)", left);
    auto* demForm = new QFormLayout(gbDem);

    leDemTif_ = new QLineEdit(gbDem);
    leDemTif_->setPlaceholderText("Select GeoTIFF DEM (.tif, projected meters)");
    btnLoadDem_ = new QPushButton("Convert+Load", gbDem);
    sbDemStride_ = makeSsb(1, 16, 1, 2);
    demInfo_ = new QLabel("DEM: not loaded", gbDem);
    demInfo_->setWordWrap(true);

    auto* demRow = new QWidget(gbDem);
    auto* demRowLayout = new QHBoxLayout(demRow);
    demRowLayout->addWidget(leDemTif_, 1);
    demRowLayout->addWidget(btnLoadDem_);

    demForm->addRow("GeoTIFF path", demRow);
    demForm->addRow("XY stride", sbDemStride_);
    demForm->addRow("Info", demInfo_);

    connect(btnLoadDem_, &QPushButton::clicked, this, &MainWindow::onLoadDemClicked);

    auto* gbZ = new QGroupBox("Vertical Domain", left);
    auto* zForm = new QFormLayout(gbZ);
    sbDz_ = makeDsb(0.1, 100.0, 0.1, 2.0, 2);
    sbZTopMargin_ = makeDsb(1.0, 5000.0, 1.0, 200.0, 1);
    sbNzMax_ = makeSsb(10, 2000, 10, 300);
    sbZSlice_ = makeDsb(-1e9, 1e9, 1.0, 0.0, 2);
    zForm->addRow("dz (m)", sbDz_);
    zForm->addRow("zTop margin (m)", sbZTopMargin_);
    zForm->addRow("Nz max", sbNzMax_);
    zForm->addRow("zSlice (m)", sbZSlice_);

    auto* gbPhys = new QGroupBox("Wind & Physics", left);
    auto* physForm = new QFormLayout(gbPhys);
    sbWindSpeed_ = makeDsb(0.0, 50.0, 0.1, 2.0, 3);
    sbWindDir_   = makeDsb(-180.0, 180.0, 1.0, 0.0, 1);
    sbK_         = makeDsb(0.0, 1e5, 0.1, 1.0, 4);
    sbDecay_     = makeDsb(0.0, 10.0, 0.001, 0.0, 4);
    physForm->addRow("Wind speed (m/s)", sbWindSpeed_);
    physForm->addRow("Wind dir (deg)", sbWindDir_);
    physForm->addRow("K (m^2/s)", sbK_);
    physForm->addRow("decay k (1/s)", sbDecay_);

    auto* gbTime = new QGroupBox("Time & Output", left);
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

    auto* gbSrc = new QGroupBox("3D Source", left);
    auto* srcForm = new QFormLayout(gbSrc);

    sbSrcX_ = makeDsb(-1e12, 1e12, 1.0, 0.0, 2);
    sbSrcY_ = makeDsb(-1e12, 1e12, 1.0, 0.0, 2);
    sbSrcZ_ = makeDsb(-1e12, 1e12, 1.0, 0.0, 2);
    sbSrcRadius_ = makeDsb(0.0, 5000.0, 0.5, 2.0, 2);
    sbLeak_ = makeDsb(0.0, 1e9, 0.1, 1.0, 6);

    sbAgl_ = makeDsb(0.0, 2000.0, 0.5, 2.0, 2);
    btnSetSrcZFromGround_ = new QPushButton("Set srcZ = ground + AGL", gbSrc);
    connect(btnSetSrcZFromGround_, &QPushButton::clicked, this, &MainWindow::onSetSrcZFromGroundClicked);

    srcForm->addRow("srcX (m)", sbSrcX_);
    srcForm->addRow("srcY (m)", sbSrcY_);
    srcForm->addRow("srcZ (m)", sbSrcZ_);
    srcForm->addRow("radius (m)", sbSrcRadius_);
    srcForm->addRow("leakRate", sbLeak_);
    srcForm->addRow("AGL (m)", sbAgl_);
    srcForm->addRow(btnSetSrcZFromGround_);

    auto* btnRow = new QWidget(left);
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

    log_ = new QPlainTextEdit(left);
    log_->setReadOnly(true);
    log_->setMaximumBlockCount(2000);
    log_->setPlaceholderText("Log...");

    leftLayout->addWidget(gbDem);
    leftLayout->addWidget(gbZ);
    leftLayout->addWidget(gbPhys);
    leftLayout->addWidget(gbTime);
    leftLayout->addWidget(gbSrc);
    leftLayout->addWidget(btnRow);
    leftLayout->addWidget(log_, 1);

    auto* right = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(right);

    view_ = new QLabel(right);
    view_->setMinimumSize(900, 600);
    view_->setAlignment(Qt::AlignCenter);
    view_->setText("Load DEM then Run.");

    status_ = new QLabel(right);

    rightLayout->addWidget(view_, 1);
    rightLayout->addWidget(status_);

    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    auto* centralLayout = new QVBoxLayout(central);
    centralLayout->addWidget(splitter);

    timer_ = new QTimer(this);
    timer_->setInterval(33);
    connect(timer_, &QTimer::timeout, this, &MainWindow::onTick);
}

QString MainWindow::framesDir() const {
    return QDir::current().filePath("outputs/frames");
}

void MainWindow::appendLog(const QString& s) {
    log_->appendPlainText(s);
}

void MainWindow::onLoadDemClicked() {
    QString tifPath = leDemTif_->text().trimmed();
    if (tifPath.isEmpty()) {
        tifPath = QFileDialog::getOpenFileName(this, "Select DEM GeoTIFF", QDir::currentPath(), "GeoTIFF (*.tif *.tiff)");
        if (tifPath.isEmpty()) return;
        leDemTif_->setText(tifPath);
    }

    const QString outDir = QDir::current().filePath("outputs/dem_cache");
    QDir().mkpath(outDir);

    const QString cmd = QString("python tools/dem_convert.py --in \"%1\" --out_dir \"%2\"")
                        .arg(tifPath).arg(outDir);

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

    const float gz = dem_.sampleBilinear(cx, cy);
    sbSrcZ_->setValue(gz + sbAgl_->value());

    sbZSlice_->setValue(gz + sbAgl_->value());

    appendLog("[DEM] loaded OK.");
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
    if (!hasDem_) { errOut = "DEM not loaded"; return false; }

    const auto& m = dem_.meta();
    const int stride = std::max(1, sbDemStride_->value());

    Grid3D g;
    g.Nx = std::max(2, m.width / stride);
    g.Ny = std::max(2, m.height / stride);

    g.dx = m.dx * stride;
    g.dy = m.dy * stride;

    g.x0 = m.origin_x;
    g.y0 = m.origin_y;

    const double dz = sbDz_->value();
    const double zTop = m.z_max + sbZTopMargin_->value();

    g.z0 = m.z_min;
    g.dz = dz;

    int Nz = static_cast<int>(std::floor((zTop - g.z0) / g.dz)) + 1;
    Nz = std::clamp(Nz, 2, sbNzMax_->value());
    g.Nz = Nz;

    Simulator3D::Params p = readSimParams();

    if (!sim_.initialize(g, dem_, p, errOut)) return false;

    simReady_ = true;
    running_ = false;

    nextExportT_ = 0.0;
    frameId_ = 0;

    appendLog(QString("[SIM] grid: Nx=%1 Ny=%2 Nz=%3 | dx=%4 dy=%5 dz=%6")
              .arg(g.Nx).arg(g.Ny).arg(g.Nz)
              .arg(g.dx).arg(g.dy).arg(g.dz));

    appendLog(QString("[SIM] stable dt <= %1 (autoClamp=%2)")
              .arg(sim_.stableDt(), 0, 'g', 6)
              .arg(p.autoClampDt));

    return true;
}

void MainWindow::renderSlice() {
    if (!simReady_) return;

    const auto& g = sim_.grid();
    const double zSlice = sbZSlice_->value();
    int k = static_cast<int>(std::round((zSlice - g.z0) / g.dz));
    k = std::clamp(k, 0, g.Nz - 1);

    sim_.extractSliceXY(k, slice_, sliceMax_);
    const int Nx = g.Nx;
    const int Ny = g.Ny;

    const int maxW = 900, maxH = 600;
    const double sx = (double)Nx / maxW;
    const double sy = (double)Ny / maxH;
    const double s = std::max(1.0, std::max(sx, sy));
    const int W = std::max(1, (int)std::round(Nx / s));
    const int H = std::max(1, (int)std::round(Ny / s));

    QImage img(W, H, QImage::Format_RGB32);

    const float maxC = std::max(1e-12f, sliceMax_);
    for (int y = 0; y < H; ++y) {
        int j = std::clamp((int)std::round(y * s), 0, Ny - 1);
        for (int x = 0; x < W; ++x) {
            int i = std::clamp((int)std::round(x * s), 0, Nx - 1);
            const float c = slice_[static_cast<std::size_t>(i) + static_cast<std::size_t>(j) * Nx];
            const float n = c / maxC;
            img.setPixelColor(x, H - 1 - y, colorMap(n));
        }
    }

    view_->setPixmap(QPixmap::fromImage(img).scaled(view_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    status_->setText(QString("t=%.3f s | zSlice=%.2f (k=%1/%2) | maxC=%.6g | EPSG:%3")
                     .arg(sim_.time(), 0, 'f', 3)
                     .arg(g.z(k), 0, 'f', 2)
                     .arg(k).arg(g.Nz - 1)
                     .arg(sliceMax_, 0, 'g', 6)
                     .arg(dem_.meta().epsg));
}

void MainWindow::onSetSrcZFromGroundClicked() {
    if (!hasDem_) { appendLog("[SRC] DEM not loaded."); return; }
    const double x = sbSrcX_->value();
    const double y = sbSrcY_->value();
    const float gz = dem_.sampleBilinear(x, y);
    const double agl = sbAgl_->value();
    sbSrcZ_->setValue(gz + agl);
    appendLog(QString("[SRC] srcZ set to ground(%.2f)+AGL(%.2f)=%.2f").arg(gz).arg(agl).arg(gz+agl));
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
    sim_.reset();
    nextExportT_ = 0.0;
    frameId_ = 0;
    renderSlice();
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

    renderSlice();

    if (cbExportCsv_->isChecked()) {
        const double t = sim_.time();
        if (t + 1e-12 >= nextExportT_) {
            const QString dir = framesDir();
            QDir().mkpath(dir);

            const auto& g = sim_.grid();
            const double zSlice = sbZSlice_->value();
            int k = static_cast<int>(std::round((zSlice - g.z0) / g.dz));
            k = std::clamp(k, 0, g.Nz - 1);

            std::vector<float> grid2d;
            float mx = 0.0f;
            sim_.extractSliceXY(k, grid2d, mx);

            ExporterCsv::FrameMeta meta;
            meta.epsg = dem_.meta().epsg;
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
