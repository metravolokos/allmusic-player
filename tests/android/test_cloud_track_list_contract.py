from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]


class CloudTrackListContractTest(unittest.TestCase):
    def test_android_cloud_catalog_uses_a_virtualized_list_and_source_rows(self):
        qml = (ROOT / "ui/android/Main.qml").read_text(encoding="utf-8")
        start = qml.index("ListView {\n        id: page")
        delegate = qml.index("delegate: Rectangle {", start)
        cloud_list = qml[start:delegate]

        self.assertIn("model: media3.cloudEntriesModel", cloud_list)
        self.assertNotIn("model: media3.cloudEntries\n", cloud_list)
        self.assertIn("required property int sourceIndex", qml[delegate:])
        self.assertIn("root.selectCloudEntry(sourceIndex, isDirectory)", qml[delegate:])
        self.assertIn("onCloudEntriesReset()", qml)
        self.assertIn("root.clearSelectionIfHidden()", qml)
        self.assertIn("media3.playCloud(root.selectedCloudIndex)", qml)

    def test_search_and_sort_are_local_and_keyboard_resizes_the_view(self):
        qml = (ROOT / "ui/android/Main.qml").read_text(encoding="utf-8")
        manifest = (ROOT / "android/AndroidManifest.xml").read_text(encoding="utf-8")

        self.assertIn("media3.cloudEntriesModel.searchText = text", qml)
        self.assertIn("media3.cloudEntriesModel.sortMode =", qml)
        self.assertIn("media3.cloudEntriesModel.resultCount", qml)
        self.assertIn('text: "Совпадений нет"', qml)
        self.assertIn("page.positionViewAtBeginning()", qml)
        self.assertIn('android:windowSoftInputMode="adjustResize"', manifest)


if __name__ == "__main__":
    unittest.main()
