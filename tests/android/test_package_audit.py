"""Negative cases for the APK publication boundary; no SDK or device needed."""
from pathlib import Path
import runpy
import tempfile
import unittest
import zipfile

AUDIT = runpy.run_path(str(Path(__file__).resolve().parents[2] / 'scripts/package-android.py'))
OPENSSL_BUILD = runpy.run_path(
    str(Path(__file__).resolve().parents[2] / 'scripts/build-android-openssl.py'))
MANIFEST = '''<manifest xmlns:android="http://schemas.android.com/apk/res/android"
 package="org.metravolokos.allmusicplayer.cloud002dmedia3">
 <uses-sdk android:minSdkVersion="28" android:targetSdkVersion="36"/>
 <uses-permission android:name="android.permission.INTERNET"/>
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
NATIVE = ('liballmusic-player', 'libQt6Core', 'libQt6Gui', 'libQt6Quick',
          'libcrypto_3', 'libssl_3', 'libplugins_tls_qopensslbackend_arm64-v8a')


def write_test_apk(path, abi='arm64-v8a', extra_native=(), dex=DEX, assets=ASSETS,
                   native=NATIVE):
    with zipfile.ZipFile(path, 'w') as archive:
        for name in tuple(native) + tuple(extra_native):
            suffix = ('.so' if name.endswith('_arm64-v8a')
                      or name in {'libcrypto_3', 'libssl_3'} else f'_{abi}.so')
            archive.writestr(f'lib/{abi}/{name}{suffix}', b'fixture, not ELF')
        archive.writestr('classes.dex', dex)
        for name in assets:
            archive.writestr(name, b'synthetic test asset')


class PublicationBoundary(unittest.TestCase):
    def test_openssl_arm64_suffix_is_configured_before_linking(self):
        config = '''    "android-arm" => {
        perlasm_scheme   => "void",
    },
    "android-arm64" => {
        inherit_from     => [ "android" ],
        asm_arch         => 'aarch64',
        perlasm_scheme   => "linux64",
    },
    "android-mips" => {
        perlasm_scheme   => "o32",
    },
'''
        patched = OPENSSL_BUILD['configure_android_arm64_variant'](config)
        self.assertIn('shlib_variant   => "_3",', patched)
        self.assertEqual(OPENSSL_BUILD['configure_android_arm64_variant'](patched), patched)
        self.assertNotIn('shlib_variant', patched.split('"android-mips"')[1])
        with self.assertRaises(ValueError):
            OPENSSL_BUILD['configure_android_arm64_variant'](config.replace('"android-arm64"', '"android-aarch64"'))

    def test_gnu_version_needs_match_needed_and_provider_definitions(self):
        dynamic = {
            'libssl_3.so': ' 0x1 (NEEDED) Shared library: [libcrypto_3.so]',
            'libcrypto_3.so': '',
        }
        versions = {
            'libssl_3.so': '''Version needs section '.gnu.version_r' contains 1 entry:
  0x00: Version: 1 File: libcrypto_3.so Cnt: 1
  0x10: Name: OPENSSL_3.5.0 Flags: none Version: 1
''',
            'libcrypto_3.so': '''Version definition section '.gnu.version_d' contains 2 entries:
  0x00: Rev: 1 Flags: BASE Index: 1 Cnt: 1
  0x10: Name: libcrypto_3.so
  0x20: Rev: 1 Flags: none Index: 2 Cnt: 1
  0x30: Name: OPENSSL_3.5.0
