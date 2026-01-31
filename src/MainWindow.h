#pragma once
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QLabel>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QComboBox>
#include <QTimer>

#include "TerrainDem.h"
#include "TerrainFlat.h"
#include "TerrainProcedural.h"
#include "Simulator3D.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onLoadDemClicked();
    void onTerrainModeChanged(int index);
    void onRunClicked();
    void onPauseClicked();
    void onResetClicked();
    void onTick();
    void onSetSrcZFromGroundClicked();

private:
    enum TerrainMode { Flat, Dem, Procedural };

    QLabel* view_{nullptr};
    QLabel* status_{nullptr};

    QComboBox* cbTerrainMode_{nullptr};
    QLineEdit* leDemTif_{nullptr};
    QPushButton* btnLoadDem_{nullptr};
    QLabel* demInfo_{nullptr};
    QSpinBox* sbDemStride_{nullptr};

    QDoubleSpinBox* sbDz_{nullptr};
    QDoubleSpinBox* sbZTopMargin_{nullptr};
    QSpinBox* sbNzMax_{nullptr};
    QDoubleSpinBox* sbZSlice_{nullptr};

    QDoubleSpinBox* sbWindSpeed_{nullptr};
    QDoubleSpinBox* sbWindDir_{nullptr};
    QDoubleSpinBox* sbK_{nullptr};
    QDoubleSpinBox* sbDecay_{nullptr};

    QDoubleSpinBox* sbTotalTime_{nullptr};
    QDoubleSpinBox* sbDt_{nullptr};
    QCheckBox* cbAutoClampDt_{nullptr};
    QCheckBox* cbExportCsv_{nullptr};
    QDoubleSpinBox* sbExportInterval_{nullptr};

    QDoubleSpinBox* sbSrcX_{nullptr};
    QDoubleSpinBox* sbSrcY_{nullptr};
    QDoubleSpinBox* sbSrcZ_{nullptr};
    QDoubleSpinBox* sbSrcRadius_{nullptr};
    QDoubleSpinBox* sbLeak_{nullptr};
    QDoubleSpinBox* sbAgl_{nullptr};
    QPushButton* btnSetSrcZFromGround_{nullptr};

    QPushButton* btnRun_{nullptr};
    QPushButton* btnPause_{nullptr};
    QPushButton* btnReset_{nullptr};
    QPlainTextEdit* log_{nullptr};

    TerrainDem dem_;
    TerrainFlat flatTerrain_;
    TerrainProcedural procTerrain_;
    bool hasDem_{false};

    Simulator3D sim_;
    bool simReady_{false};

    QTimer* timer_{nullptr};
    bool running_{false};

    double nextExportT_{0.0};
    int frameId_{0};

    std::vector<float> slice_;
    float sliceMax_{0.0f};

    void appendLog(const QString& s);
    void renderSlice();

    bool buildSimulation(QString& errOut);
    Simulator3D::Params readSimParams() const;
    ITerrain* currentTerrain();
    QString framesDir() const;
};
