"""Read-only diagnostics; never installs an alternative Qt version."""
import urllib.error
import urllib.request
import xml.etree.ElementTree as ET

base = "https://download.qt.io/online/qtsdkrepository"
for host, version, nested in (
    ("windows_x86", "6120", True),
    ("windows_x86", "6120", False),
    ("windows_x64", "6120", True),
    ("windows_x86", "6113", True),
):
    directory = f"qt6_{version}"
    suffix = f"{directory}/Updates.xml" if nested else "Updates.xml"
    url = f"{base}/{host}/desktop/{directory}/{suffix}"
    try:
        with urllib.request.urlopen(url, timeout=20) as response:
            data = response.read(8 * 1024 * 1024)
            print(f"{url}: HTTP {response.status}, {len(data)} bytes")
        names = [entry.text for entry in ET.fromstring(data).findall(".//PackageUpdate/Name")]
        print("MSVC packages:", [name for name in names if name and "msvc2022" in name])
    except (urllib.error.URLError, ET.ParseError, TimeoutError) as error:
        print(f"{url}: {error}")
