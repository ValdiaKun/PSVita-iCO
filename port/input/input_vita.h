/*
 * port/input/input_vita.h
 *
 * Native Vita controller sampling. The host loop should call update once per
 * vsync after init; shutdown detaches the live pad from the game's libpad shim.
 */
#ifndef ICO_PORT_INPUT_INPUT_VITA_H
#define ICO_PORT_INPUT_INPUT_VITA_H

int ico_input_vita_init(void);
int ico_input_vita_update(void);
void ico_input_vita_shutdown(void);

#endif /* ICO_PORT_INPUT_INPUT_VITA_H */
