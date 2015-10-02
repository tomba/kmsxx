%module pykms
%{
#include "kms++.h"

#include "kmstest.h"

using namespace kms;
%}

%include "std_string.i"
%include "stdint.i"

%include "decls.h"
%include "drmobject.h"
%include "atomicreq.h"
%include "crtc.h"
%include "card.h"
%include "property.h"
%include "framebuffer.h"
%include "dumbframebuffer.h"
%include "plane.h"
%include "connector.h"
%include "encoder.h"

%include "kmstest.h"
