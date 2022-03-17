/*===--------------------------------------------------------------------------------------------===
 * capture.h - Capture the XP panel texture to generate avionics popups.
 *
 * Created by Amy Parent <amy@amyparent.com>
 * Copyright (c) 2022 Amy Parent. All rights reserved
 *
 * NOTICE:  All information contained herein is, and remains the property
 * of Amy Alex Parent. The intellectual and technical concepts contained
 * herein are proprietary to Amy Alex Parent and may be covered by U.S. and
 * Foreign Patents, patents in process, and are protected by trade secret
 * or copyright law. Dissemination of this information or reproduction of
 * this material is strictly forbidden unless prior written permission is
 * obtained from Amy Alex Parent.
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

