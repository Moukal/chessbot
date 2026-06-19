#include "SettingsWindow.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QLabel>

SettingsWindow::SettingsWindow(Configuration& config, QWidget* parent)
    : QDialog(parent), m_config(config)
{
    setWindowTitle("ChessBot Settings");
    setMinimumWidth(420);
    buildUi();
    loadFromConfig();
}

void SettingsWindow::buildUi() {
    auto* root = new QVBoxLayout(this);

    // Engine group
    auto* engBox = new QGroupBox("Engine");
    auto* engForm = new QFormLayout(engBox);

    m_enginePath = new QLineEdit;
    auto* browseEngine = new QPushButton("...");
    browseEngine->setFixedWidth(30);
    connect(browseEngine, &QPushButton::clicked, this, &SettingsWindow::onBrowseEngine);
    auto* engineRow = new QHBoxLayout;
    engineRow->addWidget(m_enginePath);
    engineRow->addWidget(browseEngine);
    engForm->addRow("Engine path:", engineRow);

    m_threads = new QSpinBox; m_threads->setRange(1, 64);
    engForm->addRow("Threads:", m_threads);
    m_hashMb = new QSpinBox; m_hashMb->setRange(16, 8192); m_hashMb->setSuffix(" MB");
    engForm->addRow("Hash:", m_hashMb);
    m_skillLevel = new QSpinBox; m_skillLevel->setRange(0, 20);
    engForm->addRow("Skill level:", m_skillLevel);
    m_moveTimeMs = new QSpinBox; m_moveTimeMs->setRange(100, 30000); m_moveTimeMs->setSuffix(" ms");
    engForm->addRow("Move time:", m_moveTimeMs);
    m_multiPV = new QSpinBox; m_multiPV->setRange(1, 5);
    engForm->addRow("MultiPV:", m_multiPV);
    root->addWidget(engBox);

    // Play group
    auto* playBox = new QGroupBox("Play");
    auto* playForm = new QFormLayout(playBox);
    m_playAsWhite = new QCheckBox("Board from White side");
    playForm->addRow(m_playAsWhite);
    m_autoPlay = new QCheckBox("Enable auto-play");
    playForm->addRow(m_autoPlay);
    m_delayMin = new QSpinBox; m_delayMin->setRange(50, 5000); m_delayMin->setSuffix(" ms");
    playForm->addRow("Delay min:", m_delayMin);
    m_delayMax = new QSpinBox; m_delayMax->setRange(50, 10000); m_delayMax->setSuffix(" ms");
    playForm->addRow("Delay max:", m_delayMax);

    m_bookPath = new QLineEdit;
    auto* browseBook = new QPushButton("...");
    browseBook->setFixedWidth(30);
    connect(browseBook, &QPushButton::clicked, this, &SettingsWindow::onBrowseBook);
    auto* bookRow = new QHBoxLayout;
    bookRow->addWidget(m_bookPath);
    bookRow->addWidget(browseBook);
    playForm->addRow("Opening book:", bookRow);
    root->addWidget(playBox);

    // Buttons
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsWindow::onApply);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

void SettingsWindow::loadFromConfig() {
    m_enginePath->setText(QString::fromStdString(m_config.enginePath()));
    m_threads->setValue(m_config.engineThreads());
    m_hashMb->setValue(m_config.engineHashMb());
    m_skillLevel->setValue(m_config.engineSkillLevel());
    m_moveTimeMs->setValue(m_config.moveTimeMs());
    m_multiPV->setValue(m_config.multiPV());
    m_playAsWhite->setChecked(m_config.playAsWhite());
    m_autoPlay->setChecked(m_config.autoPlay());
    m_delayMin->setValue(m_config.mouseDelayMinMs());
    m_delayMax->setValue(m_config.mouseDelayMaxMs());
    m_bookPath->setText(QString::fromStdString(m_config.openingBookPath()));
}

void SettingsWindow::saveToConfig() {
    m_config.setEnginePath(m_enginePath->text().toStdString());
    m_config.setEngineThreads(m_threads->value());
    m_config.setEngineHashMb(m_hashMb->value());
    m_config.setEngineSkillLevel(m_skillLevel->value());
    m_config.setMoveTimeMs(m_moveTimeMs->value());
    m_config.setMultiPV(m_multiPV->value());
    m_config.setPlayAsWhite(m_playAsWhite->isChecked());
    m_config.setAutoPlay(m_autoPlay->isChecked());
    m_config.setMouseDelay(m_delayMin->value(), m_delayMax->value());
    m_config.setOpeningBookPath(m_bookPath->text().toStdString());
}

void SettingsWindow::onApply() {
    saveToConfig();
    m_config.save();
    emit settingsChanged();
    accept();
}

void SettingsWindow::onBrowseEngine() {
    QString path = QFileDialog::getOpenFileName(this, "Select engine binary",
        m_enginePath->text());
    if (!path.isEmpty()) m_enginePath->setText(path);
}

void SettingsWindow::onBrowseBook() {
    QString path = QFileDialog::getOpenFileName(this, "Select opening book",
        m_bookPath->text(), "Polyglot books (*.book);;All files (*)");
    if (!path.isEmpty()) m_bookPath->setText(path);
}
