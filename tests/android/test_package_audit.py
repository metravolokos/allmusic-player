"""Negative cases for the APK publication boundary; no SDK or device needed."""
from pathlib import Path
import runpy
import tempfile
import unittest
import zipfile

AUDIT = runpy.run_path(str(Path(__file__).resolve().parents[2] / 'scripts/package-android.py'))
MANIFEST = '''<manifest xmlns:android="http://schemas.android.com/apk/res/android"
 package="org.metravolokos.allmusicplayer.spike002dmedia3">
 <uses-sdk android:minSdkVersion="28" android:targetSdkVersion="36"/>
 <application android:allowBackup="false" android:usesCleartextTraffic="false">
 <activity android:name="org.qtproject.qt.android.bindings.QtActivity" android:exported="true">
 <intent-filter><action android:name="android.intent.action.MAIN"/>
 <category android:name="android.intent.category.LAUNCHER"/></intent-filter>
 </activity></application></manifest>'''
DEPENDENCIES = ('debugRuntimeClasspath\n'
                '+--- androidx.media3:media3-exoplayer:1.11.1\n'
                '\\--- androidx.core:core:1.17.0')
DEX = b'Landroidx/media3/exoplayer/ExoPlayerImpl;\nMedia3PlaybackAdapter'
ASSETS = ('assets/audio/synthetic-440hz.mp3',
          'assets/audio/synthetic-660hz.flac',
          'assets/audio/synthetic-880hz.wav')
NATIVE = ('liballmusic-player', 'libQt6Core', 'libQt6Gui', 'libQt6Quick')


