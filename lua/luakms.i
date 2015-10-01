%module libluakms
%{
#include "kms++.h"

#include "testpat.h"

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
%include "plane.h"
%include "connector.h"
%include "encoder.h"

%include "testpat.h"
