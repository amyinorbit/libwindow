/*===--------------------------------------------------------------------------------------------===
 * capture.h
 *
 * Created by Amy Parent <amy@amyparent.com>
 * Copyright (c) 2024 Amy Parent
 *
 * Licensed under the MIT License
 *===--------------------------------------------------------------------------------------------===
*/
#ifndef _CAPTURE_H_
#define _CAPTURE_H_
#include <window/window.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct panel_cap_t panel_cap_t;

panel_cap_t *panel_cap_new(vect2_t size);
void panel_cap_destroy(panel_cap_t *cap);

window_t *panel_cap_add_window(panel_cap_t *cap, const char *name, const char *id, vect2_t pos, vect2_t size);
void panel_cap_update(panel_cap_t *cap);

#ifdef __cplusplus
}
#endif


#endif /* ifndef _CAPTURE_H_ */

