"""Read-only diagnostics; never installs an alternative Qt version."""
import urllib.error
import urllib.request
import re
import xml.etree.ElementTree as ET

base = "https://download.qt.io/online/qtsdkrepository"
for directory in (
    "windows_x86/desktop/",
    "windows_x86/desktop/qt6_6120/",
    "windows_x64/desktop/qt6_6120/",
):
    url = f"{base}/{directory}"
    try:
        with urllib.request.urlopen(url, timeout=20) as response:
            page = response.read(1024 * 1024).decode("utf-8")
            links = re.findall(r'href="([^"]+)"', page)
            print(f"Index {url}: HTTP {response.status}")
            print("Entries:", [link for link in links if "qt6_61" in link
                               or "msvc" in link or "Updates" in link][:60])
    except (urllib.error.URLError, UnicodeError, TimeoutError) as error:
        print(f"Index {url}: {error}")

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
