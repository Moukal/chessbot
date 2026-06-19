#pragma once
#include "../infrastructure/Configuration.hpp"
#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <memory>

class SettingsWindow : public QDialog {
    Q_OBJECT
public:
    explicit SettingsWindow(Configuration& config, QWidget* parent = nullptr);

signals:
    void settingsChanged();

private slots:
    void onApply();
    void onBrowseEngine();
    void onBrowseBook();

private:
    void buildUi();
    void loadFromConfig();
    void saveToConfig();

    Configuration& m_config;

    QLineEdit* m_enginePath  = nullptr;
    QSpinBox*  m_threads     = nullptr;
    QSpinBox*  m_hashMb      = nullptr;
    QSpinBox*  m_skillLevel  = nullptr;
    QSpinBox*  m_moveTimeMs  = nullptr;
    QSpinBox*  m_multiPV     = nullptr;
    QCheckBox* m_playAsWhite = nullptr;
    QCheckBox* m_autoPlay    = nullptr;
    QSpinBox*  m_delayMin    = nullptr;
    QSpinBox*  m_delayMax    = nullptr;
    QLineEdit* m_bookPath    = nullptr;
};
