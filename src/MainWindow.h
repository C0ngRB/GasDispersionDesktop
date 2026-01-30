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

private:
  enum class BackendMode { Internal, External };

  // UI
  QLabel *view_{nullptr};
  QLabel *status_{nullptr};
  QComboBox *cbBackend_{nullptr};
  QLineEdit *leRunner_{nullptr};

  QDoubleSpinBox *sbWindSpeed_{nullptr}, *sbWindDir_{nullptr};
  QDoubleSpinBox *sbLx_{nullptr}, *sbLy_{nullptr};
  QSpinBox *sbNx_{nullptr}, *sbNy_{nullptr};
  QDoubleSpinBox *sbTotalTime_{nullptr}, *sbDt_{nullptr};
  QDoubleSpinBox *sbD_{nullptr}, *sbDecay_{nullptr};
  QDoubleSpinBox *sbSrcX_{nullptr}, *sbSrcY_{nullptr};
  QDoubleSpinBox *sbLeak_{nullptr}, *sbH_{nullptr};
  QCheckBox *cbAutoClampDt_{nullptr};

  QPushButton *btnRun_{nullptr};
  QPushButton *btnPause_{nullptr};
  QPushButton *btnReset_{nullptr};

  QPlainTextEdit *log_{nullptr};

  // Sim
  Simulator *sim_{nullptr};
  QTimer *timer_{nullptr};
  bool running_{false};
  BackendMode mode_{BackendMode::Internal};

  // External backend
  ExternalCfdBackend *ext_{nullptr};
  CsvFrameReader::Frame lastFrame_;
  bool hasLastFrame_{false};

  SimParams readParams() const;
  void rebuildSimulator();
  void renderFieldInternal();
  void renderFieldExternal(const CsvFrameReader::Frame& f);
  void appendLog(const QString &s);
};
