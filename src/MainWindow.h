#pragma once
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QLabel>
#include <QTimer>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLineEdit>

#include "Simulator.h"
#include "ExternalCfdBackend.h"
#include "TerrainDem.h"
#include "TerrainFlat.h"
#include "TerrainProcedural.h"
#include "Simulator3D.h"

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget *parent = nullptr);

private slots:
  void onRunClicked();
  void onPauseClicked();
  void onResetClicked();
  void onTick();
  void onExternalLog(const QString& s);
  void onExternalFrame(const CsvFrameReader::Frame& f);
  void onExternalFinished(bool ok, const QString& msg);
  void onLoadDemClicked();
  void onSetSrcZFromGroundClicked();
  void onTerrainModeChanged(int index);

private:
  enum class BackendMode { Internal, External, Terrain3D };
  enum class TerrainMode { Flat, Dem, Procedural };

  QLabel *view_{nullptr};
  QLabel *status_{nullptr};
  QComboBox *cbBackend_{nullptr};
  QLineEdit *leRunner_{nullptr};

  QComboBox *cbTerrainMode_{nullptr};
  QLineEdit *leDemTif_{nullptr};
  QPushButton *btnLoadDem_{nullptr};
  QLabel *demInfo_{nullptr};
  QSpinBox *sbDemStride_{nullptr};

  QDoubleSpinBox *sbWindSpeed_{nullptr}, *sbWindDir_{nullptr};
  QDoubleSpinBox *sbLx_{nullptr}, *sbLy_{nullptr};
  QSpinBox *sbNx_{nullptr}, *sbNy_{nullptr};
  QDoubleSpinBox *sbTotalTime_{nullptr}, *sbDt_{nullptr};
  QDoubleSpinBox *sbD_{nullptr}, *sbDecay_{nullptr};
  QDoubleSpinBox *sbSrcX_{nullptr}, *sbSrcY_{nullptr}, *sbSrcZ_{nullptr};
  QDoubleSpinBox *sbLeak_{nullptr}, *sbH_{nullptr};
  QDoubleSpinBox *sbSrcRadius_{nullptr};
  QCheckBox *cbAutoClampDt_{nullptr};
  QCheckBox *cbExportCsv_{nullptr};
  QDoubleSpinBox *sbExportInterval_{nullptr};

  QPushButton *btnRun_{nullptr};
  QPushButton *btnPause_{nullptr};
  QPushButton *btnReset_{nullptr};
  QPushButton *btnSetSrcZFromGround_{nullptr};

  QPlainTextEdit *log_{nullptr};

  Simulator *sim_{nullptr};
  QTimer *timer_{nullptr};
  bool running_{false};
  BackendMode mode_{BackendMode::Internal};

  ExternalCfdBackend *ext_{nullptr};
  CsvFrameReader::Frame lastFrame_;
  bool hasLastFrame_{false};

  TerrainDem dem_;
  TerrainFlat flatTerrain_;
  TerrainProcedural procTerrain_;
  bool hasDem_{false};

  Simulator3D sim3d_;
  bool sim3dReady_{false};
  QDoubleSpinBox *sbDz_{nullptr};
  QDoubleSpinBox *sbZTopMargin_{nullptr};
  QSpinBox *sbNzMax_{nullptr};
  QDoubleSpinBox *sbZSlice_{nullptr};

  SimParams readParams() const;
  Simulator3D::Params readParams3D() const;
  void rebuildSimulator();
  void renderFieldInternal();
  void renderFieldExternal(const CsvFrameReader::Frame& f);
  void renderField3D();
  void appendLog(const QString &s);
  bool buildSimulation3D(QString& errOut);
  ITerrain* currentTerrain();
  QString framesDir() const;
};
