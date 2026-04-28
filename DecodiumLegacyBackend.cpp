#include "DecodiumLegacyBackend.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QPalette>
#include <QProcessEnvironment>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <exception>

#include "MultiSettings.hpp"
#include "widgets/mainwindow.h"

namespace
{
QString embeddedLegacyConfigPath()
{
    QString configRoot = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (configRoot.isEmpty()) {
        configRoot = QDir::homePath();
    }

    QDir configDir(configRoot);
    configDir.mkpath(QStringLiteral("."));
    return configDir.absoluteFilePath(QStringLiteral("decodium4.ini"));
}

QString standaloneLegacyConfigPath()
{
    QString configRoot = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (configRoot.isEmpty()) {
        configRoot = QDir::homePath();
    }

    QDir configDir(configRoot);
    configDir.mkpath(QStringLiteral("."));
    return configDir.absoluteFilePath(QStringLiteral("ft2.ini"));
}

void copyFileIfMissing(QString const& targetPath, QString const& sourcePath)
{
    if (QFileInfo::exists(targetPath)) {
        return;
    }

    QFileInfo const sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        return;
    }

    QDir targetDir(QFileInfo(targetPath).absolutePath());
    targetDir.mkpath(QStringLiteral("."));
    QFile::copy(sourceInfo.absoluteFilePath(), targetPath);
}

void setSettingIfMissing(QSettings& settings, QString const& key, QVariant const& value)
{
    if (!settings.contains(key)) {
        settings.setValue(key, value);
    }
}

void mergeMissingSettings(QSettings& target, QString const& sourcePath)
{
    QFileInfo const sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        return;
    }

    QSettings source(sourceInfo.absoluteFilePath(), QSettings::IniFormat);
    for (QString const& key : source.allKeys()) {
        if (!target.contains(key)) {
            target.setValue(key, source.value(key));
        }
    }
}

void seedEmbeddedLegacyConfigDefaults(QSettings& settings)
{
    setSettingIfMissing(settings, QStringLiteral("General/AudioInputChannel"), QStringLiteral("Mono"));
    setSettingIfMissing(settings, QStringLiteral("General/AudioOutputChannel"), QStringLiteral("Mono"));
    setSettingIfMissing(settings, QStringLiteral("General/NDepth"), 51);
    setSettingIfMissing(settings, QStringLiteral("General/UILanguage"), QStringLiteral("en"));

    setSettingIfMissing(settings, QStringLiteral("Common/Mode"), QStringLiteral("FT8"));
    setSettingIfMissing(settings, QStringLiteral("Common/NDepth"), 51);
    setSettingIfMissing(settings, QStringLiteral("Common/MultithreadedFT8decoder"), false);
    setSettingIfMissing(settings, QStringLiteral("Common/NFT8Cycles"), 3);
    setSettingIfMissing(settings, QStringLiteral("Common/NFT8QSORXfreqSensitivity"), 3);
    setSettingIfMissing(settings, QStringLiteral("Common/FT8threads"), 0);
    setSettingIfMissing(settings, QStringLiteral("Common/FT8Sensitivity"), 3);
    setSettingIfMissing(settings, QStringLiteral("Common/FT8DecoderStart"), 3);
    setSettingIfMissing(settings, QStringLiteral("Common/FT8WideDXCallSearch"), true);
    setSettingIfMissing(settings, QStringLiteral("Common/FT8AP"), false);
    setSettingIfMissing(settings, QStringLiteral("Common/CQonly"), false);
    setSettingIfMissing(settings, QStringLiteral("Common/RxAll"), false);
    setSettingIfMissing(settings, QStringLiteral("Common/HideFT8Dupes"), true);
    setSettingIfMissing(settings, QStringLiteral("Common/DTtol"), 3);
    setSettingIfMissing(settings, QStringLiteral("Common/MinSync"), 0);
    setSettingIfMissing(settings, QStringLiteral("Common/actionDontSplitALLTXT"), true);
    setSettingIfMissing(settings, QStringLiteral("Common/disableWritingOfAllTxt"), false);
    setSettingIfMissing(settings, QStringLiteral("Common/splitAllTxtYearly"), false);
    setSettingIfMissing(settings, QStringLiteral("Common/splitAllTxtMonthly"), false);

    setSettingIfMissing(settings, QStringLiteral("Configuration/Decode52"), false);
    setSettingIfMissing(settings, QStringLiteral("Configuration/SingleDecode"), false);
}

