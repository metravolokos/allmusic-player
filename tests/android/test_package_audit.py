"""Negative cases for the Android Qt FFmpeg package boundary; no SDK/device needed."""
from pathlib import Path
import json
import runpy
import tempfile
import unittest
import zipfile

AUDIT = runpy.run_path(str(Path(__file__).resolve().parents[2] / 'scripts/package-android.py'))
MANIFEST = '''<manifest xmlns:android="http://schemas.android.com/apk/res/android"
 package="org.metravolokos.allmusicplayer.spike002d">
 <uses-sdk android:minSdkVersion="28" android:targetSdkVersion="36"/>
 <application android:allowBackup="false" android:usesCleartextTraffic="false">
 <activity android:name="org.qtproject.qt.android.bindings.QtActivity" android:exported="true">
 <intent-filter><action android:name="android.intent.action.MAIN"/>
 <category android:name="android.intent.category.LAUNCHER"/></intent-filter>
 </activity></application></manifest>'''
FFMPEG_NATIVE = (
    'liballmusic-player_arm64-v8a.so', 'libQt6Quick_arm64-v8a.so',
    'libQt6Multimedia_arm64-v8a.so', 'libQt6Concurrent_arm64-v8a.so',
    'libplugins_multimedia_ffmpegmediaplugin_arm64-v8a.so',
    'libavcodec.so', 'libavformat.so', 'libavutil.so',
    'libswresample.so', 'libswscale.so',
)


def write_apk(path, abi='arm64-v8a', native=FFMPEG_NATIVE, dex=b'normal'):
    with zipfile.ZipFile(path, 'w') as archive:
        for name in native:
            archive.writestr(f'lib/{abi}/{name}', b'fixture, not ELF')
        archive.writestr('classes.dex', dex)


