"""Keep the ZIP writer aligned with the Hub's one-app archive contract."""
import importlib.util
import os
from pathlib import Path
import stat
import tempfile
import unittest
import zipfile

spec = importlib.util.spec_from_file_location('release_package', Path(__file__).resolve().parents[1] / 'tools/package-release.py')
release_package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release_package)


class ReleasePackageTests(unittest.TestCase):
    def test_one_app_root_preserves_executable_and_relative_links(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            app = root / 'BridgeLab.app'
            executable = app / 'Contents/MacOS/BridgeLab'
            executable.parent.mkdir(parents=True)
            executable.write_bytes(b'fixture')
            executable.chmod(0o755)
            (app / 'Contents/current').symlink_to('MacOS/BridgeLab')
            (app / '.DS_Store').write_bytes(b'Finder metadata')
            archive = root / 'release.zip'
            release_package.write_archive(app, archive)
            with zipfile.ZipFile(archive) as package:
                self.assertEqual(set(package.namelist()), {
                    'BridgeLab.app/Contents/MacOS/BridgeLab',
                    'BridgeLab.app/Contents/current',
                })
                item = package.getinfo('BridgeLab.app/Contents/MacOS/BridgeLab')
                self.assertTrue((item.external_attr >> 16) & stat.S_IXUSR)
                link = package.getinfo('BridgeLab.app/Contents/current')
                self.assertEqual(stat.S_IFMT(link.external_attr >> 16), stat.S_IFLNK)
                self.assertEqual(link.compress_type, zipfile.ZIP_STORED)
                self.assertEqual(package.read(link), b'MacOS/BridgeLab')

    def test_external_link_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            app = root / 'BridgeLab.app'
            app.mkdir()
            outside = root / 'outside'
            outside.write_bytes(b'private')
            (app / 'escape').symlink_to('../outside')
            with self.assertRaisesRegex(ValueError, 'Link escapes'):
                release_package.write_archive(app, root / 'release.zip')


if __name__ == '__main__':
    unittest.main()