void bootstrapEmbeddedLegacyConfig()
{
    QString const targetPath = embeddedLegacyConfigPath();
    QString const legacyPath = standaloneLegacyConfigPath();

    copyFileIfMissing(targetPath, legacyPath);

    QSettings target(targetPath, QSettings::IniFormat);
    mergeMissingSettings(target, legacyPath);
    seedEmbeddedLegacyConfigDefaults(target);
    target.sync();
}

QString embeddedLegacyPrivateDataDirPath(QString const& appLocalDataPath)
{
    if (!appLocalDataPath.isEmpty()) {
        return QDir(appLocalDataPath).absoluteFilePath(QStringLiteral("embedded-ft2"));
    }

    QString root = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (root.isEmpty()) {
        root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    }
    return QDir(root).absoluteFilePath(QStringLiteral("Decodium/embedded-ft2"));
}

QStringList embeddedLegacySourceDirs()
{
    QStringList dirs;
    QString const genericRoot = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QString const appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString const appLocal = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QString const appDir = QCoreApplication::applicationDirPath();
    QString const currentDir = QDir::currentPath();

    if (!appData.isEmpty()) {
        dirs << appData;
    }
    if (!appLocal.isEmpty() && appLocal != appData) {
        dirs << appLocal;
    }
    if (!genericRoot.isEmpty()) {
        dirs << QDir(genericRoot).absoluteFilePath(QStringLiteral("ft2"));
        dirs << QDir(genericRoot).absoluteFilePath(QStringLiteral("IU8LMC/ft2"));
        dirs << QDir(genericRoot).absoluteFilePath(QStringLiteral("IU8LMC/Decodium"));
    }
    if (!appDir.isEmpty()) {
        dirs << appDir;
        dirs << QDir(appDir).absoluteFilePath(QStringLiteral("../Resources"));
        dirs << QDir(appDir).absoluteFilePath(QStringLiteral("../Resources/wsjtx"));
        dirs << QDir(appDir).absoluteFilePath(QStringLiteral("../Resources/share/wsjtx"));
        dirs << QDir(appDir).absoluteFilePath(QStringLiteral("../share/Decodium"));
        dirs << QDir(appDir).absoluteFilePath(QStringLiteral("../share/wsjtx"));
    }
    if (!currentDir.isEmpty()) {
        dirs << currentDir;
    }
#ifdef CMAKE_SOURCE_DIR
    dirs << QDir(QStringLiteral(CMAKE_SOURCE_DIR)).absolutePath();
#endif
    dirs.removeDuplicates();
    return dirs;
}

void copyIfMissing(QString const& targetDirPath, QString const& fileName)
{
    QDir targetDir(targetDirPath);
    targetDir.mkpath(QStringLiteral("."));
    QString const targetPath = targetDir.absoluteFilePath(fileName);
    if (QFileInfo::exists(targetPath)) {
        return;
    }

    for (QString const& sourceDirPath : embeddedLegacySourceDirs()) {
        QFileInfo const sourceInfo(QDir(sourceDirPath).absoluteFilePath(fileName));
        if (!sourceInfo.exists() || !sourceInfo.isFile()) {
            continue;
        }
        QFile::copy(sourceInfo.absoluteFilePath(), targetPath);
        if (QFileInfo::exists(targetPath)) {
            return;
        }
    }
}

void bootstrapEmbeddedLegacyDataDir(QString const& targetDir)
{
    QDir(targetDir).mkpath(QStringLiteral("."));

    copyIfMissing(targetDir, QStringLiteral("cty.dat"));
    copyIfMissing(targetDir, QStringLiteral("CALL3.TXT"));
    copyIfMissing(targetDir, QStringLiteral("refspec.dat"));
    copyIfMissing(targetDir, QStringLiteral("hashtable.txt"));
    copyIfMissing(targetDir, QStringLiteral("azel.dat"));
    copyIfMissing(targetDir, QStringLiteral("lotw.adi"));
    copyIfMissing(targetDir, QStringLiteral("jt9_wisdom.dat"));
    copyIfMissing(targetDir, QStringLiteral("wsjtx_wisdom.dat"));
    copyIfMissing(targetDir, QStringLiteral("wspr_wisdom.dat"));
    copyIfMissing(targetDir, QStringLiteral("decodium_wisdom.dat"));
    copyIfMissing(targetDir, QStringLiteral("wsjtx_log.adi"));
    copyIfMissing(targetDir, QStringLiteral("wsjtx.log"));
}

