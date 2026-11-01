from pathlib import Path
import re
import unittest
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[2]


class AndroidLibraryUiContractTest(unittest.TestCase):
    def test_catalog_screens_use_virtualized_lists_and_shared_library_model(self):
        qml = (ROOT / "ui/android/Main.qml").read_text(encoding="utf-8")
        for list_id in ("trackList", "artistList", "artistTrackList", "artistAlbumList", "albumList",
                        "albumTrackList", "folderList", "queueList"):
            with self.subTest(list_id=list_id):
                self.assertIn(f"id: {list_id}", qml)
        self.assertIn("model: library", qml)
        self.assertIn("model: root.cloudConnected() ? mailWebDav.entries : []", qml)
        self.assertIn("clip: true", qml)
        self.assertIn("queue.jumpTo(stableId)", qml)
        self.assertIn("queue.startFolder(root.cloudDirectory()", qml)

    def test_background_tag_scan_reads_bounded_ranges_without_whole_files(self):
        controller = (ROOT / "src/app/MailWebDavController.cpp").read_text(encoding="utf-8")
        self.assertIn("LibraryMetadataRangeLimit = 64 * 1024", controller)
        self.assertIn("qMin(LibraryMetadataRangeLimit, *entry.size - 1)", controller)
        self.assertIn("m_indexProbe->readRange(entry, 0, length)", controller)

    def test_search_sort_and_touch_controls_stay_outside_the_lists(self):
        qml = (ROOT / "ui/android/Main.qml").read_text(encoding="utf-8")
        track_list = qml.split("id: trackList", 1)[1].split("delegate: ItemDelegate", 1)[0]
        self.assertNotIn("TextField", track_list)
        self.assertIn("library.searchText = text", qml)
        self.assertIn("onActivated: library.sortMode = index", qml)
        self.assertIn("implicitHeight: 60", qml)
        self.assertIn("implicitHeight: 46", qml)
        self.assertIn("id: miniPlayer", qml)
        self.assertIn("id: navigation", qml)

    def test_connection_form_clears_password_and_shows_live_status(self):
        qml = (ROOT / "ui/android/Main.qml").read_text(encoding="utf-8")
        self.assertIn('echoMode: TextInput.Password', qml)
        self.assertIn("passwordInput.clear()", qml)
        self.assertIn("mediaController.connectCloud(emailInput.text, passwordInput.text)", qml)
        self.assertIn("mediaController.disconnectCloud()", qml)
        self.assertIn("root.cloudStatus()", qml)
        self.assertIn("!root.isCloudBusy() && !root.cloudConnected()", qml)

    def test_android_qml_methods_are_slots_and_properties_are_exposed(self):
        qml = (ROOT / "ui/android/Main.qml").read_text(encoding="utf-8")
        controller = (ROOT / "src/app/AndroidMedia3Controller.h").read_text(encoding="utf-8")
        model = (ROOT / "src/app/AndroidCloudTrackListModel.h").read_text(encoding="utf-8")
        slots = controller.split("public slots:", 1)[1].split("signals:", 1)[0]
        methods = set(re.findall(r"\bmediaController\.([A-Za-z]\w*)\s*\(", qml))
        self.assertTrue(methods)
        for name in sorted(methods):
            with self.subTest(slot=name):
                self.assertRegex(slots, rf"\b{re.escape(name)}\s*\(")

        for name in ("cloudEntriesModel", "cloudConnected", "cloudBusy", "favoritesModel",
                     "cloudStatus", "cloudDirectoryLabel", "cloudDirectoryPath",
                     "activeTrackName", "stateText", "errorText", "positionMs",
                     "durationMs", "playEnabled", "pauseEnabled", "stopEnabled",
                     "seekEnabled", "trackNames", "trackIndex", "outputStatusText"):
            with self.subTest(property=name):
                self.assertRegex(controller, rf"Q_PROPERTY\([^\n]*\b{re.escape(name)}\b")
        for name in ("searchText", "sortMode", "totalCount", "resultCount"):
            with self.subTest(model_property=name):
                self.assertRegex(model, rf"Q_PROPERTY\([^\n]*\b{re.escape(name)}\b")
        self.assertRegex(model, r"Q_INVOKABLE\s+bool\s+containsSourceIndex\s*\(")

    def test_favorites_backend_remains_while_ui_has_no_favorites_controls(self):
        qml = (ROOT / "ui/android/Main.qml").read_text(encoding="utf-8")
        controller = (ROOT / "src/app/AndroidMedia3Controller.h").read_text(encoding="utf-8")
        favorites = (ROOT / "src/app/AndroidFavoritesModel.h").read_text(encoding="utf-8")
        implementation = (ROOT / "src/app/AndroidFavoritesModel.cpp").read_text(encoding="utf-8")
        self.assertNotIn("избран", qml.casefold())
        self.assertIn("Q_PROPERTY(AndroidFavoritesModel *favoritesModel", controller)
        self.assertIn("toggleFavorite", controller)
        self.assertIn("QSettings", implementation)
        self.assertIn("IsMissingRole", favorites)

    def test_android_manifest_locks_the_activity_to_portrait(self):
        manifest_path = ROOT / "android/AndroidManifest.xml"
        manifest = ET.parse(manifest_path).getroot()
        android_namespace = "{http://schemas.android.com/apk/res/android}"
        activity = manifest.find(".//activity")
        self.assertIsNotNone(activity)
        self.assertEqual(activity.get(android_namespace + "screenOrientation"), "portrait")


if __name__ == "__main__":
    unittest.main()
