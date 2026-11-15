from pathlib import Path
import shutil,subprocess,json,re,os
root=Path(__file__).resolve().parents[1]
resources=root/'BridgeLab.app/Contents/Resources';payload=resources/'Engine'
payload.mkdir(parents=True,exist_ok=True)
for name in ['project-root.txt','tools','build']:
 old=resources/name
 if old.exists():
  # Move obsolete controller resources to development backup, not the shipped app.
  dest=root/'build'/('obsolete-app-'+name.replace('.','-'))
  if dest.exists():shutil.rmtree(dest) if dest.is_dir() else dest.unlink()
  shutil.move(str(old),str(dest))
(payload/'bin').mkdir(exist_ok=True);(payload/'runtime').mkdir(exist_ok=True)
for arch in ['x86','x64']:shutil.copy2(root/'build'/f'runner-{arch}-v14.exe',payload/'bin'/f'runner-{arch}.exe')
for name in ['Wine Crossover.app','Wine Devel.app']:
 target=payload/'runtime'/name
 if not target.exists():subprocess.run(['/bin/cp','-cR',str(root/'runtime'/name),str(target)],check=True)
template=payload/'BridgeLabInstrument.vst3'
if template.exists():shutil.rmtree(template)
shutil.copytree(root/'BridgeLabInstrument.vst3',template,symlinks=True)
meta=template/'Contents/Resources/moduleinfo.json'
meta.write_text(json.dumps(json.loads(re.sub(r',\s*([}\]])',r'\1',meta.read_text())),indent=2))
for target in [meta,template]:subprocess.run(['/usr/bin/codesign','--force','--sign','-',str(target)],check=True,stdout=subprocess.DEVNULL)
shutil.copy2(root/'build/vst3/libBridgeLabUI.dylib',payload/'libBridgeLabUI.dylib')
subprocess.run(['/usr/bin/codesign','--force','--sign','-',str(payload/'libBridgeLabUI.dylib')],check=True)
(payload/'version.txt').write_text('0.9.2\n')
shutil.copy2(root/'THIRD_PARTY_VST3_LICENSE.txt',payload/'THIRD_PARTY_VST3_LICENSE.txt')
print('Bundled portable engine; no prefixes or development paths included')
