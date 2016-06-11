/*
 * Copyright (c) 2012 Arvin Schnell <arvin.schnell@gmail.com>
 * Copyright (c) 2012 Rob Clark <rob@ti.com>
 * Copyright (c) 2015 Tomi Valkeinen <tomi.valkeinen@ti.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/* Based on a egl cube test app originally written by Arvin Schnell */

#include "cube.h"
#include <kms++util/kms++util.h>

using namespace std;

bool s_verbose;
bool s_fullscreen;
unsigned s_num_frames;

int main(int argc, char *argv[])
{
	OptionSet optionset = {
		Option("v|verbose",
		[&]()
		{
			s_verbose = true;
		}),
		Option("f|fullscreen",
		[&]()
		{
			s_fullscreen = true;
		}),
		Option("n|numframes=",
		[&](string s)
		{
			s_num_frames = stoi(s);
		}),
	};

	optionset.parse(argc, argv);

	string m;

	if (optionset.params().size() == 0)
		m = "gbm";
	else
		m = optionset.params().front();

	if (m == "gbm")
		main_gbm();
	else if (m == "null")
		main_null();
	else if (m == "x")
		main_x11();
	else if (m == "wl")
		main_wl();
	else {
		printf("Unknown mode %s\n", m.c_str());
		return -1;
	}

	return 0;
}