QString embeddedLegacyWidgetStyleSheet()
{
    return QStringLiteral(R"(
QDialog, QMessageBox, QMenu, QWidget[class="QMenu"], QComboBox QAbstractItemView, QListView, QTreeView, QTableView {
    background-color: #111827;
    color: #E8F4FD;
}

QWidget {
    background-color: #111827;
    color: #E8F4FD;
    selection-background-color: #14384A;
    selection-color: #E8F4FD;
}

QFrame, QAbstractScrollArea, QScrollArea, QStackedWidget, QTabWidget QWidget {
    background-color: #111827;
    color: #E8F4FD;
}

QDialog, QMessageBox {
    border: 1px solid #294766;
    border-radius: 12px;
}

QLabel {
    color: #E8F4FD;
    background: transparent;
}

QGroupBox {
    color: #00D4FF;
    font-weight: 600;
    border: 1px solid #294766;
    border-radius: 10px;
    margin-top: 14px;
    padding-top: 14px;
    background-color: #0F1726;
}

QGroupBox::title {
    subcontrol-origin: margin;
    left: 12px;
    padding: 0 6px;
    color: #00D4FF;
}

QTabWidget::pane {
    border: 1px solid #294766;
    border-radius: 12px;
    top: -1px;
    background: #111827;
}

QTabBar::tab {
    background: #0E1625;
    color: #89B4D0;
    border: 1px solid #294766;
    border-bottom: none;
    border-top-left-radius: 10px;
    border-top-right-radius: 10px;
    padding: 8px 18px;
    margin-right: 4px;
    min-height: 30px;
}

QTabBar::tab:selected {
    background: #14384A;
    color: #00D4FF;
}

QTabBar::tab:hover:!selected {
    background: #162235;
    color: #E8F4FD;
}

QPushButton {
    background-color: #162235;
    color: #E8F4FD;
    border: 1px solid #2F5C85;
    border-radius: 8px;
    padding: 7px 14px;
    min-height: 32px;
}

QPushButton:hover {
    background-color: #1D3047;
    border-color: #00D4FF;
}

QPushButton:pressed {
    background-color: #14384A;
}

QPushButton:default {
    border-color: #00D4FF;
}

QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QTextEdit, QPlainTextEdit {
    background-color: #0A0F1A;
    color: #E8F4FD;
    border: 1px solid #294766;
    border-radius: 6px;
    padding: 4px 8px;
}

QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus, QTextEdit:focus, QPlainTextEdit:focus {
    border-color: #00D4FF;
}

QComboBox::drop-down {
    border: none;
    width: 26px;
}

QComboBox::down-arrow {
    width: 0px;
    height: 0px;
    border-left: 6px solid transparent;
    border-right: 6px solid transparent;
    border-top: 8px solid #00D4FF;
    margin-right: 8px;
}

QComboBox QAbstractItemView, QListView, QTreeView, QTableView {
    border: 1px solid #00D4FF;
    outline: 0;
    gridline-color: #294766;
    alternate-background-color: #0F1726;
}

QHeaderView::section {
    background-color: #162235;
    color: #89B4D0;
    border: 1px solid #294766;
    padding: 6px;
}

QMenu {
    border: 1px solid #294766;
    border-radius: 10px;
    padding: 6px;
}

QMenu::item {
    padding: 8px 24px 8px 14px;
    border-radius: 6px;
    margin: 2px 4px;
}

QMenu::item:selected {
    background: #14384A;
    color: #00D4FF;
}

QMenu::separator {
    height: 1px;
    background: #294766;
    margin: 6px 10px;
}

QCheckBox, QRadioButton {
    color: #E8F4FD;
    spacing: 8px;
}

QCheckBox::indicator, QRadioButton::indicator {
    width: 16px;
    height: 16px;
    background: #0A0F1A;
    border: 1px solid #2F5C85;
}

QCheckBox::indicator {
    border-radius: 4px;
}

QCheckBox::indicator:checked {
    background: #00D4FF;
    border-color: #00D4FF;
}

QRadioButton::indicator {
    border-radius: 8px;
}

QRadioButton::indicator:checked {
    background: #00D4FF;
    border: 4px solid #0A0F1A;
}

QScrollBar:vertical {
    background: #0F1726;
    width: 12px;
    margin: 0;
}

QScrollBar::handle:vertical {
    background: #294766;
    min-height: 28px;
    border-radius: 6px;
}

QScrollBar::handle:vertical:hover {
    background: #00D4FF;
}

QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical,
QScrollBar:horizontal, QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal,
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
    background: transparent;
    height: 0;
    width: 0;
}

QToolTip {
    background-color: #0A0F1A;
    color: #E8F4FD;
    border: 1px solid #00D4FF;
    padding: 6px 8px;
}
)");
}

