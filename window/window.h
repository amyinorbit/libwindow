/*===--------------------------------------------------------------------------------------------===
 * window.h - Window manager for X-Plane
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
#ifndef _WINDOW_H_
#define _WINDOW_H_

#include <XPLMDisplay.h>
#include <XPLMUtilities.h>
#include <acfutils/geom.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct window_t window_t;

typedef enum {
    WINDOW_MOUSE_DOWN,
    WINDOW_MOUSE_MOVE,
    WINDOW_MOUSE_UP
} mouse_action_t;

typedef void (*window_draw_f)(window_t *window, vect2_t pos, vect2_t size, void *refcon);
typedef int (*window_click_f)(window_t *window, mouse_action_t act, vect2_t pos, vect2_t scale, void *refcon);
typedef void (*window_key_f)(window_t *window, int key, char c, bool ctrl, void *refcon);

typedef struct {
    vect2_t         size;
    double          min_scale;
    double          max_scale;
    
    char            name[64];
    char            id[64];
    bool            is_decorated;
    bool            is_aspect_cstr;
    window_draw_f   draw;
    window_click_f  click;
    window_key_f    key;
} window_conf_t;

void window_sys_init(const char *assets_dir, const char *output_dir);
void window_sys_save(void);
void window_sys_restore(void);
void window_sys_fini(void);

void window_sys_move_to_vr(void);
void window_sys_move_to_2d(void);

window_t *window_new(const window_conf_t *conf, void *refcon);
void window_destroy(window_t *window);

XPLMCommandRef window_bind_cmd(window_t *window, const char *cmd);
XPLMCommandRef window_bind_cmd2(window_t *window, const char *cmd, const char *desc);
void window_bind_cmd3(window_t *window, XPLMCommandRef cmd);
void window_unbind_cmd(window_t *window);

void window_toggle(window_t *window);
void window_show(window_t *window);
void window_pop_out(window_t *window);
void window_hide(window_t *window);
bool window_is_visible(const window_t *window);
bool window_is_popped_out(const window_t *window);

vect2_t window_get_size(const window_t *window);


vect2_t window_desk2win(const window_t *window, vect2_t pos);
vect2_t window_win2desk(const window_t *window, vect2_t pos);

#ifdef __cplusplus
}
#endif

#endif /* ifndef _WINDOW_H_ */

