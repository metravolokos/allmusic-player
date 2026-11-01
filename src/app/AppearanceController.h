#pragma once

#include <QObject>
#include <QPalette>
#include <QString>

class AppearanceController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString themeMode READ themeMode NOTIFY themeModeChanged)

public:
    explicit AppearanceController(QObject *parent = nullptr);

    QString themeMode() const { return m_themeMode; }
    Q_INVOKABLE void setThemeMode(const QString &mode);
    void setPreviewThemeMode(const QString &mode);

    static void prepareFusionStyle();

signals:
    void themeModeChanged();

private:
    void applyColorScheme();

    QString m_themeMode = QStringLiteral("system");
    QPalette m_systemPalette;
};