QPalette embeddedLegacyWidgetPalette(QPalette base)
{
    QColor const windowColor {QStringLiteral("#111827")};
    QColor const midColor {QStringLiteral("#162235")};
    QColor const baseColor {QStringLiteral("#0A0F1A")};
    QColor const altBaseColor {QStringLiteral("#0F1726")};
    QColor const textColor {QStringLiteral("#E8F4FD")};
    QColor const secondaryText {QStringLiteral("#89B4D0")};
    QColor const accent {QStringLiteral("#00D4FF")};

    base.setColor(QPalette::Window, windowColor);
    base.setColor(QPalette::WindowText, textColor);
    base.setColor(QPalette::Base, baseColor);
    base.setColor(QPalette::AlternateBase, altBaseColor);
    base.setColor(QPalette::Text, textColor);
    base.setColor(QPalette::Button, midColor);
    base.setColor(QPalette::ButtonText, textColor);
    base.setColor(QPalette::BrightText, accent);
    base.setColor(QPalette::Highlight, QColor(QStringLiteral("#14384A")));
    base.setColor(QPalette::HighlightedText, textColor);
    base.setColor(QPalette::ToolTipBase, baseColor);
    base.setColor(QPalette::ToolTipText, textColor);
    base.setColor(QPalette::PlaceholderText, secondaryText);
    base.setColor(QPalette::Disabled, QPalette::WindowText, secondaryText);
    base.setColor(QPalette::Disabled, QPalette::Text, secondaryText);
    base.setColor(QPalette::Disabled, QPalette::ButtonText, secondaryText);
    return base;
}
}

DecodiumLegacyBackend::DecodiumLegacyBackend(QObject* parent)
    : QObject(parent)
{
    m_backendStartupMs = QDateTime::currentMSecsSinceEpoch();
    m_app = qobject_cast<QApplication*>(QCoreApplication::instance());
    if (!m_app) {
        m_failureReason = QStringLiteral("QApplication is required for the legacy backend");
        return;
    }

    m_originalApplicationName = m_app->applicationName();
    m_originalOrganizationName = m_app->organizationName();
    m_originalOrganizationDomain = m_app->organizationDomain();
    m_originalApplicationVersion = m_app->applicationVersion();
    m_originalQuitOnLastWindowClosed = m_app->quitOnLastWindowClosed();
    m_originalStyleSheet = m_app->styleSheet();
    m_originalPalette = m_app->palette();
    QString const originalAppLocalDataPath =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QString const embeddedLegacyDataDir =
        embeddedLegacyPrivateDataDirPath(originalAppLocalDataPath);
    auto const embeddedRig = m_app->property("decodiumEmbeddedLegacyRigControlEnabled");
    if (embeddedRig.isValid()) {
        m_rigControlEnabled = embeddedRig.toBool();
    }

    m_app->setQuitOnLastWindowClosed(false);
    m_app->setOrganizationName(QString {});
    m_app->setOrganizationDomain(QString {});
    m_app->setApplicationName(QStringLiteral("decodium4"));
    if (m_app->applicationVersion().isEmpty()) {
        m_app->setApplicationVersion(QStringLiteral("4.0.0"));
    }
    m_app->setProperty("decodiumEmbeddedLegacyShell", true);
    m_app->setProperty("decodiumEmbeddedLegacyDataDir", embeddedLegacyDataDir);
    m_app->setProperty("decodiumEmbeddedLegacyRigControlEnabled", m_rigControlEnabled);
    bootstrapEmbeddedLegacyDataDir(embeddedLegacyDataDir);
    bootstrapEmbeddedLegacyConfig();

    try {
        m_multiSettings = std::make_unique<MultiSettings>();

        const bool disableInputResampling =
            m_multiSettings->settings()->value(QStringLiteral("Audio/DisableInputResampling"), false).toBool();
        const unsigned downSampleFactor = disableInputResampling ? 1u : 4u;

        QDir tempDir {QStandardPaths::writableLocation(QStandardPaths::TempLocation)};
        QDir::setCurrent(qApp->applicationDirPath());

        m_mainWindow = new MainWindow(tempDir, false, m_multiSettings.get(),
                                      downSampleFactor, nullptr,
                                      QProcessEnvironment::systemEnvironment());
        m_mainWindow->setAttribute(Qt::WA_DontShowOnScreen, true);
        m_mainWindow->legacySetEmbeddedMode(true);
        m_mainWindow->legacySetRigControlEnabled(m_rigControlEnabled);
        m_mainWindow->hide();
        // In embedded mode the legacy window must start with monitor OFF.
        // The QML bridge will enable RX explicitly after the backend is stable.
        m_mainWindow->legacySetMonitoring(false);
        connect(m_mainWindow,
                SIGNAL(legacyWaterfallRowReady(QByteArray const&,int,int,int,int,QString const&)),
                this,
                SIGNAL(waterfallRowReady(QByteArray const&,int,int,int,int,QString const&)));
        connect(m_mainWindow,
                SIGNAL(legacyAudioSamplesReady(QByteArray const&)),
                this,
                SIGNAL(audioSamplesReady(QByteArray const&)));
        connect(m_mainWindow,
                SIGNAL(legacyWarningRaised(QString const&,QString const&,QString const&)),
                this,
                SIGNAL(warningRaised(QString const&,QString const&,QString const&)));
        connect(m_mainWindow,
                SIGNAL(legacyRigErrorRaised(QString const&,QString const&,QString const&)),
                this,
                SIGNAL(rigErrorRaised(QString const&,QString const&,QString const&)));
        connect(m_mainWindow,
                SIGNAL(legacyPreferencesRequested()),
                this,
                SIGNAL(preferencesRequested()));
        connect(m_mainWindow,
                SIGNAL(legacyQuitRequested()),
                this,
                SIGNAL(quitRequested()));
        connect(m_mainWindow,
                SIGNAL(legacyPttRequested(bool)),
                this,
                SIGNAL(rigPttRequested(bool)));
        m_available = true;

        // Prevent legacy startup options from auto-starting monitor behind the
        // QML shell before the bridge explicitly takes ownership.
        QTimer::singleShot(2500, m_mainWindow, [this]() {
            if (m_mainWindow && !m_monitoringControlClaimed) {
                m_mainWindow->legacySetMonitoring(false);
            }
        });

        applyEmbeddedWidgetTheme();
    } catch (std::exception const& e) {
        m_app->setProperty("decodiumEmbeddedLegacyShell", false);
        m_app->setProperty("decodiumEmbeddedLegacyDataDir", QString {});
        m_app->setProperty("decodiumEmbeddedLegacyRigControlEnabled", QVariant {});
        m_failureReason = QString::fromLocal8Bit(e.what());
        m_available = false;
    } catch (...) {
        m_app->setProperty("decodiumEmbeddedLegacyShell", false);
        m_app->setProperty("decodiumEmbeddedLegacyDataDir", QString {});
        m_app->setProperty("decodiumEmbeddedLegacyRigControlEnabled", QVariant {});
        m_failureReason = QStringLiteral("Unknown exception while creating legacy backend");
        m_available = false;
    }

    restoreApplicationIdentity();
}