class PublicationBoundary(unittest.TestCase):
    def test_minimal_manifest(self):
        AUDIT['check_manifest'](MANIFEST)

    def test_permission_and_component_rejections(self):
        for element in ('uses-permission android:name="android.permission.INTERNET"',
                        'uses-permission-sdk-23 android:name="android.permission.READ_MEDIA_AUDIO"',
                        'uses-permission android:name="android.permission.RECORD_AUDIO"',
                        'uses-permission android:name="android.permission.CAMERA"',
                        'uses-permission android:name="android.permission.BLUETOOTH"',
                        'uses-permission android:name="android.permission.MODIFY_AUDIO_SETTINGS"',
                        'service android:name="UnexpectedService"',
                        'provider android:name="UnexpectedProvider"',
                        'receiver android:name="UnexpectedReceiver"'):
            with self.subTest(element=element), self.assertRaises(AssertionError):
                AUDIT['check_manifest'](MANIFEST.replace('<application', '<' + element + '/><application'))

    def test_identity_platform_and_backup_rejections(self):
        for before, after in [('spike002d', 'wrong'), ('minSdkVersion="28"', 'minSdkVersion="27"'),
                              ('targetSdkVersion="36"', 'targetSdkVersion="35"'),
                              ('allowBackup="false"', 'allowBackup="true"'),
                              ('QtActivity', 'WrongActivity')]:
            with self.subTest(change=before), self.assertRaises(AssertionError):
                AUDIT['check_manifest'](MANIFEST.replace(before, after))

    def test_ffmpeg_runtime_and_abi_boundary(self):
        with tempfile.TemporaryDirectory() as directory:
            apk = Path(directory) / 'fixture.apk'
            for abi, dex, dependency, accepted in [
                ('arm64-v8a', b'normal', 'androidx.core:core:1.17.0', True),
                ('x86_64', b'normal', '', False),
                ('arm64-v8a', b'Lcom/google/android/gms/common/X;', '', False),
                ('arm64-v8a', b'normal', 'com.google.firebase:firebase-common', False),
                ('arm64-v8a', b'normal', 'androidx.core:core:1.17.0 FAILED', False),
            ]:
                with self.subTest(abi=abi, dex=dex, dependency=dependency):
                    write_apk(apk, abi=abi, dex=dex)
                    if accepted:
                        AUDIT['check_runtime'](apk, 'debugRuntimeClasspath\n' + dependency)
                    else:
                        with self.assertRaises(AssertionError):
                            AUDIT['check_runtime'](apk, 'debugRuntimeClasspath\n' + dependency)

    def test_android_system_backend_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            apk = Path(directory) / 'fixture.apk'
            native = (*FFMPEG_NATIVE, 'libplugins_multimedia_androidmediaplugin_arm64-v8a.so')
            write_apk(apk, native=native)
            with self.assertRaises(AssertionError):
                AUDIT['check_runtime'](apk, 'debugRuntimeClasspath\nandroidx.core:core:1.17.0')

    def test_every_ffmpeg_runtime_library_is_required(self):
        with tempfile.TemporaryDirectory() as directory:
            apk = Path(directory) / 'fixture.apk'
            write_apk(apk, native=tuple(name for name in FFMPEG_NATIVE
                                        if name != 'libavcodec.so'))
            with self.assertRaises(AssertionError):
                AUDIT['check_runtime'](apk, 'debugRuntimeClasspath\nandroidx.core:core:1.17.0')

            write_apk(apk, native=(*FFMPEG_NATIVE, 'libpostproc.so.58'))
            with self.assertRaises(AssertionError):
                AUDIT['check_runtime'](apk, 'debugRuntimeClasspath\nandroidx.core:core:1.17.0')

            write_apk(apk, native=tuple(
                'libavcodec.so.999' if name == 'libavcodec.so' else name
                for name in FFMPEG_NATIVE))
            with self.assertRaises(AssertionError):
                AUDIT['check_runtime'](apk, 'debugRuntimeClasspath\nandroidx.core:core:1.17.0')

    def test_multimedia_and_ffmpeg_plugin_are_required(self):
        with tempfile.TemporaryDirectory() as directory:
            apk = Path(directory) / 'fixture.apk'
            native = tuple(name for name in FFMPEG_NATIVE
                           if name not in {'libQt6Multimedia',
                                           'libplugins_multimedia_ffmpegmediaplugin_arm64-v8a.so'})
            write_apk(apk, native=native)
            with self.assertRaises(AssertionError):
                AUDIT['check_runtime'](apk, 'debugRuntimeClasspath\nandroidx.core:core:1.17.0')

    def test_qt_ffmpeg_license_and_attribution_are_required(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'LICENSE.LGPL-2.1-or-later.txt').write_text('LGPL notice')
            attribution = [{
                'Id': 'ffmpeg', 'Version': '9.0.1',
                'LicenseId': 'LGPL-2.1-or-later AND BSD-3-Clause AND BSD-2-Clause '
                             'AND BSD-Source-Code AND ISC AND MIT AND MPL-2.0',
                'QtUsage': 'The binaries do not contain these components.',
            }]
            (root / 'qt_attribution.json').write_text(json.dumps(attribution))
            AUDIT['check_ffmpeg_licenses'](root)
            (root / 'qt_attribution.json').unlink()
            with self.assertRaises(AssertionError):
                AUDIT['check_ffmpeg_licenses'](root)

    def test_qt_ffmpeg_license_rejects_gpl_lgplv3_and_unknown_components(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'LICENSE.LGPL-2.1-or-later.txt').write_text('LGPL notice')
            base = {
                'Id': 'ffmpeg', 'Version': '9.0.1',
                'LicenseId': 'LGPL-2.1-or-later AND BSD-3-Clause AND BSD-2-Clause '
                             'AND BSD-Source-Code AND ISC AND MIT AND MPL-2.0',
                'QtUsage': 'The binaries do not contain these components.',
            }
            for invalid in (
                    dict(base, LicenseId=base['LicenseId'] + ' AND GPL-2.0-only'),
                    dict(base, LicenseId=base['LicenseId'] + ' AND LGPL-3.0-only'),
                    dict(base, LicenseId=base['LicenseId'] + ' AND Proprietary-1.0'),
                    dict(base, Version='9.0.2'),
                    dict(base, QtUsage='Optional components are present.'),
            ):
                with self.subTest(invalid=invalid), self.assertRaises(AssertionError):
                    (root / 'qt_attribution.json').write_text(json.dumps([invalid]))
                    AUDIT['check_ffmpeg_licenses'](root)

            (root / 'LICENSE.LGPL-2.1-or-later.txt').unlink()
            with self.assertRaises(AssertionError):
                (root / 'qt_attribution.json').write_text(json.dumps([base]))
                AUDIT['check_ffmpeg_licenses'](root)


if __name__ == '__main__':
    unittest.main()