def write_test_apk(path, abi='arm64-v8a', extra_native=(), dex=DEX, assets=ASSETS):
    with zipfile.ZipFile(path, 'w') as archive:
        for name in NATIVE + tuple(extra_native):
            archive.writestr(f'lib/{abi}/{name}_{abi}.so', b'fixture, not ELF')
        archive.writestr('classes.dex', dex)
        for name in assets:
            archive.writestr(name, b'synthetic test asset')


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
        for before, after in [('spike002dmedia3', 'wrong'), ('minSdkVersion="28"', 'minSdkVersion="27"'),
                              ('targetSdkVersion="36"', 'targetSdkVersion="35"'),
                              ('allowBackup="false"', 'allowBackup="true"'),
                              ('QtActivity', 'WrongActivity')]:
            with self.subTest(change=before), self.assertRaises(AssertionError):
                AUDIT['check_manifest'](MANIFEST.replace(before, after))

    def test_runtime_and_abi_boundary(self):
        with tempfile.TemporaryDirectory() as directory:
            apk = Path(directory) / 'fixture.apk'
            write_test_apk(apk)
            self.assertEqual(len(AUDIT['check_runtime'](apk, DEPENDENCIES)), len(NATIVE))

            write_test_apk(apk, abi='x86_64')
            with self.assertRaises(AssertionError):
                AUDIT['check_runtime'](apk, DEPENDENCIES)

            write_test_apk(apk, dex=b'Lcom/google/android/gms/common/X;')
            with self.assertRaises(AssertionError):
                AUDIT['check_runtime'](apk, DEPENDENCIES)

            write_test_apk(apk, assets=ASSETS[:-1])
            with self.assertRaises(AssertionError):
                AUDIT['check_runtime'](apk, DEPENDENCIES)

    def test_media3_dependency_is_required(self):
        with tempfile.TemporaryDirectory() as directory:
            apk = Path(directory) / 'fixture.apk'
            write_test_apk(apk)
            for dependencies in ('debugRuntimeClasspath\nandroidx.media3:media3-exoplayer:1.11.2',
                                 'debugRuntimeClasspath\nandroidx.core:core:1.17.0'):
                with self.subTest(dependencies=dependencies), self.assertRaises(AssertionError):
                    AUDIT['check_runtime'](apk, dependencies)

    def test_qt_multimedia_and_ffmpeg_runtime_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            apk = Path(directory) / 'fixture.apk'
            write_test_apk(apk, extra_native=('libQt6Multimedia',
                                              'libplugins_multimedia_androidmediaplugin'))
            with self.assertRaises(AssertionError):
                AUDIT['check_runtime'](apk, DEPENDENCIES)

            write_test_apk(apk, extra_native=('libavcodec',))
            with self.assertRaises(AssertionError):
                AUDIT['check_runtime'](apk, DEPENDENCIES)

            write_test_apk(apk)
            with self.assertRaises(AssertionError):
                AUDIT['check_runtime'](apk, DEPENDENCIES + '\nandroidx.media3:media3-decoder-ffmpeg:1.11.1')

            write_test_apk(apk, dex=DEX + b' androidx.media3.decoder.ffmpeg.FfmpegAudioRenderer')
            self.assertEqual(len(AUDIT['check_runtime'](apk, DEPENDENCIES)), len(NATIVE))

            write_test_apk(apk, dex=DEX + b' Landroidx/media3/decoder/ffmpeg/FfmpegAudioRenderer;')
            with self.assertRaises(AssertionError):
                AUDIT['check_runtime'](apk, DEPENDENCIES)

    def test_missing_media3_classes_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            apk = Path(directory) / 'fixture.apk'
            write_test_apk(apk, dex=b'ordinary application classes')
            with self.assertRaises(AssertionError):
                AUDIT['check_runtime'](apk, DEPENDENCIES)

    def test_maven_license_audit_requires_and_records_declared_license(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            pom_dir = root / 'gradle/caches/modules-2/files-2.1/androidx.media3/media3-exoplayer/1.11.1/hash'
            pom_dir.mkdir(parents=True)
            pom = pom_dir / 'media3-exoplayer-1.11.1.pom'
            pom.write_text('''<project xmlns="http://maven.apache.org/POM/4.0.0">
              <licenses><license><name>Apache License, Version 2.0</name>
              <url>https://www.apache.org/licenses/LICENSE-2.0.txt</url></license></licenses>
            </project>''', encoding='utf-8')
            output = root / 'out'
            records = AUDIT['audit_maven_licenses'](
                'debugRuntimeClasspath\\n+--- androidx.media3:media3-exoplayer:1.11.1',
                root / 'gradle', output)
            self.assertIn('Apache License, Version 2.0', records[0])
            self.assertTrue((output / 'licenses/java/DEPENDENCY-LICENSES.txt').is_file())

            failureaccess_dir = root / 'gradle/caches/modules-2/files-2.1/com.google.guava/failureaccess/1.0.2/hash'
            failureaccess_dir.mkdir(parents=True)
            (failureaccess_dir / 'failureaccess-1.0.2.pom').write_text(
                '<project xmlns="http://maven.apache.org/POM/4.0.0"><artifactId>failureaccess</artifactId></project>',
                encoding='utf-8')
            records = AUDIT['audit_maven_licenses'](
                'debugRuntimeClasspath\n+--- androidx.media3:media3-exoplayer:1.11.1'
                '\n+--- com.google.guava:failureaccess:1.0.2',
                root / 'gradle', output)
            failureaccess_record = next(r for r in records if r.startswith('com.google.guava:failureaccess:1.0.2'))
            self.assertIn('Apache License, Version 2.0', failureaccess_record)
            self.assertIn('google/guava/blob/v32.1.1', failureaccess_record)

            pom.write_text('''<project xmlns="http://maven.apache.org/POM/4.0.0">
              <licenses><license><name>Unknown terms</name></license></licenses>
            </project>''', encoding='utf-8')
            with self.assertRaises(AssertionError):
                AUDIT['audit_maven_licenses'](
                    'debugRuntimeClasspath\\n+--- androidx.media3:media3-exoplayer:1.11.1',
                    root / 'gradle', output)


if __name__ == '__main__':
    unittest.main()