DecodiumLegacyBackend::~DecodiumLegacyBackend()
{
    if (m_mainWindow) {
        m_mainWindow->legacyShutdownForEmbedding();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 200);
    }
    restoreEmbeddedWidgetTheme();
    delete m_mainWindow;
    m_mainWindow = nullptr;
    if (m_app) {
        m_app->setProperty("decodiumEmbeddedLegacyShell", false);
        m_app->setProperty("decodiumEmbeddedLegacyDataDir", QString {});
        m_app->setProperty("decodiumEmbeddedLegacyRigControlEnabled", QVariant {});
    }
}

QString DecodiumLegacyBackend::callsign() const
{
    return m_mainWindow ? m_mainWindow->legacyCallsign() : QString {};
}

QString DecodiumLegacyBackend::grid() const
{
    return m_mainWindow ? m_mainWindow->legacyGrid() : QString {};
}

QString DecodiumLegacyBackend::mode() const
{
    return m_mainWindow ? m_mainWindow->legacyMode() : QString {};
}

QString DecodiumLegacyBackend::rigName() const
{
    return m_mainWindow ? m_mainWindow->legacyRigName() : QString {};
}

double DecodiumLegacyBackend::dialFrequency() const
{
    return m_mainWindow ? static_cast<double>(m_mainWindow->legacyDialFrequency()) : 0.0;
}

int DecodiumLegacyBackend::rxFrequency() const
{
    return m_mainWindow ? m_mainWindow->legacyRxFrequency() : 0;
}

int DecodiumLegacyBackend::txFrequency() const
{
    return m_mainWindow ? m_mainWindow->legacyTxFrequency() : 0;
}

QString DecodiumLegacyBackend::audioInputDeviceName() const
{
    return m_mainWindow ? m_mainWindow->legacyAudioInputDeviceName() : QString {};
}

QString DecodiumLegacyBackend::audioOutputDeviceName() const
{
    return m_mainWindow ? m_mainWindow->legacyAudioOutputDeviceName() : QString {};
}

int DecodiumLegacyBackend::audioInputChannel() const
{
    return m_mainWindow ? m_mainWindow->legacyAudioInputChannel() : 0;
}

