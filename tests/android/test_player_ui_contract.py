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

    def test_both_platforms_expose_real_catalog_scopes_and_queue_controls(self):
        for platform in ("android", "desktop"):
            with self.subTest(platform=platform):
                qml = (ROOT / "ui" / platform / "Main.qml").read_text(encoding="utf-8")
                for contract in (
                    "library.firstPlayableStableId", "queue.startArtist", "queue.startAlbum",
                    "queue.startFolder", "queue.previous()", "queue.next()",
                    "queue.repeatMode", "queue.shuffleEnabled", "Очередь воспроизведения",
                    "artistAlbums", "Альбомы исполнителя",
                    "echoMode: TextInput.Password", "appearance.setThemeMode",
                ):
                    self.assertIn(contract, qml)
                self.assertNotIn("избран", qml.casefold())

    def test_desktop_connects_cloud_and_local_playback(self):
        qml = (ROOT / "ui/desktop/Main.qml").read_text(encoding="utf-8")
        for contract in (
            "model: mailWebDav.entries", "mailWebDav.openDirectory(index)",
            "mailWebDav.goToParent()", "queue.startFolder(mailWebDav.currentDirectory",
            "playback.openLocal(selectedFile)", "playback.togglePlayback()",
            "playback.stop()", "media.setPosition(value)",
            "playback.audio.volume = value", "mailAppPassword.clear()",
            "Реквизиты не сохраняются",
        ):
            with self.subTest(contract=contract):
                self.assertIn(contract, qml)

    def test_android_uses_media3_and_large_touch_targets(self):
        qml = (ROOT / "ui/android/Main.qml").read_text(encoding="utf-8")
        self.assertIn("media3.stateText", qml)
        self.assertIn("media3.seekTo(Math.round(value))", qml)
        self.assertIn("media3.disconnectCloud()", qml)
        self.assertIn("implicitHeight: 54", qml)
        self.assertIn("implicitHeight: 48", qml)
        self.assertIn('text: "Стоп"', qml)


if __name__ == "__main__":
    unittest.main()
