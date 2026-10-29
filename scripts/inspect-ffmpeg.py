"""Query the deployed library itself rather than guessing from DLL filenames."""
import ctypes
import json
import os
from pathlib import Path
import re
import sys

path = Path(os.environ['ALLMUSIC_AVUTIL']).resolve()
with os.add_dll_directory(str(path.parent)):
    library = ctypes.CDLL(str(path))
    result = {}
    for symbol in ('av_version_info', 'avutil_configuration', 'avutil_license'):
        function = getattr(library, symbol)
        function.restype = ctypes.c_char_p
        result[symbol] = function().decode('utf-8')
release = re.fullmatch(r'(\d+\.\d+(?:\.\d+)?)', result['av_version_info'])
assert release, 'Non-release FFmpeg build: determine exact corresponding source before packaging'
assert '--enable-gpl' not in result['avutil_configuration']
assert '--enable-nonfree' not in result['avutil_configuration']
assert 'LGPL' in result['avutil_license'], result
result['release'] = release[1]
Path(sys.argv[1]).write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
