#pragma once
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QLabel>
#include <QTimer>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QPlainTextEdit>

#include "Simulator.h"

class MainWindow : public QMainWindow
{
  Q_OBJECT
public:
  explicit MainWindow(QWidget *parent = nullptr);

private slots:
  void onRunClicked();
  void onPauseClicked();
  void onResetClicked();
  void onTick();

private:
  // UI
  QLabel *view_{nullptr};
  QLabel *status_{nullptr};

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

  SimParams readParams() const;
  void rebuildSimulator();
  void renderField();
  void appendLog(const QString &s);
};
