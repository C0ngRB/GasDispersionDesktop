#include "MainWindow.h"
#include "ColorMap.h"

#include <QtWidgets/QWidget>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QSplitter>
#include <QDir>
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
  setWindowTitle("GasDispersionDesktop - Internal / External CFD");
  resize(1200, 700);

  auto *central = new QWidget(this);
  setCentralWidget(central);

  auto *splitter = new QSplitter(Qt::Horizontal, central);

  // ---- Left panel: controls ----
  auto *left = new QWidget(splitter);
  auto *leftLayout = new QVBoxLayout(left);

  // Backend selection
  auto *gbBackend = new QGroupBox("Backend", left);
  auto *backendForm = new QFormLayout(gbBackend);
  cbBackend_ = new QComboBox(gbBackend);
  cbBackend_->addItem("Internal (built-in)");
  cbBackend_->addItem("External CFD (runner.bat)");
  leRunner_ = new QLineEdit(gbBackend);
  leRunner_->setPlaceholderText("e.g. tools\\run_mock_cfd.bat");
  leRunner_->setText("tools\\run_mock_cfd.bat");
  backendForm->addRow("Mode", cbBackend_);
  backendForm->addRow("Runner", leRunner_);

  auto *gbWind = new QGroupBox("Wind", left);
  auto *windForm = new QFormLayout(gbWind);
  sbWindSpeed_ = makeDsb(0.0, 50.0, 0.1, 2.0, 3);
  sbWindDir_   = makeDsb(-180.0, 180.0, 1.0, 0.0, 1);
  windForm->addRow("Speed (m/s)", sbWindSpeed_);
  windForm->addRow("Direction (deg, 0=+x)", sbWindDir_);

  auto *gbDomain = new QGroupBox("Domain & Grid", left);
  auto *domainForm = new QFormLayout(gbDomain);
  sbLx_ = makeDsb(10.0, 5000.0, 10.0, 200.0, 1);
  sbLy_ = makeDsb(10.0, 5000.0, 10.0, 100.0, 1);
  sbNx_ = makeSsb(50, 2000, 50, 300);
  sbNy_ = makeSsb(50, 2000, 50, 150);
  domainForm->addRow("Lx (m)", sbLx_);
  domainForm->addRow("Ly (m)", sbLy_);
  domainForm->addRow("Nx", sbNx_);
  domainForm->addRow("Ny", sbNy_);

  auto *gbTime = new QGroupBox("Time", left);
  auto *timeForm = new QFormLayout(gbTime);
  sbTotalTime_ = makeDsb(1.0, 36000.0, 10.0, 60.0, 1);
  sbDt_        = makeDsb(1e-4, 10.0, 0.01, 0.05, 4);
  cbAutoClampDt_ = new QCheckBox("Auto clamp dt (CFL/diffusion stable)", gbTime);
  cbAutoClampDt_->setChecked(true);
  timeForm->addRow("Total (s)", sbTotalTime_);
  timeForm->addRow("dt (s)", sbDt_);
  timeForm->addRow(cbAutoClampDt_);

  auto *gbPhys = new QGroupBox("Dispersion", left);
  auto *physForm = new QFormLayout(gbPhys);
  sbD_     = makeDsb(0.0, 1e5, 0.1, 1.0, 4);
  sbDecay_ = makeDsb(0.0, 10.0, 0.001, 0.0, 4);
  physForm->addRow("D (m^2/s)", sbD_);
  physForm->addRow("decay k (1/s)", sbDecay_);

  auto *gbSrc = new QGroupBox("Point Source", left);
  auto *srcForm = new QFormLayout(gbSrc);
  sbSrcX_ = makeDsb(0.0, 1e6, 1.0, 20.0, 2);
  sbSrcY_ = makeDsb(0.0, 1e6, 1.0, 50.0, 2);
  sbLeak_ = makeDsb(0.0, 1e9, 0.1, 1.0, 6);
  sbH_    = makeDsb(0.01, 1000.0, 0.1, 1.0, 3);
  srcForm->addRow("x (m)", sbSrcX_);
  srcForm->addRow("y (m)", sbSrcY_);
  srcForm->addRow("Leak Q", sbLeak_);
  srcForm->addRow("Effective height H (m)", sbH_);

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
  leftLayout->addWidget(gbWind);
  leftLayout->addWidget(gbDomain);
  leftLayout->addWidget(gbTime);
  leftLayout->addWidget(gbPhys);
  leftLayout->addWidget(gbSrc);
  leftLayout->addWidget(btnRow);
  leftLayout->addWidget(log_, 1);

  // ---- Right panel: visualization ----
  auto *right = new QWidget(splitter);
  auto *rightLayout = new QVBoxLayout(right);
  view_ = new QLabel(right);
  view_->setMinimumSize(800, 500);
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

  // external backend
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

void MainWindow::appendLog(const QString &s)
{
  log_->appendPlainText(s);
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

void MainWindow::onRunClicked()
{
  mode_ = (cbBackend_->currentIndex() == 0) ? BackendMode::Internal : BackendMode::External;
  if (mode_ == BackendMode::Internal) {
    rebuildSimulator();
    running_ = true;
    timer_->start();
    appendLog("[Run] INTERNAL start");
    return;
  }
  // External CFD
  hasLastFrame_ = false;
  running_ = true;
  timer_->start();
  appendLog("[Run] EXTERNAL start");
  ExternalCfdBackend::Config cfg;
  cfg.runnerPath = leRunner_->text().trimmed();
  cfg.workRootDir = "work";
  cfg.pollIntervalMs = 100;
  ext_->start(readParams(), cfg);
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
  rebuildSimulator();
  renderFieldInternal();
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
  // External: 如果新帧没到，就维持最后一帧；这里主要用于保持 UI 刷新
  if (hasLastFrame_) renderFieldExternal(lastFrame_);
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
  // 让 UI 停在最后一帧
}
