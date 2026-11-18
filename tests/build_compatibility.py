"""Check packaged CPU slices and deployment metadata; this is not OS certification."""
from pathlib import Path
import json,plistlib,re,subprocess
root=Path(__file__).resolve().parents[1];app=root/'BridgeLab.app';engine=app/'Contents/Resources/Engine'
paths=[app/'Contents/MacOS/BridgeLab',engine/'BridgeLabInstrument.vst3/Contents/MacOS/BridgeLabInstrument',engine/'libBridgeLabUI.dylib',engine/'libBridgeLabRealtime.dylib']
results=[]
for path in paths:
 slices=subprocess.check_output(['lipo','-archs',str(path)],text=True).split();assert set(slices)=={'arm64','x86_64'},(path,slices)
 for arch in slices:
  loads=subprocess.check_output(['otool','-arch',arch,'-l',str(path)],text=True)
  versions=[]
  for command in re.split(r'Load command \d+',loads):
   field='minos' if 'cmd LC_BUILD_VERSION' in command else 'version' if 'cmd LC_VERSION_MIN_MACOSX' in command else None
   if field:versions+=re.findall(r'^\s*'+field+r' (\d+\.\d+(?:\.\d+)?)\s*$',command,re.M)
  assert versions,(path,arch)
  assert all(tuple(map(int,v.split('.')[:2]))<=(11,0) for v in versions),(path,versions)
  deps=subprocess.check_output(['otool','-arch',arch,'-L',str(path)],text=True).splitlines()[1:]
  deps=[line.strip().split(' (')[0] for line in deps]
  assert all(d.startswith(('/System/Library/','/usr/lib/','@rpath/','@loader_path/','@executable_path/')) for d in deps),(path,deps)
  results.append(dict(binary=str(path.relative_to(app)),arch=arch,minimum_versions=versions,dependencies=deps,passed=True))
plist=plistlib.loads((app/'Contents/Info.plist').read_bytes());assert plist['LSMinimumSystemVersion']=='11.0'
print(json.dumps(dict(deployment_target='11.0',note='Static build checks do not certify other macOS versions, physical Intel hardware or FL Studio versions.',checks=results),indent=2))
