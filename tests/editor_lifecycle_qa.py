"""Actual Windows editor close/reopen and runner lifetime checks."""
from pathlib import Path
import importlib.util,json,mmap,os,struct,tempfile,time,subprocess
spec=importlib.util.spec_from_file_location('editor',Path(__file__).with_name('installed_editor.py'))
e=importlib.util.module_from_spec(spec);spec.loader.exec_module(e)
rows=json.loads((e.HOME/'plugins.json').read_text());results=[]
for row in rows:
 s=e.Session(row)
 fd,path=tempfile.mkstemp(prefix='BridgeLab-lifecycle-',suffix='.frame');os.ftruncate(fd,e.MAP_BYTES);mapping=mmap.mmap(fd,e.MAP_BYTES);os.close(fd)
 try:
  _,before=s.request(3)
  initial_resources=struct.unpack('<4I',s.request(12)[1]);assert initial_resources[:3]==(0,0,0),'audio-only instance allocated an editor window or frame'
  assert s.request(6,('Z:'+path).encode())[0][2]==1
  s.pump(30)
  opened=struct.unpack('<4I',s.request(11)[1]);assert opened[0:2]==(1,1)
  opened_resources=struct.unpack('<4I',s.request(12)[1]);assert opened_resources[:3]==(1,1,e.MAP_BYTES)
  assert s.request(2)[0][2]==1
  closed=struct.unpack('<4I',s.request(11)[1]);assert closed[0:2]==(0,0)
  closed_resources=struct.unpack('<4I',s.request(12)[1]);assert closed_resources[:3]==(0,0,0),'closing retained an editor host window or frame'
  s.pump(30)
  idle=struct.unpack('<4I',s.request(11)[1]);assert idle[2]==closed[2],'closed editor still receives idle callbacks'
  _,after=s.request(3);assert after==before,'close changed sound state'
  for _ in range(3):
   assert s.request(6,('Z:'+path).encode())[0][2]==1;s.pump(20)
   assert struct.unpack('<4I',s.request(11)[1])[0:2]==(1,1)
   assert s.request(2)[0][2]==1
  assert s.request(6,('Z:'+path).encode())[0][2]==1;s.pump(30)
  _,restored=s.request(3);assert restored==before,'reopening changed sound state'
  result=dict(plugin=row['name'],passed=True,audio_only_has_no_editor_resources=True,closed_host_window_destroyed=True,closed_gui_released=True,closed_frame_unmapped=True,closed_idle_callbacks_stopped=True,reopened_three_times=True,state_unchanged=True)
 finally:
  s.close();mapping.close();os.unlink(path)
 results.append(result)
sessions=[]
try:
 for row in [rows[0],rows[-1],rows[0]]:sessions.append(e.Session(row))
 pids=[s.p.pid for s in sessions]
 rss=[int(subprocess.check_output(['/bin/ps','-o','rss=','-p',str(pid)],text=True).strip()) for pid in pids]
 for s in sessions:s.close()
 sessions=[]
 for pid in pids:
  try:os.kill(pid,0)
  except ProcessLookupError:pass
  else:raise AssertionError('runner remains after instance destruction')
 results.append(dict(test='three simultaneous isolated runners exit on destruction',passed=True,runner_count=3,rss_kib=rss,rss_note='RSS includes shared pages; not unique physical RAM'))
finally:
 for s in sessions:s.close()
print(json.dumps(results,indent=2))
Path(os.environ['BRIDGELAB_QA_EDITOR_REPORT']).write_text(json.dumps(results,indent=2)+'\n')
