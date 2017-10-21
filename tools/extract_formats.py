#!/usr/bin/env python
# Copyright (C) 2017 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from __future__ import print_function
import os
import subprocess
import argparse
import datetime

"""Pulls all format files from an Android device.

Usage: ./tools/extract_formats.py [-s serial] [-p dir_prefix]
"""


def adb(*cmd, **kwargs):
  serial = kwargs.get('serial', None)
  prefix = ['adb']
  if serial:
    prefix += ['-s', serial]
  cmd = prefix + list(cmd)
  output = subprocess.check_output(cmd)
  return output


def get_devices():
  serials = [s.split('\t')[0] for s in adb('devices').split('\n')[1:] if s]
  return serials


def ensure_output_directory_empty(path):
  if os.path.isfile(path):
    print('The output directory {} exists as a file. Remove or choose a new directory'.format(path))
    exit(1)

  if os.path.isdir(path) and os.listdir(path):
    print('The output directory {} exists but is not empty.'.format(path))
    exit(1)

  if not os.path.isdir(path):
    os.makedirs(path)


def ensure_single_device(serial):
  serials = get_devices()
  if serial is None and len(serials) == 1:
    return serials[0]
  
  if serial in serials:
    return serial

  if not serials:
    print('No devices connected.')
  elif serial is None:
    print('More than one device connected, use -s.')
  else:
    print('No device with serial {} found.'.format(serial))
  exit(1)


def pull_format_files(serial, output_directory):
  # Pulling each file individually is 100x slower so we pipe all together then
  # split them on the host.
  cmd = 'find /sys/kernel/debug/tracing/events/*/*/format | while read f; do echo "path:" $f; cat $f; done'

  output = adb('shell', cmd, serial=serial)
  sections = output.split('path: /sys/kernel/debug/tracing/events/')
  for section in sections:
    if not section:
      continue
    path, rest = section.split('\n', 1)
    path = os.path.join(output_directory, path)
    os.makedirs(os.path.dirname(path))
    with open(path, 'wb') as f:
      f.write(rest)


# Produces output of the form: prefix_android_seed_N2F62_3.10.49
def get_output_directory(prefix=None):
  build_id = adb('shell', 'getprop', 'ro.build.id').replace('\n', '')
  product = adb('shell', 'getprop', 'ro.build.product').replace('\n', '')
  kernel = adb('shell', 'uname', '-r').split('-')[0]
  parts = ['android', product, build_id, kernel]
  if prefix:
    parts = [prefix] + parts
  return '_'.join(parts)


if __name__ == '__main__':
  parser = argparse.ArgumentParser(description='Pull format files.')
  parser.add_argument('-p', dest='prefix', default=None,
                      help='the output directory prefix')
  parser.add_argument('-s', dest='serial', default=None,
                      help='use device with the given serial')
  args = parser.parse_args()

  prefix = args.prefix
  serial = args.serial

  serial = ensure_single_device(serial)
  output_directory = get_output_directory(prefix)

  ensure_output_directory_empty(output_directory)
  pull_format_files(serial, output_directory)
