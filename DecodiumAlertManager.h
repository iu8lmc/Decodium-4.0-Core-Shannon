#pragma once
#include <QObject>
#include <QSoundEffect>
#include <QUrl>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

class DecodiumAlertManager : public QObject {
    Q_OBJECT
public:
    explicit DecodiumAlertManager(QObject* parent = nullptr);

    void setEnabled(bool v) { m_enabled = v; }
    bool isEnabled() const { return m_enabled; }

    // Suona un alert. type: "CQ", "MyCall", "DX", "73", "NewDXCC", "Error"
    void playAlert(const QString& type);

    // Imposta volume 0.0-1.0
    void setVolume(float v);

private:
    QSoundEffect* soundForType(const QString& type);
    QString findSoundFile(const QString& name) const;

    QSoundEffect* m_cqSound     {nullptr};
    QSoundEffect* m_myCallSound {nullptr};
    QSoundEffect* m_dxSound     {nullptr};
    QSoundEffect* m_sound73     {nullptr};
    QSoundEffect* m_newDxccSound{nullptr};
    QSoundEffect* m_errorSound  {nullptr};
    float         m_volume      {1.0f};
    bool          m_enabled     {true};
};
