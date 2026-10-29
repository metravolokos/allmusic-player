"""Read-only diagnostics; never installs an alternative Qt version."""
import urllib.error
import urllib.request
import re
import xml.etree.ElementTree as ET

base = "https://download.qt.io/online/qtsdkrepository"
for directory in (
    "windows_x86/desktop/",
    "windows_x86/desktop/qt6_6120/",
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

for version in ("6120", "6113"):
    directory = f"qt6_{version}"
    suffix = f"{directory}_msvc2022_64/Updates.xml"
    url = f"{base}/windows_x86/desktop/{directory}/{suffix}"
    try:
        with urllib.request.urlopen(url, timeout=20) as response:
            data = response.read(8 * 1024 * 1024)
            print(f"{url}: HTTP {response.status}, {len(data)} bytes")
        names = [entry.text for entry in ET.fromstring(data).findall(".//PackageUpdate/Name")]
        print("MSVC packages:", [name for name in names if name and "msvc2022" in name])
    except (urllib.error.URLError, ET.ParseError, TimeoutError) as error:
        print(f"{url}: {error}")
