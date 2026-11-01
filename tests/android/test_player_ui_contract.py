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

    def test_screens_use_flat_media_rows_and_documented_visual_preview_mode(self):
        for platform in ("android", "desktop"):
            with self.subTest(platform=platform):
                qml = (ROOT / "ui" / platform / "Main.qml").read_text(encoding="utf-8")
                self.assertIn("readonly property color accentColor", qml)
                self.assertIn("readonly property color surfaceColor", qml)
                self.assertIn("background: Rectangle", qml)
                self.assertIn("property bool isCurrent:", qml)
                self.assertIn("showcaseMode", qml)
                self.assertIn('stableId: "preview-02"', qml)

        main = (ROOT / "src/app/main.cpp").read_text(encoding="utf-8")
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("setPreviewThemeMode(smokeTheme)", main)
        self.assertIn('QStringLiteral("AndroidPreview")', main)
        for variant in (
            "ui-preview-android-library-light", "ui-preview-android-library-dark",
            "ui-preview-android-now-light", "ui-preview-android-now-dark",
            "ui-preview-desktop-library-light", "ui-preview-desktop-library-dark",
            "ui-preview-desktop-now-light", "ui-preview-desktop-now-dark",
        ):
            with self.subTest(variant=variant):
                self.assertIn(variant, cmake)

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
        self.assertIn("mediaController.stateText", qml)
        self.assertIn("mediaController.seekTo(Math.round(value))", qml)
        self.assertIn("mediaController.disconnectCloud()", qml)
        self.assertIn("implicitHeight: 60", qml)
        self.assertIn("implicitHeight: 48", qml)
        self.assertIn('text: "Стоп"', qml)


if __name__ == "__main__":
    unittest.main()
