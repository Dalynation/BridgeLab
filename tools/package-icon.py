from pathlib import Path
import struct
root=Path(__file__).resolve().parents[1]
parts=[]
for tag,name in [(b'ic07','icon_128x128.png'),(b'ic08','icon_256x256.png'),(b'ic09','icon_512x512.png'),(b'ic10','icon_512x512@2x.png')]:
    data=(root/'build/BridgeLab.iconset'/name).read_bytes()
    parts.append(tag+struct.pack('>I',len(data)+8)+data)
body=b''.join(parts)
(root/'BridgeLab.app/Contents/Resources/BridgeLab.icns').write_bytes(b'icns'+struct.pack('>I',len(body)+8)+body)
