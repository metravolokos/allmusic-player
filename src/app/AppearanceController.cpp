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

void AppearanceController::setPreviewThemeMode(const QString &mode)
{
    if (!isSupportedMode(mode) || m_themeMode == mode)
        return;
    m_themeMode = mode;
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
    palette.setColor(QPalette::Window, dark ? QColor("#111815") : QColor("#f3f5f2"));
    palette.setColor(QPalette::WindowText, dark ? QColor("#eef3ef") : QColor("#202a25"));
    palette.setColor(QPalette::Base, dark ? QColor("#18211d") : QColor("#ffffff"));
    palette.setColor(QPalette::AlternateBase, dark ? QColor("#202b25") : QColor("#edf1ed"));
    palette.setColor(QPalette::Text, dark ? QColor("#eef3ef") : QColor("#202a25"));
    palette.setColor(QPalette::Button, dark ? QColor("#293630") : QColor("#e7eee9"));
    palette.setColor(QPalette::ButtonText, dark ? QColor("#eef3ef") : QColor("#202a25"));
    palette.setColor(QPalette::Highlight, dark ? QColor("#7bc5a4") : QColor("#397b60"));
    palette.setColor(QPalette::HighlightedText, dark ? QColor("#102019") : QColor("#ffffff"));
    palette.setColor(QPalette::BrightText, dark ? QColor("#ff9387") : QColor("#a32119"));
    palette.setColor(QPalette::PlaceholderText, dark ? QColor("#abb8b0") : QColor("#5e6a63"));
    palette.setColor(QPalette::Mid, dark ? QColor("#6d7c73") : QColor("#849087"));
    palette.setColor(QPalette::Midlight, dark ? QColor("#3b4941") : QColor("#d6ddd7"));
    palette.setColor(QPalette::Disabled, QPalette::Text,
                     dark ? QColor("#a3b0a8") : QColor("#5c665f"));
    palette.setColor(QPalette::Disabled, QPalette::WindowText,
                     dark ? QColor("#a3b0a8") : QColor("#5c665f"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText,
                     dark ? QColor("#a3b0a8") : QColor("#5c665f"));
    palette.setColor(QPalette::Disabled, QPalette::PlaceholderText,
                     dark ? QColor("#89968e") : QColor("#68736c"));
    application->setPalette(palette);
}
