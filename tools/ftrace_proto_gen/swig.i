%module swigdemo


%{
#include <stdlib.h>
#include <stdint.h>
#include <iosfwd>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>
#include "ftrace_to_proto.h"
%}

%include "std_string.i"
%include "std_vector.i"
namespace std {
  %template(vectoreee) vector<perfetto::FtraceEventField>;
};


%include "ftrace_to_proto.h"