int DecodiumLegacyBackend::audioOutputChannel() const
{
    return m_mainWindow ? m_mainWindow->legacyAudioOutputChannel() : 0;
}

int DecodiumLegacyBackend::rxInputLevel() const
{
    return m_mainWindow ? m_mainWindow->legacyRxInputLevel() : 50;
}

QString DecodiumLegacyBackend::waterfallPalette() const
{
    return m_mainWindow ? m_mainWindow->legacyWaterfallPalette() : QString {};
}

bool DecodiumLegacyBackend::alt12Enabled() const
{
    return m_mainWindow && m_mainWindow->legacyAlt12Enabled();
}

bool DecodiumLegacyBackend::txFirst() const
{
    return m_mainWindow && m_mainWindow->legacyTxFirst();
}

bool DecodiumLegacyBackend::monitoring() const
{
    return m_mainWindow && m_mainWindow->legacyMonitoring();
}

bool DecodiumLegacyBackend::transmitting() const
{
    return m_mainWindow && m_mainWindow->legacyTransmitting();
}

bool DecodiumLegacyBackend::tuning() const
{
    return m_mainWindow && m_mainWindow->legacyTuning();
}

bool DecodiumLegacyBackend::catConnected() const
{
    return m_mainWindow && m_mainWindow->legacyCatConnected();
}

double DecodiumLegacyBackend::signalLevel() const
{
    return m_mainWindow ? m_mainWindow->legacySignalLevel() : 0.0;
}

int DecodiumLegacyBackend::bandActivityRevision() const
{
    return m_mainWindow ? m_mainWindow->legacyBandActivityRevision() : -1;
}

QStringList DecodiumLegacyBackend::bandActivityLines() const
{
    return m_mainWindow ? m_mainWindow->legacyBandActivityLines() : QStringList {};
}

int DecodiumLegacyBackend::rxFrequencyRevision() const
{
    return m_mainWindow ? m_mainWindow->legacyRxFrequencyRevision() : -1;
}

QStringList DecodiumLegacyBackend::rxFrequencyLines() const
{
    return m_mainWindow ? m_mainWindow->legacyRxFrequencyLines() : QStringList {};
}

QString DecodiumLegacyBackend::txMessage(int index) const
{
    return m_mainWindow ? m_mainWindow->legacyTxMessage(index) : QString {};
}

int DecodiumLegacyBackend::currentTx() const
{
    return m_mainWindow ? m_mainWindow->legacyCurrentTx() : 1;
}

QString DecodiumLegacyBackend::adifLogPath() const
{
    return m_mainWindow ? m_mainWindow->legacyAdifLogPath() : QString {};
}

QString DecodiumLegacyBackend::allTxtPath() const
{
    return m_mainWindow ? m_mainWindow->legacyAllTxtPath() : QString {};
}

int DecodiumLegacyBackend::txOutputAttenuation() const
{
    return m_mainWindow ? m_mainWindow->legacyTxOutputAttenuation() : 0;
}

int DecodiumLegacyBackend::specialOperationActivity() const
{
    return m_mainWindow ? m_mainWindow->legacySpecialOperationActivity() : 0;
}

bool DecodiumLegacyBackend::superFoxEnabled() const
{
    return m_mainWindow && m_mainWindow->legacySuperFoxEnabled();
}

QStringList DecodiumLegacyBackend::foxCallerQueueLines() const
{
    return m_mainWindow ? m_mainWindow->legacyFoxCallerQueueLines() : QStringList {};
}

void DecodiumLegacyBackend::setMode(const QString& mode)
{
    if (m_mainWindow) {
        m_mainWindow->legacySetMode(mode);
    }
}

void DecodiumLegacyBackend::setDialFrequency(double frequencyHz)
{
    if (m_mainWindow && frequencyHz > 0.0) {
        m_mainWindow->legacySetDialFrequency(static_cast<MainWindow::Frequency>(qRound64(frequencyHz)));
    }
}

void DecodiumLegacyBackend::setMonitoring(bool enabled)
{
    if (m_mainWindow) {
        m_monitoringControlClaimed = true;
        m_startupMonitorRequested = enabled;

        constexpr qint64 startupMonitorGateMs {2500};
        qint64 const elapsedMs = QDateTime::currentMSecsSinceEpoch() - m_backendStartupMs;
        if (enabled && elapsedMs < startupMonitorGateMs) {
            if (!m_startupMonitorEnablePending) {
                m_startupMonitorEnablePending = true;
                int const remainingMs = int(startupMonitorGateMs - elapsedMs);
                QTimer::singleShot(remainingMs, this, [this]() {
                    m_startupMonitorEnablePending = false;
                    if (!m_mainWindow || !m_startupMonitorRequested) {
                        return;
                    }
                    m_mainWindow->legacySetMonitoring(true);
                });
            }
            return;
        }

        m_startupMonitorEnablePending = false;
        m_mainWindow->legacySetMonitoring(enabled);
    }
}

