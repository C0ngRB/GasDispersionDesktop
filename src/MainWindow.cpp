#include "MainWindow.h"
#include "ColorMap.h"
#include "ExporterCsv.h"

#include <QtWidgets/QWidget>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QSplitter>
#include <QDir>
#include <QFileDialog>
#include <algorithm>

static QDoubleSpinBox *makeDsb(double minV, double maxV, double step, double val, int decimals = 3)
{
  auto *sb = new QDoubleSpinBox();
  sb->setRange(minV, maxV);
  sb->setDecimals(decimals);
  sb->setSingleStep(step);
  sb->setValue(val);
  return sb;
}
static QSpinBox *makeSsb(int minV, int maxV, int step, int val)
{
  auto *sb = new QSpinBox();
  sb->setRange(minV, maxV);
  sb->setSingleStep(step);
  sb->setValue(val);
  return sb;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
  setWindowTitle("GasDispersionDesktop - Internal / External / 3D Terrain CFD");
  resize(1300, 760);

  auto *central = new QWidget(this);
  setCentralWidget(central);

  auto *splitter = new QSplitter(Qt::Horizontal, central);

  auto *left = new QWidget(splitter);
  auto *leftLayout = new QVBoxLayout(left);

  auto *gbBackend = new QGroupBox("Backend", left);
  auto *backendForm = new QFormLayout(gbBackend);
  cbBackend_ = new QComboBox(gbBackend);
  cbBackend_->addItem("Internal (built-in)");
  cbBackend_->addItem("External CFD (runner.bat)");
  cbBackend_->addItem("3D Terrain (DEM + 3D source)");
  leRunner_ = new QLineEdit(gbBackend);
  leRunner_->setPlaceholderText("e.g. tools\\run_mock_cfd.bat");
  leRunner_->setText("tools\\run_mock_cfd.bat");
  backendForm->addRow("Mode", cbBackend_);
  backendForm->addRow("Runner", leRunner_);

  auto *gbDem = new QGroupBox("Terrain DEM (GeoTIFF)", left);
  auto *demForm = new QFormLayout(gbDem);
  leDemTif_ = new QLineEdit(gbDem);
  leDemTif_->setPlaceholderText("Select GeoTIFF DEM (.tif)");
  btnLoadDem_ = new QPushButton("Convert+Load", gbDem);
  sbDemStride_ = makeSsb(1, 16, 1, 2);
  demInfo_ = new QLabel("DEM: not loaded", gbDem);
  demInfo_->setWordWrap(true);

  auto *demRow = new QWidget(gbDem);
  auto *demRowLayout = new QHBoxLayout(demRow);
  demRowLayout->addWidget(leDemTif_, 1);
  demRowLayout->addWidget(btnLoadDem_);

  demForm->addRow("GeoTIFF path", demRow);
  demForm->addRow("XY stride", sbDemStride_);
  demForm->addRow("Info", demInfo_);

  connect(btnLoadDem_, &QPushButton::clicked, this, &MainWindow::onLoadDemClicked);

  auto *gbWind = new QGroupBox("Wind", left);
  auto *windForm = new QFormLayout(gbWind);
  sbWindSpeed_ = makeDsb(0.0, 50.0, 0.1, 2.0, 3);
  sbWindDir_   = makeDsb(-180.0, 180.0, 1.0, 0.0, 1);
  windForm->addRow("Speed (m/s)", sbWindSpeed_);
  windForm->addRow("Direction (deg, 0=+x)", sbWindDir_);

  auto *gbDomain = new QGroupBox("Domain & Grid (2D)", left);
  auto *domainForm = new QFormLayout(gbDomain);
  sbLx_ = makeDsb(10.0, 5000.0, 10.0, 200.0, 1);
  sbLy_ = makeDsb(10.0, 5000.0, 10.0, 100.0, 1);
  sbNx_ = makeSsb(50, 2000, 50, 300);
  sbNy_ = makeSsb(50, 2000, 50, 150);
  domainForm->addRow("Lx (m)", sbLx_);
  domainForm->addRow("Ly (m)", sbLy_);
  domainForm->addRow("Nx", sbNx_);
  domainForm->addRow("Ny", sbNy_);

  auto *gbZ3D = new QGroupBox("Vertical Domain (3D)", left);
  auto *zForm = new QFormLayout(gbZ3D);
  sbDz_ = makeDsb(0.1, 100.0, 0.1, 2.0, 2);
  sbZTopMargin_ = makeDsb(1.0, 5000.0, 1.0, 200.0, 1);
  sbNzMax_ = makeSsb(10, 2000, 10, 300);
  sbZSlice_ = makeDsb(-1e9, 1e9, 1.0, 0.0, 2);
  zForm->addRow("dz (m)", sbDz_);
  zForm->addRow("zTop margin (m)", sbZTopMargin_);
  zForm->addRow("Nz max", sbNzMax_);
  zForm->addRow("zSlice (m)", sbZSlice_);

  auto *gbTime = new QGroupBox("Time", left);
  auto *timeForm = new QFormLayout(gbTime);
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

  auto *gbPhys = new QGroupBox("Dispersion", left);
  auto *physForm = new QFormLayout(gbPhys);
  sbD_     = makeDsb(0.0, 1e5, 0.1, 1.0, 4);
  sbDecay_ = makeDsb(0.0, 10.0, 0.001, 0.0, 4);
  physForm->addRow("D (m^2/s)", sbD_);
  physForm->addRow("decay k (1/s)", sbDecay_);

  auto *gbSrc = new QGroupBox("Source", left);
  auto *srcForm = new QFormLayout(gbSrc);
  sbSrcX_ = makeDsb(0.0, 1e6, 1.0, 20.0, 2);
  sbSrcY_ = makeDsb(0.0, 1e6, 1.0, 50.0, 2);
  sbSrcZ_ = makeDsb(-1e6, 1e6, 1.0, 2.0, 2);
  sbLeak_ = makeDsb(0.0, 1e9, 0.1, 1.0, 6);
  sbH_    = makeDsb(0.01, 1000.0, 0.1, 1.0, 3);
  sbSrcRadius_ = makeDsb(0.0, 5000.0, 0.5, 2.0, 2);
  srcForm->addRow("x (m)", sbSrcX_);
  srcForm->addRow("y (m)", sbSrcY_);
  srcForm->addRow("z (m, 3D)", sbSrcZ_);
  srcForm->addRow("Leak Q", sbLeak_);
  srcForm->addRow("Effective height H (m)", sbH_);
  srcForm->addRow("srcRadius (m, 3D)", sbSrcRadius_);

  btnSetSrcZFromGround_ = new QPushButton("Set srcZ = ground + H", gbSrc);
  connect(btnSetSrcZFromGround_, &QPushButton::clicked, this, &MainWindow::onSetSrcZFromGroundClicked);
  srcForm->addRow(btnSetSrcZFromGround_);

  auto *btnRow = new QWidget(left);
  auto *btnLayout = new QHBoxLayout(btnRow);
  btnRun_ = new QPushButton("Run", btnRow);
  btnPause_ = new QPushButton("Pause", btnRow);
  btnReset_ = new QPushButton("Reset", btnRow);
  btnLayout->addWidget(btnRun_);
  btnLayout->addWidget(btnPause_);
  btnLayout->addWidget(btnReset_);

  log_ = new QPlainTextEdit(left);
  log_->setReadOnly(true);
  log_->setMaximumBlockCount(2000);
  log_->setPlaceholderText("Log...");

  leftLayout->addWidget(gbBackend);
  leftLayout->addWidget(gbDem);
  leftLayout->addWidget(gbWind);
  leftLayout->addWidget(gbDomain);
  leftLayout->addWidget(gbZ3D);
  leftLayout->addWidget(gbTime);
  leftLayout->addWidget(gbPhys);
  leftLayout->addWidget(gbSrc);
  leftLayout->addWidget(btnRow);
  leftLayout->addWidget(log_, 1);

  auto *right = new QWidget(splitter);
  auto *rightLayout = new QVBoxLayout(right);
  view_ = new QLabel(right);
  view_->setMinimumSize(900, 600);
  view_->setAlignment(Qt::AlignCenter);
  view_->setText("Click Run to start.");
  status_ = new QLabel(right);
  rightLayout->addWidget(view_, 1);
  rightLayout->addWidget(status_);

  splitter->addWidget(left);
  splitter->addWidget(right);
  splitter->setStretchFactor(0, 0);
  splitter->setStretchFactor(1, 1);
  auto *centralLayout = new QVBoxLayout(central);
  centralLayout->addWidget(splitter);

  timer_ = new QTimer(this);
  timer_->setInterval(33);
  connect(timer_, &QTimer::timeout, this, &MainWindow::onTick);
  connect(btnRun_, &QPushButton::clicked, this, &MainWindow::onRunClicked);
  connect(btnPause_, &QPushButton::clicked, this, &MainWindow::onPauseClicked);
  connect(btnReset_, &QPushButton::clicked, this, &MainWindow::onResetClicked);

  ext_ = new ExternalCfdBackend(this);
  connect(ext_, &ExternalCfdBackend::logLine, this, &MainWindow::onExternalLog);
  connect(ext_, &ExternalCfdBackend::newFrameAvailable, this, &MainWindow::onExternalFrame);
  connect(ext_, &ExternalCfdBackend::finished, this, &MainWindow::onExternalFinished);

  rebuildSimulator();
  renderFieldInternal();
}

