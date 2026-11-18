#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Source archives must retain the reviewed tree without requiring Git metadata."""
import importlib.util,json,os,tempfile,unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location('integrity',Path(__file__).resolve().parents[1]/'scripts/source_integrity.py')
integrity=importlib.util.module_from_spec(spec);spec.loader.exec_module(integrity)
class Sources(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name);self.original=integrity.ROOT;integrity.ROOT=self.root
  self.src=self.root/'vendor/fixture';self.src.mkdir(parents=True);(self.root/'data').mkdir();(self.src/'program.cpp').write_text('int main() {}\n');(self.src/'gradlew.bat').write_bytes(b'@echo off\r\n');self.save()
 def tearDown(self):integrity.ROOT=self.original;self.temp.cleanup()
 def save(self):
  sha,count=integrity.tree_digest(self.src);(self.root/'data/source-pins.json').write_text(json.dumps({'fixture':{'commit':'reviewed','sha256':sha,'files':count}}))
 def test_archive_verifies_without_git(self):self.assertEqual(integrity.verify('fixture','reviewed'),self.src)
 def test_modified_or_extra_source_is_rejected(self):
  (self.src/'program.cpp').write_text('modified');self.assertRaises(RuntimeError,integrity.verify,'fixture','reviewed');self.save();(self.src/'extra.cpp').write_text('unexpected');self.assertRaises(RuntimeError,integrity.verify,'fixture','reviewed')
 def test_windows_line_endings_follow_upstream_attributes(self):
  (self.src/'gradlew.bat').write_bytes(b'@echo off\n');integrity.verify('fixture','reviewed')
 def test_executable_mode_is_part_of_the_digest(self):
  os.chmod(self.src/'program.cpp',0o755);self.assertRaises(RuntimeError,integrity.verify,'fixture','reviewed')
 def test_symlinks_are_hashed_without_following_them(self):
  (self.src/'link').symlink_to('/not/a/source/path');self.save();integrity.verify('fixture','reviewed');(self.src/'link').unlink();(self.src/'link').symlink_to('/different');self.assertRaises(RuntimeError,integrity.verify,'fixture','reviewed')
 def test_wrong_pin_is_rejected(self):self.assertRaises(RuntimeError,integrity.verify,'fixture','other-revision')
if __name__=='__main__':unittest.main()
