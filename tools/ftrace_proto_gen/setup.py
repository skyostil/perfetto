from distutils.core import setup, Extension
import os
os.environ["CC"] = "clang++"
os.environ["CXX"] = "clang++"
os.environ["CFLAGS"] = "-std=c++11"

extension_mod = Extension("_swigdemo", ["swig.cc", "ftrace_to_proto.cc"], language = "c++")

setup(name = "swigdemo", ext_modules=[extension_mod])
