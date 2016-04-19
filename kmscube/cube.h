#pragma once

extern bool s_verbose;
extern bool s_fullscreen;
extern unsigned s_num_frames;

void main_null();
void main_gbm();
void main_x11();
void main_wl();

