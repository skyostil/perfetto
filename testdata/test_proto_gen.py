from __future__ import print_function
import sys
import os
import subprocess
import tempfile

def test_command(*args):
  exit_code = subprocess.call(args, stdout=sys.stdout, stderr=sys.stderr)
  if exit_code != 0:
    assert False, 'Error running command: ' + ' '.join(args)

if __name__ == '__main__':
  format_proto_gen_path = sys.argv[1]
  protoc_path = sys.argv[2]
  format_path = sys.argv[3]
  tmpdir = tempfile.mkdtemp()
  proto_path = os.path.join('format.proto')
  test_command(format_proto_gen_path, format_path, proto_path)
  test_command(protoc_path, proto_path, '--cpp_out='+tmpdir)
