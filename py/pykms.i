%module(directors="1") pykms
%{
#include "kms++.h"

#include "kmstest.h"

using namespace kms;
%}

%include "std_string.i"
%include "stdint.i"
%include "std_vector.i"
%include "std_map.i"

%feature("director") PageFlipHandlerBase;

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
%include "pagefliphandler.h"
%include "videomode.h"

%include "color.h"
%include "kmstest.h"

%template(ConnectorVector) std::vector<kms::Connector*>;
%template(CrtcVector) std::vector<kms::Crtc*>;
%template(EncoderVector) std::vector<kms::Encoder*>;
%template(PlaneVector) std::vector<kms::Plane*>;
%template(VideoModeVector) std::vector<kms::Videomode>;
/* for some reason uint64_t doesn't compile on 64 bit pc */
/* %template(map_u32_u64) std::map<uint32_t, uint64_t>; */
%template(map_u32_u64) std::map<uint32_t, unsigned long long>;