''',
        }
        AUDIT['audit_gnu_versions'](dynamic, versions)
        with self.assertRaises(AssertionError):
            AUDIT['audit_gnu_versions'](
                {'libssl_3.so': '', 'libcrypto_3.so': ''}, versions)
        with self.assertRaises(AssertionError):
            AUDIT['audit_gnu_versions'](
                dynamic, {**versions, 'libcrypto_3.so': versions['libcrypto_3.so'].replace('OPENSSL_3.5.0', 'OPENSSL_3.4.0')})

    def test_minimal_manifest(self):
        AUDIT['check_manifest'](MANIFEST)

    def test_internet_is_the_only_permission(self):
        self.assertRaises(AssertionError, AUDIT['check_manifest'],
                          MANIFEST.replace('android.permission.INTERNET',
                                           'android.permission.ACCESS_NETWORK_STATE'))

    def test_permission_and_component_rejections(self):
        for element in ('uses-permission-sdk-23 android:name="android.permission.READ_MEDIA_AUDIO"',
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
        for before, after in [('cloud002dmedia3', 'wrong'), ('minSdkVersion="28"', 'minSdkVersion="27"'),
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

    def test_fusion_qt_controls_are_audited_without_widening_unknown_modules(self):
        # Exact names observed in the 6.12.0 Android arm64 APK after enabling Fusion.
        qt_modules = (
            "QuickControls2", "QuickControls2Impl", "QuickControls2Basic",
            "QuickControls2BasicStyleImpl", "QuickControls2Fusion",
            "QuickControls2FusionStyleImpl", "QuickControls2FluentWinUI3",
            "QuickControls2FluentWinUI3StyleImpl", "QuickControls2Imagine",
            "QuickControls2ImagineStyleImpl", "QuickControls2Material",
            "QuickControls2MaterialStyleImpl", "QuickControls2Universal",
            "QuickControls2UniversalStyleImpl", "QuickEffects", "QuickLayouts",
            "QuickShapes", "QuickTemplates2",
        )
        with tempfile.TemporaryDirectory() as directory:
            apk = Path(directory) / 'fusion.apk'
            extra = tuple('libQt6' + module for module in qt_modules)
            write_test_apk(apk, extra_native=extra)
            self.assertEqual(len(AUDIT['check_runtime'](apk, DEPENDENCIES)),
                             len(NATIVE) + len(extra))
            write_test_apk(apk, extra_native=('libQt6QuickControls2Unreviewed',))
            with self.assertRaisesRegex(AssertionError, 'Review Qt module license'):
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

    def test_openssl_runtime_is_required(self):
        with tempfile.TemporaryDirectory() as directory:
            apk = Path(directory) / 'fixture.apk'
            write_test_apk(apk, native=NATIVE[:-2])
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
            self.assertIn('google/guava/blob/v33.3.1', failureaccess_record)

            guava_dir = root / 'gradle/caches/modules-2/files-2.1/com.google.guava/guava/33.3.1-android/hash'
            guava_dir.mkdir(parents=True)
            (guava_dir / 'guava-33.3.1-android.pom').write_text(
                '<project xmlns="http://maven.apache.org/POM/4.0.0"><artifactId>guava</artifactId></project>',
                encoding='utf-8')
            records = AUDIT['audit_maven_licenses'](
                'debugRuntimeClasspath\n+--- androidx.media3:media3-exoplayer:1.11.1'
                '\n+--- com.google.guava:failureaccess:1.0.2'
                '\n+--- com.google.guava:guava:33.3.1-android',
                root / 'gradle', output)
            guava_record = next(r for r in records if r.startswith('com.google.guava:guava:33.3.1-android'))
            self.assertIn('Apache License, Version 2.0', guava_record)
            self.assertIn('google/guava/blob/v33.3.1/pom.xml', guava_record)

            listenablefuture_dir = root / ('gradle/caches/modules-2/files-2.1/com.google.guava/'
                                            'listenablefuture/9999.0-empty-to-avoid-conflict-with-guava/hash')
            listenablefuture_dir.mkdir(parents=True)
            (listenablefuture_dir / 'listenablefuture-9999.0-empty-to-avoid-conflict-with-guava.pom').write_text(
                '<project xmlns="http://maven.apache.org/POM/4.0.0"><artifactId>listenablefuture</artifactId></project>',
                encoding='utf-8')
            records = AUDIT['audit_maven_licenses'](
                'debugRuntimeClasspath\n+--- androidx.media3:media3-exoplayer:1.11.1'
                '\n+--- com.google.guava:failureaccess:1.0.2'
                '\n+--- com.google.guava:guava:33.3.1-android'
                '\n+--- com.google.guava:listenablefuture:9999.0-empty-to-avoid-conflict-with-guava',
                root / 'gradle', output)
            listenablefuture_record = next(
                r for r in records if r.startswith(
                    'com.google.guava:listenablefuture:9999.0-empty-to-avoid-conflict-with-guava'))
            self.assertIn('Apache License, Version 2.0', listenablefuture_record)
            self.assertIn('guava-parent/26.0-android', listenablefuture_record)

            pom.write_text('''<project xmlns="http://maven.apache.org/POM/4.0.0">
              <licenses><license><name>Unknown terms</name></license></licenses>
            </project>''', encoding='utf-8')
            with self.assertRaises(AssertionError):
                AUDIT['audit_maven_licenses'](
                    'debugRuntimeClasspath\\n+--- androidx.media3:media3-exoplayer:1.11.1',
                    root / 'gradle', output)


if __name__ == '__main__':
    unittest.main()