void DecodiumLegacyBackend::setAutoSeq(bool enabled)
{
    if (m_mainWindow) {
        m_mainWindow->legacySetAutoSeq(enabled);
    }
}

void DecodiumLegacyBackend::setTxEnabled(bool enabled)
{
    if (m_mainWindow) {
        if (enabled) {
            m_monitoringControlClaimed = true;
        }
        m_mainWindow->legacySetTxEnabled(enabled);
    }
}

void DecodiumLegacyBackend::setHoldTxFreq(bool enabled)
{
    if (m_mainWindow) {
        m_mainWindow->legacySetHoldTxFreq(enabled);
    }
}

bool DecodiumLegacyBackend::holdTxFreq() const
{
    if (m_mainWindow) {
        return m_mainWindow->legacyHoldTxFreq();
    }
    return false;
}

void DecodiumLegacyBackend::setAutoCq(bool enabled)
{
    if (m_mainWindow) {
        m_mainWindow->legacySetAutoCq(enabled);
    }
}

void DecodiumLegacyBackend::setDecodeDepthBits(int bits)
{
    if (m_mainWindow) {
        m_mainWindow->legacySetDecodeDepthBits(bits);
    }
}

void DecodiumLegacyBackend::setCqOnly(bool enabled)
{
    if (m_mainWindow) {
        m_mainWindow->legacySetCqOnly(enabled);
    }
}

void DecodiumLegacyBackend::setRxFrequency(int frequencyHz)
{
    if (m_mainWindow) {
        m_mainWindow->legacySetRxFrequency(frequencyHz);
    }
}

void DecodiumLegacyBackend::setTxFrequency(int frequencyHz)
{
    if (m_mainWindow) {
        m_mainWindow->legacySetTxFrequency(frequencyHz);
    }
}

void DecodiumLegacyBackend::setRigControlEnabled(bool enabled)
{
    m_rigControlEnabled = enabled;
    if (m_app) {
        m_app->setProperty("decodiumEmbeddedLegacyRigControlEnabled", enabled);
    }
    if (m_mainWindow) {
        m_mainWindow->legacySetRigControlEnabled(enabled);
    }
}

void DecodiumLegacyBackend::setRigPtt(bool enabled)
{
    if (m_mainWindow) {
        m_mainWindow->legacySetRigPtt(enabled);
    }
}

void DecodiumLegacyBackend::setAudioInputDeviceName(const QString& name)
{
    if (m_mainWindow) {
        m_mainWindow->legacySetAudioInputDeviceName(name);
    }
}

void DecodiumLegacyBackend::setAudioOutputDeviceName(const QString& name)
{
    if (m_mainWindow) {
        m_mainWindow->legacySetAudioOutputDeviceName(name);
    }
}

void DecodiumLegacyBackend::setAudioInputChannel(int channel)
{
    if (m_mainWindow) {
        m_mainWindow->legacySetAudioInputChannel(channel);
    }
}

void DecodiumLegacyBackend::setAudioOutputChannel(int channel)
{
    if (m_mainWindow) {
        m_mainWindow->legacySetAudioOutputChannel(channel);
    }
}

void DecodiumLegacyBackend::setRxInputLevel(int value)
{
    if (m_mainWindow) {
        m_mainWindow->legacySetRxInputLevel(value);
    }
}

void DecodiumLegacyBackend::setTxOutputAttenuation(int value)
{
    if (m_mainWindow) {
        m_mainWindow->legacySetTxOutputAttenuation(value);
    }
}

void DecodiumLegacyBackend::setDxCall(const QString& call)
{
    if (m_mainWindow) {
        m_mainWindow->legacySetDxCall(call);
    }
}

void DecodiumLegacyBackend::setDxGrid(const QString& grid)
{
    if (m_mainWindow) {
        m_mainWindow->legacySetDxGrid(grid);
    }
}

void DecodiumLegacyBackend::setTxMessage(int index, const QString& message)
{
    if (m_mainWindow) {
        m_mainWindow->legacySetTxMessage(index, message);
    }
}

void DecodiumLegacyBackend::selectTxMessage(int index)
{
    if (m_mainWindow) {
        m_mainWindow->legacySelectTxMessage(index);
    }
}

void DecodiumLegacyBackend::generateStandardMessages()
{
    if (m_mainWindow) {
        m_mainWindow->legacyGenerateStandardMessages();
    }
}

