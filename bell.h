#ifndef BELL_H
#define BELL_H
#include "appstate.h"

void toggle_bell_type(haywire_state *app);
void toggle_bell_level(haywire_state *app);
void ring_bells(haywire_state *app);
void bell_status_print(haywire_state *app);
#endif