SimParams MainWindow::readParams() const
{
  SimParams p;
  p.windSpeed_mps = sbWindSpeed_->value();
  p.windDir_deg   = sbWindDir_->value();
  p.Lx_m = sbLx_->value();
  p.Ly_m = sbLy_->value();
  p.Nx   = sbNx_->value();
  p.Ny   = sbNy_->value();
  p.totalTime_s = sbTotalTime_->value();
  p.dt_s        = sbDt_->value();
  p.autoClampDt = cbAutoClampDt_->isChecked();
  p.D_m2ps     = sbD_->value();
  p.decay_1ps = sbDecay_->value();
  p.srcX_m = std::clamp(sbSrcX_->value(), 0.0, p.Lx_m);
  p.srcY_m = std::clamp(sbSrcY_->value(), 0.0, p.Ly_m);
  p.leakRate = sbLeak_->value();
  p.effectiveHeight_m = sbH_->value();
  return p;
}

Simulator3D::Params MainWindow::readParams3D() const
{
  Simulator3D::Params p;
  p.totalTime_s = sbTotalTime_->value();
  p.dt_s = sbDt_->value();
  p.autoClampDt = cbAutoClampDt_->isChecked();
  p.windSpeed_mps = sbWindSpeed_->value();
  p.windDir_deg = sbWindDir_->value();
  p.K_m2ps = sbD_->value();
  p.decay_1ps = sbDecay_->value();
  p.srcX_m = sbSrcX_->value();
  p.srcY_m = sbSrcY_->value();
  p.srcZ_m = sbSrcZ_->value();
  p.srcRadius_m = sbSrcRadius_->value();
  p.leakRate = sbLeak_->value();
  return p;
}