void DecodiumLegacyBackend::startTune(bool enabled)
{
    if (m_mainWindow) {
        if (enabled) {
            m_monitoringControlClaimed = true;
        }
        m_mainWindow->legacyStartTune(enabled);
    }
}

void DecodiumLegacyBackend::stopTransmission()
{
    if (m_mainWindow) {
        m_mainWindow->legacyStopTransmission();
    }
}

void DecodiumLegacyBackend::armCurrentTx()
{
    if (m_mainWindow) {
        m_monitoringControlClaimed = true;
        m_mainWindow->legacyArmCurrentTx();
    }
}

void DecodiumLegacyBackend::logQso()
{
    if (m_mainWindow) {
        m_mainWindow->legacyLogQso();
    }
}

void DecodiumLegacyBackend::setAutoSpotEnabled(bool enabled)
{
    if (m_mainWindow) {
        m_mainWindow->legacySetAutoSpotEnabled(enabled);
    }
}

void DecodiumLegacyBackend::setNextLogClusterSpotState(bool available, bool checked)
{
    if (m_mainWindow) {
        m_mainWindow->legacySetNextLogClusterSpotState(available, checked);
    }
}

void DecodiumLegacyBackend::setNextLogPromptAlreadyAccepted()
{
    if (m_mainWindow) {
        m_mainWindow->legacySetNextLogPromptAlreadyAccepted();
    }
}

void DecodiumLegacyBackend::clearBandActivity()
{
    if (m_mainWindow) {
        m_mainWindow->legacyClearBandActivity();
    }
}

void DecodiumLegacyBackend::clearRxFrequency()
{
    if (m_mainWindow) {
        m_mainWindow->legacyClearRxFrequency();
    }
}

void DecodiumLegacyBackend::setWaterfallPalette(const QString& palette)
{
    if (m_mainWindow) {
        m_mainWindow->legacySetWaterfallPalette(palette);
    }
}

void DecodiumLegacyBackend::openSettings(int tabIndex)
{
    if (m_mainWindow) {
        applyEmbeddedWidgetTheme();
        m_mainWindow->legacyOpenSettings(tabIndex);
    }
}

void DecodiumLegacyBackend::openTimeSyncSettings()
{
    if (m_mainWindow) {
        applyEmbeddedWidgetTheme();
        m_mainWindow->legacyOpenTimeSyncPanel();
    }
}

void DecodiumLegacyBackend::retryRigConnection()
{
    if (m_mainWindow) {
        m_mainWindow->legacyRetryRigConnection();
    }
}

void DecodiumLegacyBackend::setAlt12Enabled(bool enabled)
{
    if (m_mainWindow) {
        m_mainWindow->legacySetAlt12Enabled(enabled);
    }
}

void DecodiumLegacyBackend::setTxFirst(bool enabled)
{
    if (m_mainWindow) {
        m_mainWindow->legacySetTxFirst(enabled);
    }
}

void DecodiumLegacyBackend::setSpecialOperationActivity(int activity)
{
    if (m_mainWindow) {
        if (m_mainWindow->legacySpecialOperationActivity() == activity) {
            return;
        }
        m_mainWindow->legacySetSpecialOperationActivity(activity);
    }
}

void DecodiumLegacyBackend::setSuperFoxEnabled(bool enabled)
{
    if (m_mainWindow) {
        m_mainWindow->legacySetSuperFoxEnabled(enabled);
    }
}

void DecodiumLegacyBackend::applyEmbeddedWidgetTheme()
{
    if (!m_app) {
        return;
    }

    QString const combined = m_originalStyleSheet
        + QStringLiteral("\n/* Decodium embedded legacy widget theme */\n")
        + embeddedLegacyWidgetStyleSheet();
    m_app->setPalette(embeddedLegacyWidgetPalette(m_originalPalette));
    m_app->setStyleSheet(combined);
}

void DecodiumLegacyBackend::restoreEmbeddedWidgetTheme()
{
    if (!m_app) {
        return;
    }

    m_app->setPalette(m_originalPalette);
    m_app->setStyleSheet(m_originalStyleSheet);
}

void DecodiumLegacyBackend::restoreApplicationIdentity()
{
    if (!m_app) {
        return;
    }
    m_app->setOrganizationName(m_originalOrganizationName);
    m_app->setOrganizationDomain(m_originalOrganizationDomain);
    m_app->setApplicationName(m_originalApplicationName);
    m_app->setApplicationVersion(m_originalApplicationVersion);
    m_app->setQuitOnLastWindowClosed(m_originalQuitOnLastWindowClosed);
}
