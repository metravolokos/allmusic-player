"""Negative cases for the APK publication boundary; no SDK or device needed."""
from pathlib import Path
import runpy
import tempfile
import unittest
import zipfile

AUDIT = runpy.run_path(str(Path(__file__).resolve().parents[2] / 'scripts/package-android.py'))
MANIFEST = '''<manifest xmlns:android="http://schemas.android.com/apk/res/android"
 package="org.metravolokos.allmusicplayer.spike001c">
 <uses-sdk android:minSdkVersion="28" android:targetSdkVersion="36"/>
 <application android:allowBackup="false" android:usesCleartextTraffic="false">
 <activity android:name="org.qtproject.qt.android.bindings.QtActivity" android:exported="true">
 <intent-filter><action android:name="android.intent.action.MAIN"/>
 <category android:name="android.intent.category.LAUNCHER"/></intent-filter>
 </activity></application></manifest>'''


class PublicationBoundary(unittest.TestCase):
    def test_minimal_manifest(self):
        AUDIT['check_manifest'](MANIFEST)

    def test_permission_and_component_rejections(self):
        for element in ('uses-permission android:name="android.permission.INTERNET"',
                        'uses-permission-sdk-23 android:name="android.permission.READ_MEDIA_AUDIO"',
                        'service android:name="UnexpectedService"',
                        'provider android:name="UnexpectedProvider"',
                        'receiver android:name="UnexpectedReceiver"'):
            with self.subTest(element=element), self.assertRaises(AssertionError):
                AUDIT['check_manifest'](MANIFEST.replace('<application', '<' + element + '/><application'))

    def test_identity_platform_and_backup_rejections(self):
        for before, after in [('spike001c', 'wrong'), ('minSdkVersion="28"', 'minSdkVersion="27"'),
                              ('targetSdkVersion="36"', 'targetSdkVersion="35"'),
                              ('allowBackup="false"', 'allowBackup="true"'),
                              ('QtActivity', 'WrongActivity')]:
            with self.subTest(change=before), self.assertRaises(AssertionError):
                AUDIT['check_manifest'](MANIFEST.replace(before, after))

    def test_runtime_and_abi_boundary(self):
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
                    with zipfile.ZipFile(apk, 'w') as archive:
                        for name in ('liballmusic-player', 'libQt6Quick'):
                            archive.writestr(f'lib/{abi}/{name}_{abi}.so', b'fixture, not ELF')
                        archive.writestr('classes.dex', dex)
                    if accepted:
                        AUDIT['check_runtime'](apk, 'debugRuntimeClasspath\n' + dependency)
                    else:
                        with self.assertRaises(AssertionError):
                            AUDIT['check_runtime'](apk, 'debugRuntimeClasspath\n' + dependency)


if __name__ == '__main__':
    unittest.main()
