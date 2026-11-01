#include "AppearanceController.h"

#include <QQuickStyle>
#include <QGuiApplication>
#include <QStyleHints>
#include <QSettings>
#include <QColor>
#include <Qt>

namespace {
constexpr auto kThemeSetting = "appearance/themeMode";

bool isSupportedMode(const QString &mode)
{
    return mode == QStringLiteral("system") || mode == QStringLiteral("light")
        || mode == QStringLiteral("dark");
}
}

AppearanceController::AppearanceController(QObject *parent)
    : QObject(parent)
{
    if (auto *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
        m_systemPalette = application->palette();
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("MetraVolokos"), QStringLiteral("AllMusicPlayer"));
    const QString savedMode = settings.value(QLatin1String(kThemeSetting), m_themeMode).toString();
    if (isSupportedMode(savedMode))
        m_themeMode = savedMode;
    applyColorScheme();
}

void AppearanceController::prepareFusionStyle()
{
    QQuickStyle::setStyle(QStringLiteral("Fusion"));
}

void AppearanceController::setThemeMode(const QString &mode)
{
    if (!isSupportedMode(mode) || m_themeMode == mode)
        return;

    m_themeMode = mode;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("MetraVolokos"), QStringLiteral("AllMusicPlayer"));
    settings.setValue(QLatin1String(kThemeSetting), m_themeMode);
    settings.sync();
    applyColorScheme();
    emit themeModeChanged();
}

void AppearanceController::applyColorScheme()
{
    const Qt::ColorScheme scheme = m_themeMode == QStringLiteral("dark") ? Qt::ColorScheme::Dark
        : m_themeMode == QStringLiteral("light") ? Qt::ColorScheme::Light
                                                  : Qt::ColorScheme::Unknown;
    auto *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
    if (!application)
        return;
    application->styleHints()->setColorScheme(scheme);
    if (m_themeMode == QStringLiteral("system")) {
        application->setPalette(m_systemPalette);
        return;
    }

    QPalette palette;
    const bool dark = m_themeMode == QStringLiteral("dark");
    palette.setColor(QPalette::Window, dark ? QColor("#1d1f22") : QColor("#f5f6f8"));
    palette.setColor(QPalette::WindowText, dark ? QColor("#e6e2dc") : QColor("#202124"));
    palette.setColor(QPalette::Base, dark ? QColor("#141517") : QColor("#ffffff"));
    palette.setColor(QPalette::AlternateBase, dark ? QColor("#24272b") : QColor("#f1f3f4"));
    palette.setColor(QPalette::Text, dark ? QColor("#e6e2dc") : QColor("#202124"));
    palette.setColor(QPalette::Button, dark ? QColor("#2a2d31") : QColor("#eceff1"));
    palette.setColor(QPalette::ButtonText, dark ? QColor("#e6e2dc") : QColor("#202124"));
    palette.setColor(QPalette::Highlight, dark ? QColor("#86b7d8") : QColor("#3a6f91"));
    palette.setColor(QPalette::HighlightedText, dark ? QColor("#10212e") : QColor("#ffffff"));
    palette.setColor(QPalette::BrightText, dark ? QColor("#ff8a80") : QColor("#b3261e"));
    application->setPalette(palette);
}
