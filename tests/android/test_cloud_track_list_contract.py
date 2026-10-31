from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]


def block_for_id(qml, object_id):
    id_position = qml.index(f"id: {object_id}")
    opening = qml.rfind("{", 0, id_position)
    depth = 0
    quote = None
    escaped = False
    for position in range(opening, len(qml)):
        character = qml[position]
        if quote:
            if escaped:
                escaped = False
            elif character == "\\":
                escaped = True
            elif character == quote:
                quote = None
            continue
        if character in {'"', "'"}:
            quote = character
        elif character == "{":
            depth += 1
        elif character == "}":
            depth -= 1
            if depth == 0:
                return qml[opening:position + 1]
    raise AssertionError(f"Unclosed QML object containing id: {object_id}")


class CloudTrackListContractTest(unittest.TestCase):
    def test_only_the_virtualized_catalog_list_scrolls_and_keeps_source_rows(self):
        qml = (ROOT / "ui/android/Main.qml").read_text(encoding="utf-8")
        cloud_list = block_for_id(qml, "cloudList")

        self.assertIn("model: media3.cloudEntriesModel", cloud_list)
        self.assertIn("clip: true", cloud_list)
        self.assertIn("anchors.top: topPanel.bottom", cloud_list)
        self.assertIn("anchors.bottom: playerPanel.top", cloud_list)
        self.assertNotIn("header:", cloud_list)
        self.assertNotIn("footer:", cloud_list)
        self.assertIn("required property int sourceIndex", cloud_list)
        self.assertIn("root.selectCloudEntry(sourceIndex, label, isDirectory)", cloud_list)
        self.assertIn("onCloudEntriesReset()", qml)
        self.assertIn("root.clearSelectionIfHidden()", qml)
        self.assertEqual(qml.count("ListView {"), 1)
        self.assertNotIn("Flickable {", qml)

    def test_search_and_player_controls_stay_outside_the_scrolling_list(self):
        qml = (ROOT / "ui/android/Main.qml").read_text(encoding="utf-8")
        cloud_list = block_for_id(qml, "cloudList")
        top_panel = block_for_id(qml, "topPanel")
        search_panel = block_for_id(qml, "searchPanel")
        sort_panel = block_for_id(qml, "sortAndCountPanel")
        player_panel = block_for_id(qml, "playerPanel")

        self.assertIn("cloudSearchInput", top_panel)
        self.assertIn("media3.cloudEntriesModel.searchText = text", search_panel)
        self.assertIn("media3.cloudEntriesModel.sortMode =", sort_panel)
        self.assertIn("media3.cloudEntriesModel.resultCount", sort_panel)
        self.assertNotIn("cloudSearchInput", cloud_list)
        self.assertIn("anchors.bottom: parent.bottom", player_panel)
        self.assertIn("media3.playCloud(root.selectedCloudIndex)", player_panel)
        self.assertIn("media3.canPlayCloud(root.selectedCloudIndex)", player_panel)
        self.assertIn("media3.pause()", player_panel)
        self.assertIn("media3.stop()", player_panel)
        self.assertIn("media3.seekTo(", player_panel)
        self.assertNotIn("media3.playCloud(", cloud_list)
        self.assertNotIn("media3.seekTo(", cloud_list)

    def test_connection_form_collapses_after_connecting(self):
        qml = (ROOT / "ui/android/Main.qml").read_text(encoding="utf-8")
        top_panel = block_for_id(qml, "topPanel")

        self.assertIn("id: connectForm", top_panel)
        self.assertIn("visible: !media3.cloudConnected", top_panel)
        self.assertIn("id: connectedBar", top_panel)
        self.assertIn("visible: media3.cloudConnected", top_panel)
        self.assertIn("media3.disconnectCloud()", top_panel)

    def test_search_and_sort_are_local_and_keyboard_resizes_the_view(self):
        qml = (ROOT / "ui/android/Main.qml").read_text(encoding="utf-8")
        manifest = (ROOT / "android/AndroidManifest.xml").read_text(encoding="utf-8")

        self.assertIn("media3.cloudEntriesModel.searchText = text", qml)
        self.assertIn("media3.cloudEntriesModel.sortMode =", qml)
        self.assertIn("media3.cloudEntriesModel.resultCount", qml)
        self.assertIn('text: "Совпадений нет"', qml)
        self.assertIn('android:windowSoftInputMode="adjustResize"', manifest)


if __name__ == "__main__":
    unittest.main()
