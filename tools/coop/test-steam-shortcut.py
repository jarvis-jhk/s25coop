#!/usr/bin/env python3
# Copyright (C) 2026 s25coop contributors
# SPDX-License-Identifier: GPL-2.0-or-later
"""Run the real installer/helper in isolated Steam homes; no Steam or internet access."""
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tarfile
import tempfile
import unittest
import zlib

TOOLS = Path(__file__).resolve().parent
NAME = 'The Settlers II: Coop'
SUFFIXES = ('p.png', '.jpg', '_hero.png', '_logo.png', '_icon.png')
SCRATCH = Path(os.environ.get('S25COOP_TEST_TMPDIR', str(Path.cwd() / 'build')))


def text(key, value):
    return b'\x01' + key.encode() + b'\0' + value.encode() + b'\0'


def shortcut(index, appid, title, exe, options='', extra=b''):
    # Put the executable path in an earlier field too: a byte search must not select it.
    return (b'\0' + str(index).encode() + b'\0\x02appid\0' + struct.pack('<I', appid)
            + text('LaunchOptions', options) + text('appname', title) + text('exe', '"' + exe + '"')
            + text('StartDir', '"/kept working directory"') + text('icon', '/old/icon.png')
            + b'\x02AllowOverlay\0\x01\0\0\0\x00tags\0' + text('0', 'kept tag') + b'\x08'
            + extra + b'\x08')


def vdf(*entries):
    return b'\0shortcuts\0' + b''.join(entries) + b'\x08\x08'