QString MainWindow::framesDir() const {
  return QDir::current().filePath("outputs/frames");
}

void MainWindow::appendLog(const QString &s)
{
  log_->appendPlainText(s);
}

void MainWindow::onLoadDemClicked()
{
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
    appendLog("[DEM] convert failed. Ensure rasterio or gdal is installed.");
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
  sbSrcZ_->setValue(gz + sbH_->value());
  sbZSlice_->setValue(gz + sbH_->value());

  appendLog("[DEM] loaded OK.");
}

void MainWindow::onSetSrcZFromGroundClicked()
{
  if (!hasDem_) { appendLog("[SRC] DEM not loaded."); return; }
  const double x = sbSrcX_->value();
  const double y = sbSrcY_->value();
  const float gz = dem_.sampleBilinear(x, y);
  const double h = sbH_->value();
  sbSrcZ_->setValue(gz + h);
  appendLog(QString("[SRC] srcZ set to ground(%.2f)+H(%.2f)=%.2f").arg(gz).arg(h).arg(gz+h));
}

void MainWindow::rebuildSimulator()
{
  const auto p = readParams();
  if (!sim_)
    sim_ = new Simulator(p);
  else
    sim_->reset(p);
}

void MainWindow::renderFieldInternal()
{
  if (!sim_)
    return;

  const int Nx = sim_->Nx();
  const int Ny = sim_->Ny();
  const auto& f = sim_->field();
  const int maxW = 900, maxH = 600;
  const double sx = (double)Nx / maxW;
  const double sy = (double)Ny / maxH;
  const double s = std::max(1.0, std::max(sx, sy));
  const int W = std::max(1, (int)std::round(Nx / s));
  const int H = std::max(1, (int)std::round(Ny / s));
  QImage img(W, H, QImage::Format_RGB32);
  const float maxC = std::max(1e-12f, sim_->maxC());
  for (int y = 0; y < H; ++y) {
    int j = std::clamp((int)std::round(y * s), 0, Ny - 1);
    for (int x = 0; x < W; ++x) {
      int i = std::clamp((int)std::round(x * s), 0, Nx - 1);
      float c = f[i + j * Nx];
      float n = c / maxC;
      const QColor col = colorMap(n);
      img.setPixelColor(x, H - 1 - y, col);
    }
  }
  view_->setPixmap(QPixmap::fromImage(img).scaled(view_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
  status_->setText(QString("INTERNAL | t=%.3f s | maxC=%.6g | grid=%1x%2")
                   .arg(sim_->time(), 0, 'f', 3)
                   .arg(sim_->maxC(), 0, 'g', 6)
                   .arg(Nx).arg(Ny));
}

void MainWindow::renderFieldExternal(const CsvFrameReader::Frame& fr)
{
  const int Nx = fr.Nx;
  const int Ny = fr.Ny;
  const auto& f = fr.data;
  const int maxW = 900, maxH = 600;
  const double sx = (double)Nx / maxW;
  const double sy = (double)Ny / maxH;
  const double s = std::max(1.0, std::max(sx, sy));
  const int W = std::max(1, (int)std::round(Nx / s));
  const int H = std::max(1, (int)std::round(Ny / s));
  QImage img(W, H, QImage::Format_RGB32);
  float maxC = 1e-12f;
  for (float v : f) maxC = std::max(maxC, v);
  for (int y = 0; y < H; ++y) {
    int j = std::clamp((int)std::round(y * s), 0, Ny - 1);
    for (int x = 0; x < W; ++x) {
      int i = std::clamp((int)std::round(x * s), 0, Nx - 1);
      float c = f[(size_t)i + (size_t)j * (size_t)Nx];
      float n = c / maxC;
      img.setPixelColor(x, H - 1 - y, colorMap(n));
    }
  }
  view_->setPixmap(QPixmap::fromImage(img).scaled(view_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
  status_->setText(QString("EXTERNAL | t=%.3f s | maxC=%.6g | grid=%1x%2")
                   .arg(fr.t, 0, 'f', 3)
                   .arg(maxC, 0, 'g', 6)
                   .arg(Nx).arg(Ny));
}

bool MainWindow::buildSimulation3D(QString& errOut)
{
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

  Simulator3D::Params p = readParams3D();

  if (!sim3d_.initialize(g, dem_, p, errOut)) return false;

  sim3dReady_ = true;
  return true;
}

void MainWindow::renderField3D()
{
  if (!sim3dReady_) return;

  const auto& g = sim3d_.grid();
  const double zSlice = sbZSlice_->value();
  int k = static_cast<int>(std::round((zSlice - g.z0) / g.dz));
  k = std::clamp(k, 0, g.Nz - 1);

  std::vector<float> slice;
  float sliceMax = 0.0f;
  sim3d_.extractSliceXY(k, slice, sliceMax);

  const int Nx = g.Nx;
  const int Ny = g.Ny;

  const int maxW = 900, maxH = 600;
  const double sx = (double)Nx / maxW;
  const double sy = (double)Ny / maxH;
  const double s = std::max(1.0, std::max(sx, sy));
  const int W = std::max(1, (int)std::round(Nx / s));
  const int H = std::max(1, (int)std::round(Ny / s));

  QImage img(W, H, QImage::Format_RGB32);
  const float maxC = std::max(1e-12f, sliceMax);

  for (int y = 0; y < H; ++y) {
    int j = std::clamp((int)std::round(y * s), 0, Ny - 1);
    for (int x = 0; x < W; ++x) {
      int i = std::clamp((int)std::round(x * s), 0, Nx - 1);
      const float c = slice[static_cast<std::size_t>(i) + static_cast<std::size_t>(j) * Nx];
      const float n = c / maxC;
      img.setPixelColor(x, H - 1 - y, colorMap(n));
    }
  }

  view_->setPixmap(QPixmap::fromImage(img).scaled(view_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

  status_->setText(QString("3D TERRAIN | t=%.3f s | zSlice=%.2f (k=%1/%2) | maxC=%.6g | EPSG:%3")
                   .arg(sim3d_.time(), 0, 'f', 3)
                   .arg(g.z(k), 0, 'f', 2)
                   .arg(k).arg(g.Nz - 1)
                   .arg(sliceMax, 0, 'g', 6)
                   .arg(dem_.meta().epsg));
}

void MainWindow::onRunClicked()
{
  mode_ = static_cast<BackendMode>(cbBackend_->currentIndex());
  if (mode_ == BackendMode::Internal) {
    rebuildSimulator();
    running_ = true;
    timer_->start();
    appendLog("[Run] INTERNAL start");
    return;
  }
  if (mode_ == BackendMode::External) {
    hasLastFrame_ = false;
    running_ = true;
    timer_->start();
    appendLog("[Run] EXTERNAL start");
    ExternalCfdBackend::Config cfg;
    cfg.runnerPath = leRunner_->text().trimmed();
    cfg.workRootDir = "work";
    cfg.pollIntervalMs = 100;
    ext_->start(readParams(), cfg);
    return;
  }
  if (mode_ == BackendMode::Terrain3D) {
    QString err;
    if (!buildSimulation3D(err)) {
      appendLog("[Run] 3D build failed: " + err);
      return;
    }
    running_ = true;
    timer_->start();
    appendLog("[Run] 3D TERRAIN start");
  }
}

void MainWindow::onPauseClicked()
{
  running_ = false;
  timer_->stop();
  if (mode_ == BackendMode::External) ext_->stop();
  appendLog("[Pause] stopped");
}

void MainWindow::onResetClicked()
{
  running_ = false;
  timer_->stop();
  if (mode_ == BackendMode::External) ext_->stop();
  if (mode_ == BackendMode::Internal) {
    rebuildSimulator();
    renderFieldInternal();
  } else if (mode_ == BackendMode::Terrain3D) {
    sim3dReady_ = false;
  }
  appendLog("[Reset] done");
}

void MainWindow::onTick()
{
  if (!running_) return;
  if (mode_ == BackendMode::Internal) {
    const auto p = readParams();
    if (sim_->time() >= p.totalTime_s) {
      running_ = false; timer_->stop();
      appendLog("[Done] INTERNAL reached total time");
      return;
    }
    for (int k = 0; k < 2; ++k) sim_->step();
    renderFieldInternal();
    return;
  }
  if (mode_ == BackendMode::External) {
    if (hasLastFrame_) renderFieldExternal(lastFrame_);
    return;
  }
  if (mode_ == BackendMode::Terrain3D) {
    const auto p = readParams3D();
    if (sim3d_.time() >= p.totalTime_s) {
      running_ = false; timer_->stop();
      appendLog("[Done] 3D TERRAIN reached total time");
      return;
    }
    for (int s = 0; s < 2; ++s) sim3d_.step();
    renderField3D();

    if (cbExportCsv_->isChecked()) {
      const double t = sim3d_.time();
      const auto& g = sim3d_.grid();
      const double zSlice = sbZSlice_->value();
      int k = static_cast<int>(std::round((zSlice - g.z0) / g.dz));
      k = std::clamp(k, 0, g.Nz - 1);

      static double nextExportT = 0.0;
      static int frameId = 0;
      if (t + 1e-12 >= nextExportT) {
        const QString dir = framesDir();
        QDir().mkpath(dir);

        std::vector<float> grid2d;
        float mx = 0.0f;
        sim3d_.extractSliceXY(k, grid2d, mx);

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

        const QString path = QDir(dir).filePath(QString("frame_%1.csv").arg(frameId, 4, 10, QLatin1Char('0')));
        QString err;
        if (!ExporterCsv::writeGridFrame(path, meta, grid2d, err)) {
          appendLog("[CSV] write failed: " + err);
        } else {
          appendLog("[CSV] wrote: " + path);
        }

        frameId++;
        nextExportT += sbExportInterval_->value();
      }
    }
    return;
  }
}

void MainWindow::onExternalLog(const QString& s)
{
  appendLog(s);
}

void MainWindow::onExternalFrame(const CsvFrameReader::Frame& f)
{
  lastFrame_ = f;
  hasLastFrame_ = true;
  renderFieldExternal(f);
}

void MainWindow::onExternalFinished(bool ok, const QString& msg)
{
  appendLog(QString("[ExternalFinished] ok=%1 %2").arg(ok).arg(msg));
}
