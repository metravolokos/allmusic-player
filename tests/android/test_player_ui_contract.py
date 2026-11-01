"""Cross-platform QML and appearance wiring regression checks."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]


class PlayerUiContractTest(unittest.TestCase):
    def test_both_platforms_use_fusion_and_offer_saved_three_way_theme(self):
        for platform in ("android", "desktop"):
            with self.subTest(platform=platform):
                qml = (ROOT / "ui" / platform / "Main.qml").read_text(encoding="utf-8")
                self.assertIn("import QtQuick.Controls.Fusion", qml)
                self.assertIn('model: ["Системная", "Светлая", "Тёмная"]', qml)
                self.assertIn('appearance.setThemeMode(["system", "light", "dark"][index])', qml)

        controller = (ROOT / "src/app/AppearanceController.cpp").read_text(encoding="utf-8")
        header = (ROOT / "src/app/AppearanceController.h").read_text(encoding="utf-8")
        main = (ROOT / "src/app/main.cpp").read_text(encoding="utf-8")
        self.assertIn('QQuickStyle::setStyle(QStringLiteral("Fusion"))', controller)
        self.assertIn('QSettings::IniFormat, QSettings::UserScope', controller)
        self.assertIn('"appearance/themeMode"', controller)
        self.assertIn("Q_PROPERTY(QString themeMode", header)
        self.assertIn("AppearanceController::prepareFusionStyle();", main)

    def test_desktop_library_and_controls_are_visible_in_primary_layout(self):
        qml = (ROOT / "ui/desktop/Main.qml").read_text(encoding="utf-8")
        for contract in (
            "ListView {", "model: mailWebDav.entries", "mailWebDav.openDirectory(window.selectedEntry)",
            "mailWebDav.goToParent()", "mailWebDav.play(window.selectedEntry)",
            "playback.openLocal(selectedFile)", "playback.togglePlayback()", "playback.stop()",
            "media.setPosition(value)", "playback.audio.volume = value",
            "property bool diagnosticsExpanded: false",
        ):
            with self.subTest(contract=contract):
                self.assertIn(contract, qml)
        self.assertIn('echoMode: TextInput.Password', qml)
        self.assertIn("mailAppPassword.clear()", qml)
        self.assertIn("Данные не сохраняются", qml)
        self.assertIn("id: playerPanel", qml)
        self.assertIn("id: cloudPanel", qml)

    def test_android_retains_touch_controls_and_media3_status(self):
        qml = (ROOT / "ui/android/Main.qml").read_text(encoding="utf-8")
        self.assertIn("id: cloudList", qml)
        self.assertIn("media3.stateText", qml)
        self.assertIn("media3.playCloud(root.selectedCloudIndex)", qml)
        self.assertIn("media3.seekTo(Math.round(value))", qml)
        self.assertIn("media3.disconnectCloud()", qml)
        self.assertIn("implicitHeight: 54", qml)
        self.assertIn("implicitHeight: 48", qml)


if __name__ == "__main__":
    unittest.main()