class SteamFixture(unittest.TestCase):
    def setUp(self):
        # Keep scratch under the checkout; /tmp may be a different filesystem.
        self.temp = tempfile.TemporaryDirectory(prefix='steam-fixture-', dir=SCRATCH)
        self.root = Path(self.temp.name)
        self.home = self.root / 'home with spaces'
        self.base = self.home / '.local/share/s25coop'
        self.game = self.base / 'game'
        self.game.mkdir(parents=True)
        self.config = self.home / '.steam/steam/userdata/17/config'
        self.config.mkdir(parents=True)
        self.file = self.config / 'shortcuts.vdf'
        self.exe = str(self.base / 'install.sh')
        self.bin = self.root / 'bin'
        self.bin.mkdir()
        self.env = dict(os.environ, HOME=str(self.home), XDG_DATA_HOME=str(self.home / '.local/share'),
                        PATH=str(self.bin) + ':' + os.environ['PATH'], S25COOP_NO_REPORTS='1',
                        STUB_ROOT=str(self.root))
        self.stub('pgrep', 'if [ -f "$STUB_ROOT/unknown" ]; then exit 2; fi\n[ -f "$STUB_ROOT/running" ]')
        self.stub('steam', 'echo "$*" >> "$STUB_ROOT/steam.log"\nif [ "${1:-}" = -shutdown ] && [ ! -f "$STUB_ROOT/stuck" ]; then rm -f "$STUB_ROOT/running"; fi')
        self.stub('sleep', 'exit 0')
        self.stub('zenity', 'echo "$*" >> "$STUB_ROOT/dialog.log"\ncase "$*" in *"Install?"*) exit 0;; *"Add s25coop to Steam"*) [ -f "$STUB_ROOT/allow-add" ];; *--question*) exit 1;; *) exit 0;; esac')
        self.seed_artwork(self.config, 0x89ABCDEF)
        self.original = shortcut(3, 0x89ABCDEF, 's25coop', self.exe, '--kept options')
        self.unrelated = shortcut(8, 0x81234567, NAME, '/other/game', self.exe)
        self.file.write_bytes(vdf(self.unrelated, self.original))
        self.prepared = False

    def tearDown(self):
        self.temp.cleanup()

    def stub(self, name, body):
        file = self.bin / name
        file.write_text('#!/bin/bash\n' + body + '\n')
        file.chmod(0o755)

    def seed_artwork(self, config, appid):
        grid = config / 'grid'
        grid.mkdir(exist_ok=True)
        for suffix in SUFFIXES:
            (grid / (str(appid) + suffix)).write_bytes(b'kept artwork')

    def helper(self, mode=None, expected=0):
        # Missing artwork is tested too, without allowing an accidental internet request.
        isolated = ('import runpy, sys, urllib.request; '
                    'urllib.request.urlopen=lambda *a, **k: '
                    '(_ for _ in ()).throw(RuntimeError("fixture network blocked")); '
                    'script=sys.argv.pop(1); runpy.run_path(script, run_name="__main__")')
        args = [sys.executable, '-c', isolated, str(TOOLS / 'add-to-steam.py')]
        if mode:
            args.append(mode)
        args.extend([NAME, self.exe])
        if not mode:
            args.extend([str(self.base), 'run'])
        result = subprocess.run(args, env=self.env, capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
        return result

    def expected_renamed(self, config=None):
        config = config or self.config
        icon = str(config / 'grid' / (str(0x89ABCDEF) + '_icon.png'))
        return self.original.replace(text('appname', 's25coop'), text('appname', NAME)).replace(
            text('icon', '/old/icon.png'), text('icon', icon))

    def assert_renamed(self):
        self.assertEqual(self.file.read_bytes(), vdf(self.unrelated, self.expected_renamed()))
        self.assertEqual((self.config / 'shortcuts.vdf.s25coop-backup').read_bytes(),
                         vdf(self.unrelated, self.original))
        for suffix in SUFFIXES:
            self.assertEqual((self.config / 'grid' / (str(0x89ABCDEF) + suffix)).read_bytes(), b'kept artwork')

    def prepare_installer(self):
        if self.prepared:
            return
        self.prepared = True
        self.payload = self.root / 'payload/s25coop'
        self.payload.mkdir(parents=True)
        shutil.copyfile(TOOLS / 'install.sh', self.payload / 'install.sh')
        shutil.copyfile(TOOLS / 'add-to-steam.py', self.payload / 'add-to-steam.py')
        (self.payload / 'VERSION').write_text('1.2.3')
        (self.payload / 's25coop.sh').write_text('#!/bin/bash\nprintf "%s\\n" "$*" > "$STUB_ROOT/launched"\n')
        (self.payload / 's25coop.sh').chmod(0o755)
        archive = self.root / 'release.tar.gz'
        with tarfile.open(archive, 'w:gz') as out:
            out.add(self.payload, arcname='s25coop')
        self.stub('curl', '''if [ "${1:-}" = -fsL ]; then
  echo '{"tag_name":"v1.2.3","assets":[{"browser_download_url":"https://fixture.invalid/game-linux-x86_64.tar.gz"}]}'
elif [ "${1:-}" = -fL ]; then
  while [ "$1" != -o ]; do shift; done
  cp "$STUB_ROOT/release.tar.gz" "$2"
else
  cat > "$STUB_ROOT/report.log"
fi''')
        for folder in ('DATA', 'GFX'):
            (self.base / 'S2' / folder).mkdir(parents=True)

    def installer(self, run=False):
        self.prepare_installer()
        script = self.base / 'install.sh'
        shutil.copyfile(TOOLS / 'install.sh', script)
        args = ['/bin/bash', str(script)]
        if run:
            args.extend(['run', '--test-launch-argument'])
        result = subprocess.run(args, env=self.env, capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def test_exact_entry_and_byte_preservation(self):
        # Desired name in an unrelated entry must not hide the old name; options containing
        # our exe must not select that unrelated entry either.
        self.assertEqual(self.helper('--status').stdout.strip(), 'old')
        self.helper('--migrate-name')
        self.assert_renamed()
        once = self.file.read_bytes()
        self.helper('--migrate-name')
        self.assertEqual(self.file.read_bytes(), once)
        self.assertEqual(self.helper('--status').stdout.strip(), 'current')

    def test_add_mode_matches_executable_field_only(self):
        self.helper()
        self.assert_renamed()

    def test_add_mode_refuses_running_steam(self):
        before = self.file.read_bytes()
        (self.root / 'running').touch()
        self.helper(None, 1)
        self.assertEqual(self.file.read_bytes(), before)

    def test_live_steam_guard_and_artwork_only(self):
        original = self.file.read_bytes()
        (self.root / 'running').touch()
        self.assertIn('Steam is running', self.helper('--migrate-name', 1).stderr)
        self.helper(None, 1)
        self.helper('--artwork-only')
        self.assertEqual(self.file.read_bytes(), original)
        self.assertFalse((self.config / 'shortcuts.vdf.s25coop-backup').exists())
        (self.root / 'running').unlink()
        (self.root / 'unknown').touch()
        self.helper('--migrate-name', 1)
        self.assertEqual(self.file.read_bytes(), original)

    def test_malformed_and_ambiguous_files_are_untouched(self):
        variants = [b'not a VDF', self.file.read_bytes()[:-1],
                    vdf(self.original, shortcut(9, 123, 'duplicate', self.exe))]
        for data in variants:
            with self.subTest(data=data):
                self.file.write_bytes(data)
                self.helper('--migrate-name', 1)
                self.assertEqual(self.file.read_bytes(), data)
                self.assertFalse((self.config / 'shortcuts.vdf.s25coop-backup').exists())

    def test_validates_all_users_before_writing(self):
        second = self.home / '.steam/steam/userdata/18/config'
        second.mkdir(parents=True)
        (second / 'shortcuts.vdf').write_bytes(b'invalid')
        before = self.file.read_bytes()
        self.helper('--migrate-name', 1)
        self.assertEqual(self.file.read_bytes(), before)
        (second / 'shortcuts.vdf').write_bytes(vdf(self.original))
        self.seed_artwork(second, 0x89ABCDEF)
        self.helper('--migrate-name')
        self.assert_renamed()
        self.assertEqual((second / 'shortcuts.vdf').read_bytes(), vdf(self.expected_renamed(second)))

    def test_migration_does_not_create_missing_entries(self):
        self.file.write_bytes(vdf(self.unrelated))
        second = self.home / '.steam/steam/userdata/18/config'
        second.mkdir(parents=True)
        before = self.file.read_bytes()
        self.helper('--migrate-name')
        self.assertEqual(self.file.read_bytes(), before)
        self.assertFalse((second / 'shortcuts.vdf').exists())
        self.assertEqual(self.helper('--status').stdout.strip(), 'missing')

    def test_name_migration_never_waits_for_artwork(self):
        shutil.rmtree(self.config / 'grid')
        self.assertEqual(self.helper('--migrate-name').stderr, '')
        once = self.file.read_bytes()
        self.assertIn(text('appname', NAME), once)
        self.assertFalse((self.config / 'grid').exists())
        (self.root / 'running').touch()
        result = self.helper('--artwork-only')
        self.assertIn('fixture network blocked', result.stderr)
        self.assertEqual(self.file.read_bytes(), once)

    def test_adds_new_entry_without_touching_other_games(self):
        self.file.write_bytes(vdf(self.unrelated))
        new_appid = zlib.crc32((self.exe + NAME).encode()) | 0x80000000
        self.seed_artwork(self.config, new_appid)
        self.helper()
        data = self.file.read_bytes()
        self.assertIn(self.unrelated, data)
        self.assertIn(text('AppName', NAME), data)
        self.assertIn(b'\x02appid\0' + struct.pack('<I', new_appid), data)
        self.assertEqual(data.count(text('Exe', '"' + self.exe + '"')), 1)
        self.helper()
        self.assertEqual(self.file.read_bytes(), data)
        self.file.unlink()
        self.helper()
        self.assertIn(text('AppName', NAME), self.file.read_bytes())

    def test_installer_repairs_existing_entry_without_rename_question(self):
        (self.root / 'running').touch()
        self.installer()
        self.assert_renamed()
        desktop = self.home / '.local/share/applications/s25coop.desktop'
        self.assertIn('Name=' + NAME + '\n', desktop.read_text())
        dialogs = (self.root / 'dialog.log').read_text()
        self.assertNotIn('Give the s25coop entry', dialogs)
        self.assertNotIn('Add s25coop to Steam', dialogs)
        self.assertIn('-shutdown', (self.root / 'steam.log').read_text())
        self.installer()
        self.assert_renamed()

    def test_installer_refuses_to_write_if_steam_stays_running(self):
        (self.root / 'running').touch()
        (self.root / 'stuck').touch()
        before = self.file.read_bytes()
        self.assertIn('Steam did not close', self.installer().stdout)
        self.assertEqual(self.file.read_bytes(), before)
        self.assertFalse((self.config / 'shortcuts.vdf.s25coop-backup').exists())

    def test_first_add_remains_optional(self):
        self.file.write_bytes(vdf(self.unrelated))
        before = self.file.read_bytes()
        self.installer()
        self.assertEqual(self.file.read_bytes(), before)
        (self.root / 'allow-add').touch()
        new_appid = zlib.crc32((self.exe + NAME).encode()) | 0x80000000
        self.seed_artwork(self.config, new_appid)
        self.installer()
        self.assertIn(text('AppName', NAME), self.file.read_bytes())
        self.assertIn(self.unrelated, self.file.read_bytes())

    def test_updated_desktop_launch_repairs_names_without_stopping_steam(self):
        self.installer(run=True)
        self.assert_renamed()
        self.assertEqual((self.root / 'launched').read_text().strip(), '--test-launch-argument')
        self.assertFalse((self.root / 'steam.log').exists())
        self.assertIn('Name=' + NAME + '\n',
                      (self.home / '.local/share/applications/s25coop.desktop').read_text())

    def test_game_mode_update_preserves_live_shortcuts(self):
        (self.root / 'running').touch()
        before = self.file.read_bytes()
        self.installer(run=True)
        self.assertEqual(self.file.read_bytes(), before)
        self.assertEqual((self.root / 'launched').read_text().strip(), '--test-launch-argument')
        self.assertFalse((self.root / 'steam.log').exists())
        self.assertFalse((self.config / 'shortcuts.vdf.s25coop-backup').exists())

    def test_malformed_launcher_migration_reports_and_still_launches(self):
        self.file.write_bytes(b'invalid')
        self.env['S25COOP_NO_REPORTS'] = ''
        self.installer(run=True)
        self.assertEqual(self.file.read_bytes(), b'invalid')
        self.assertTrue((self.root / 'launched').exists())
        self.assertIn('Steam shortcuts:', (self.root / 'report.log').read_text())


if __name__ == '__main__':
    SCRATCH.mkdir(parents=True, exist_ok=True)
    unittest.main()
