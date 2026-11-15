import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
import subprocess
import wave
spec = importlib.util.spec_from_file_location('bridge', Path(__file__).resolve().parents[1]/'tools/bridge.py')
b = importlib.util.module_from_spec(spec); spec.loader.exec_module(b)
class ImportTests(unittest.TestCase):
    def check_binary(self, machine, magic, bits):
        data=bytearray(512);data[:2]=b'MZ';struct.pack_into('<I',data,60,128)
        data[128:132]=b'PE\0\0';struct.pack_into('<HHIIIHH',data,132,machine,1,0,0,0,224,0x2000);struct.pack_into('<H',data,152,magic)
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/'test.dll';p.write_bytes(data)
            self.assertEqual(b.inspect(p)['bits'],bits)
            p.write_bytes(data[:160])
            with self.assertRaises(ValueError):b.inspect(p)
    def test_x86(self):self.check_binary(0x14c,0x10b,32)
    def test_x64(self):self.check_binary(0x8664,0x20b,64)
    def test_invalid(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/'bad.dll';p.write_bytes(b'not a plugin')
            with self.assertRaises(ValueError):b.inspect(p)
    def test_render_harness(self):
        root=Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as td:
            out=Path(td)/'test.wav'
            result=subprocess.run([str(root/'build/runner-native'),str(root/'build/fixture.dylib'),str(out)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            with wave.open(str(out)) as f:
                self.assertEqual((f.getnchannels(),f.getframerate(),f.getnframes()),(2,48000,144000))
                pcm=struct.unpack('<288000h',f.readframes(144000))
            self.assertGreater(max(pcm[:96000]),3000)
            self.assertEqual(max(abs(s) for s in pcm[192000:]),0)
    def test_bad_plugin_is_contained(self):
        root=Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as td:
            result=subprocess.run([str(root/'build/runner-native'),'/does/not/exist',str(Path(td)/'out.wav')],capture_output=True)
            self.assertEqual(result.returncode,1)
if __name__=='__main__':unittest.main()
