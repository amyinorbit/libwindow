/*===--------------------------------------------------------------------------------------------===
 * window.c - Window manager implementation
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
#include <window/window.h>

#include <acfutils/assert.h>
#include <acfutils/avl.h>
#include <acfutils/safe_alloc.h>
#include <acfutils/glew.h>
#include <acfutils/dr.h>
#include <acfutils/lacf_gl_pic.h>
#include <acfutils/conf.h>
#include <acfutils/time.h>
#include <acfutils/cursor.h>
#include <acfutils/widget.h>
#include <stddef.h>
#include <XPLMGraphics.h>

#define BUTTON_ASSET_SIZE (64)
#define BUTTON_SIZE (BUTTON_ASSET_SIZE/2.0)
#define BUTTON_HOVER_DELAY (2.0)
#define KEYBOARD_WIDTH (50)
#define KEYBOARD_HEIGHT BUTTON_SIZE
#define RESIZE_MARGIN (12)

struct window_t {
    XPLMWindowID        ref;
    XPLMCommandRef      cmd;
    
    bool                is_decorated;
    win_resize_ctl_t    resize_ctl;
    vect2_t             last_click;
    double              last_hover;
    
    window_conf_t       conf;
    avl_node_t          mgr_node;
    void                *refcon;
};

static struct {
    bool            is_init;
    
    char            *output_dir;
    avl_tree_t      windows;
    dr_t            dr_viewport;
    dr_t            dr_vr_enabled;
    
    // XPLMWindowID    overlay;
    
    cursor_t        *cursor;
    lacf_gl_pic_t   *close;
    lacf_gl_pic_t   *popout;
    lacf_gl_pic_t   *resize_l;
    lacf_gl_pic_t   *resize_r;
    lacf_gl_pic_t   *keyboard;
} sys = {};



static void save_window(const window_t *window, conf_t *conf) {
    
    bool visible = XPLMGetWindowIsVisible(window->ref);
    bool is_popped_out = XPLMWindowIsPoppedOut(window->ref);
    int left, top, right, bottom;
    
    if(!is_popped_out) {
        XPLMGetWindowGeometry(window->ref, &left, &top, &right, &bottom);
    } else {
        XPLMGetWindowGeometryOS(window->ref, &left, &top, &right, &bottom);
    }
    
    conf_set_i_v(conf, "%s/pos/left", left, window->conf.id);
    conf_set_i_v(conf, "%s/pos/right", right, window->conf.id);
    conf_set_i_v(conf, "%s/pos/top", top, window->conf.id);
    conf_set_i_v(conf, "%s/pos/bottom", bottom, window->conf.id);
    
    
    conf_set_b_v(conf, "%s/visible", visible, window->conf.id);
    conf_set_b_v(conf, "%s/popout", is_popped_out, window->conf.id);
}


static void restore_window(window_t *window, conf_t *conf) {
    int left = 0, top = 0, right = 0, bottom = 0;
    
    if(!conf_get_i_v(conf, "%s/pos/left", &left, window->conf.id)) return;
    if(!conf_get_i_v(conf, "%s/pos/right", &right, window->conf.id)) return;
    if(!conf_get_i_v(conf, "%s/pos/top", &top, window->conf.id)) return;
    if(!conf_get_i_v(conf, "%s/pos/bottom", &bottom, window->conf.id)) return;
    
    bool_t visible = false, is_popped_out = false;
    if(!conf_get_b_v(conf, "%s/visible", &visible, window->conf.id)) return;
    conf_get_b_v(conf, "%s/popout", &is_popped_out, window->conf.id);
    
    if(dr_geti(&sys.dr_vr_enabled) == 1) {
        XPLMSetWindowPositioningMode(window->ref, xplm_WindowVR, -1);
    } else if(is_popped_out) {
    	XPLMSetWindowPositioningMode(window->ref, xplm_WindowPopOut, -1);
        XPLMSetWindowGeometryOS(window->ref, left, top, right, bottom);
    } else {
        XPLMSetWindowGeometry(window->ref, left, top, right, bottom);
    }
    XPLMSetWindowIsVisible(window->ref, visible);
}

void window_sys_save() {
    VERIFY(sys.is_init);
    
    conf_t *conf = conf_create_empty();
    for(window_t *win = avl_first(&sys.windows); win != NULL; win = AVL_NEXT(&sys.windows, win)) {
        save_window(win, conf);
    }
    
    char *path = mkpathname(sys.output_dir, "windows.txt", NULL);
    if(!conf_write_file(conf, path)) {
        logMsg("could not save window positions to `%s`", path);
    }
    
    lacf_free(path);
    conf_free(conf);
}

void window_sys_restore() {
    VERIFY(sys.is_init);
    
    char *path = mkpathname(sys.output_dir, "windows.txt", NULL);
    if(!file_exists(path, NULL)) {
        lacf_free(path);
        return;
    }
    
    int errline = 0;
    conf_t *conf = conf_read_file(path, &errline);
    
    if(!conf) {
        logMsg("error in window positions file at line %d", errline);
        lacf_free(path);
        window_sys_save();
        return;
    }
    lacf_free(path);
    
    for(window_t *win = avl_first(&sys.windows); win != NULL; win = AVL_NEXT(&sys.windows, win)) {
        restore_window(win, conf);
    }
    conf_free(conf);
}

static bool is_out_of_bounds(const window_t *window) {
    
#define MARGIN 30
    
    int screen_w, screen_h;
    XPLMGetScreenSize(&screen_w, &screen_h);
    
    int left, top, right, bottom;
    XPLMGetWindowGeometry(window->ref, &left, &top, &right, &bottom);
    return left > screen_w - MARGIN || right < MARGIN || top > screen_h - MARGIN || bottom < MARGIN;
}

static bool is_in_button(const window_t *window, vect2_t button, vect2_t click) {
    return !window->conf.is_decorated
        && !XPLMWindowIsPoppedOut(window->ref) && !XPLMWindowIsInVR(window->ref)
        && click.x > button.x && click.x < button.x + BUTTON_SIZE
        && click.y > button.y && click.y < button.y + BUTTON_SIZE;
}

static vect2_t close_button_pos(const window_t *window) {
    int left, top, right, bottom;
    XPLMGetWindowGeometry(window->ref, &left, &top, &right, &bottom);
    return VECT2(left, top - BUTTON_SIZE);
}

static vect2_t popout_button_pos(const window_t *window) {
    int left, top, right, bottom;
    XPLMGetWindowGeometry(window->ref, &left, &top, &right, &bottom);
    return VECT2(right - BUTTON_SIZE, top - BUTTON_SIZE);
}

static void handle_key(XPLMWindowID id, char key, XPLMKeyFlags flags, char vkey, void *refcon, int lose) {
    UNUSED(id);
    UNUSED(key);
    UNUSED(flags);
    
    if(lose) return;
    if(!(flags & xplm_DownFlag)) return;
    window_t *window = refcon;
    if(window->conf.key) window->conf.key(window, (int)vkey, key, window->refcon);
}

static XPLMCursorStatus default_cursor(XPLMWindowID id, int x, int y, void *refcon) {
    UNUSED(id);
    UNUSED(x);
    UNUSED(y);
    
    window_t *window = refcon;
    if(!XPLMIsWindowInFront(window->ref)) return xplm_CursorDefault;
    window->last_hover = microclock();
        
    vect2_t pos = VECT2(x, y);
    if(is_in_button(window, close_button_pos(window), pos) || is_in_button(window, popout_button_pos(window), pos)) {
        cursor_make_current(sys.cursor);
        return xplm_CursorCustom;
    }
    
    return xplm_CursorDefault;
}

static int default_scroll(XPLMWindowID id, int x, int y, int wheel, int clicks, void *refcon) {
    UNUSED(id);
    UNUSED(x);
    UNUSED(y);
    UNUSED(wheel);
    UNUSED(clicks);
    UNUSED(refcon);
    return 1;
}

static int default_click(XPLMWindowID id, int x, int y, XPLMMouseStatus s, void *refcon) {
    UNUSED(id);
    UNUSED(x);
    UNUSED(y);
    UNUSED(s);
    UNUSED(refcon);
    return 1;
}


static int handle_click(XPLMWindowID id, int x, int y, XPLMMouseStatus status, void *refcon) {
    UNUSED(id);
    window_t *window = refcon;
    
    vect2_t click = VECT2(x, y);
    int left, top, right, bottom;
    XPLMGetWindowGeometry(window->ref, &left, &top, &right, &bottom);
    vect2_t click_win = window_desk2win(window, click);
    
    vect2_t size = VECT2(right - left, top - bottom);
    vect2_t scale = VECT2(size.x / window->conf.size.x, size.y / window->conf.size.y);
    
    vect2_t close_button = close_button_pos(window);
    vect2_t popout_button = popout_button_pos(window);
    
    switch(status) {
    case xplm_MouseDown:
        window->last_click = NULL_VECT2;
        
        if(window->conf.key && !XPLMHasKeyboardFocus(window->ref)) {
            XPLMTakeKeyboardFocus(window->ref);
        }
        
        if(!window->conf.is_decorated && is_in_button(window, close_button, click)) {
            window_hide(window);
            return 1;
        }
        
        if(!window->conf.is_decorated && is_in_button(window, popout_button, click)) {
            window_pop_out(window);
            return 1;
        }
        
        if(window->conf.click && window->conf.click(window, WINDOW_MOUSE_DOWN, click_win, scale, window->refcon)) {
            return 1;    
        } else if(
            !XPLMWindowIsPoppedOut(window->ref)
            && !XPLMWindowIsInVR(window->ref)
            && click.x > left + RESIZE_MARGIN
            && click.x < right - RESIZE_MARGIN
            && click.y > bottom + RESIZE_MARGIN
            && click.y < top - RESIZE_MARGIN
        ) {
            window->last_click = click;
            return 1;
        }
        break;
        
    case xplm_MouseDrag:
        if(!IS_NULL_VECT2(window->last_click)) {
            vect2_t diff = vect2_sub(click, window->last_click);
            window->last_click = click;
            XPLMSetWindowGeometry(window->ref, left + diff.x, top + diff.y, right + diff.x, bottom + diff.y);
            return 1;
        } else if(window->conf.click) {
            return window->conf.click(window, WINDOW_MOUSE_MOVE, click_win, scale, window->refcon);
        }
        break;
        
    case xplm_MouseUp:
        if(!IS_NULL_VECT2(window->last_click)) {
            window->last_click = NULL_VECT2;
            return 1;
        } else {
            if(window->conf.click) {
                window->conf.click(window, WINDOW_MOUSE_UP, click_win, scale, window->refcon);
            }
            window->last_click = NULL_VECT2;
            return 0;
        }
        break;
    }
    return 0;
}

static void handle_focus(window_t *window) {
    if(!window->conf.key) return;
    if(!XPLMIsWindowInFront(window->ref) && XPLMHasKeyboardFocus(window->ref)) {
        XPLMTakeKeyboardFocus(NULL);
    }
}

static void handle_draw(XPLMWindowID id, void *refcon) {
    UNUSED(id);
    window_t *window = refcon;
    
    if(window->conf.is_aspect_cstr) {
        win_resize_ctl_update(&window->resize_ctl);
    }
    
    if(!XPLMGetWindowIsVisible(window->ref)) return;
    
    handle_focus(window);
    
    int left, top, right, bottom;
    XPLMGetWindowGeometry(window->ref, &left, &top, &right, &bottom);
    
    bool is_in_sim_window = !XPLMWindowIsPoppedOut(window->ref) && !XPLMWindowIsInVR(window->ref);
    bool buttons_faded_in = microclock() - window->last_hover < BUTTON_HOVER_DELAY * 1e6;
    
    if(window->conf.draw) {
        window->conf.draw(
            window,
            VECT2(left, bottom),
            VECT2(right-left, top-bottom),
            window->refcon
        );
    }
    
    if(!window->conf.is_decorated && is_in_sim_window && buttons_faded_in) {
        lacf_gl_pic_draw(
            sys.close,
            VECT2(left, top - BUTTON_SIZE),
            VECT2(BUTTON_SIZE, BUTTON_SIZE),
            0.8
        );
        lacf_gl_pic_draw(
            sys.popout,
            VECT2(right - BUTTON_SIZE, top - BUTTON_SIZE),
            VECT2(BUTTON_SIZE, BUTTON_SIZE),
            0.8
        );
            
        lacf_gl_pic_draw(
            sys.resize_l,
            VECT2(left, bottom),
            VECT2(24, 24),
            0.5
        );
        lacf_gl_pic_draw(
            sys.resize_r,
            VECT2(right - 24, bottom),
            VECT2(24, 24),
            0.5
        );
    }
    
    if(!window->conf.is_decorated && is_in_sim_window) {
        float color[] = {1, 1, 1, 0.8};
        char title[128];
        lacf_strlcpy(title, window->conf.name, sizeof(title));

        double width = strlen(title) * 6;
        double height = 15;
        double x = (left + right) / 2.0;
        double x1 = x - (width/2.0);
        double x2 = x + (width/2.0);
        double y = bottom - 5;


        XPLMDrawTranslucentDarkBox(x1 - 5, y, x2 + 5, y - height);
        XPLMDrawString(color, x1, y - 10, title, NULL, xplmFont_Basic);
    }
    
    if(XPLMHasKeyboardFocus(window->ref)) {
        double t = microclock() / 1e6;
        
        lacf_gl_pic_draw(
            sys.keyboard,
            VECT2(left + BUTTON_SIZE, top - BUTTON_SIZE),
            VECT2(KEYBOARD_WIDTH, KEYBOARD_HEIGHT),
            0.5 + 0.5 * sin(t * 5)
        );
    }
}

static lacf_gl_pic_t *load_image(const char *dir, const char *name) {
    char *path = mkpathname(dir, "data", "images", name, NULL);
    lacf_gl_pic_t *pic = lacf_gl_pic_new(path);
    VERIFY3P(pic, !=, NULL);
    lacf_free(path);
    return pic;
}

static int window_cmp(const void *wp1, const void *wp2) {
    const window_t *w1 = wp1;
    const window_t *w2 = wp2;
    
    int result = strcmp(w1->conf.id, w2->conf.id);
    if(result < 0) return -1;
    if(result > 0) return 1;
    return 0;
}


void window_sys_init(const char *dir, const char *output_dir) {
    if(sys.is_init) return;
    fdr_find(&sys.dr_viewport, "sim/graphics/view/viewport");
    fdr_find(&sys.dr_vr_enabled, "sim/graphics/VR/enabled");
    
    sys.close = load_image(dir, "close.png");
    sys.popout = load_image(dir, "popout.png");
    sys.resize_l = load_image(dir, "resize_l.png");
    sys.resize_r = load_image(dir, "resize_r.png");
    sys.keyboard = load_image(dir, "keyboard.png");
    
    char *cursor_path = mkpathname(dir, "data", "cursors", "click.png", NULL);
    logMsg("Cursor path: %s", cursor_path);
    sys.cursor = cursor_read_from_file(cursor_path);
    VERIFY3P(sys.cursor, !=, NULL);
    
    avl_create(&sys.windows, window_cmp, sizeof(window_t), offsetof(window_t, mgr_node));
    sys.output_dir = safe_strdup(output_dir);
    
    sys.is_init = true;
}

static void window_destroy_private(window_t *window);

void window_sys_fini(void) {
    if(!sys.is_init) return;
    sys.is_init = false;
    
    window_t *window = NULL;
    void *cookie = NULL;
    while((window = avl_destroy_nodes(&sys.windows, &cookie)) != NULL) {
        window_destroy_private(window);
    }
    avl_destroy(&sys.windows);
    
    // XPLMDestroyWindow(sys.overlay);
    lacf_gl_pic_destroy(sys.close);
    lacf_gl_pic_destroy(sys.popout);
    lacf_gl_pic_destroy(sys.keyboard);
    cursor_free(sys.cursor);
    lacf_free(sys.output_dir);
    
    sys.close = NULL;
    sys.popout = NULL;
    sys.keyboard = NULL;
    sys.cursor = NULL;
    sys.output_dir = NULL;
}

void window_sys_move_to_vr(void) {
    if(!sys.is_init) return;
    for(window_t *win = avl_first(&sys.windows); win != NULL; win = AVL_NEXT(&sys.windows, win)) {
        XPLMSetWindowPositioningMode(win->ref, xplm_WindowVR, 0);
    }
}

void window_sys_move_to_2d(void) {
    if(!sys.is_init) return;
    for(window_t *win = avl_first(&sys.windows); win != NULL; win = AVL_NEXT(&sys.windows, win)) {
        XPLMSetWindowPositioningMode(win->ref, xplm_WindowPositionFree, -1);
    }
}


static void strreplace(char *str, const char *matches, char replace) {
    for(char *ptr = str; *ptr; ++ptr) {
        if(strchr(matches, *ptr) != NULL) {
            *ptr = replace;
        }
    }
}

window_t *window_new(const window_conf_t *conf, void *refcon) {
    ASSERT3P(conf, !=, NULL);
    
    window_t *window = safe_calloc(1, sizeof(*window));
    
    XPLMCreateWindow_t cfg = {};
    cfg.structSize = sizeof(cfg);
    cfg.visible = 0;
    cfg.drawWindowFunc = handle_draw;
    cfg.handleKeyFunc = handle_key;
    cfg.handleCursorFunc = default_cursor;
    cfg.handleMouseClickFunc = handle_click;
    cfg.handleRightClickFunc = default_click;
    cfg.handleMouseWheelFunc = default_scroll;
    cfg.left = 100;
    cfg.right = cfg.left + conf->size.x;
    cfg.bottom = 100;
    cfg.top = cfg.bottom + conf->size.y;
    cfg.decorateAsFloatingWindow = conf->is_decorated
        ? xplm_WindowDecorationRoundRectangle
        : xplm_WindowDecorationSelfDecoratedResizable;
    cfg.layer = xplm_WindowLayerFloatingWindows;
    cfg.refcon = window;
    
    window->ref = XPLMCreateWindowEx(&cfg);
    window->conf = *conf;
    window->refcon = refcon;
    
    if(!strlen(window->conf.id)) {
        lacf_strlcpy(window->conf.id, window->conf.name, sizeof(window->conf.id));
        strreplace(window->conf.id, " \t/\\~!@#$%^&*()-+=", '_');
    }
    
    if(conf->is_aspect_cstr) {
        win_resize_ctl_init(&window->resize_ctl, window->ref, conf->size.x, conf->size.y);
    }
    
    XPLMSetWindowResizingLimits(
        window->ref,
        conf->size.x * conf->min_scale,
        conf->size.y * conf->min_scale,
        conf->size.x * conf->max_scale,
        conf->size.y * conf->max_scale
    );
    XPLMSetWindowTitle(window->ref, conf->name);
    if(dr_geti(&sys.dr_vr_enabled) == 1) {
        XPLMSetWindowPositioningMode(window->ref, xplm_WindowVR, 0);
    }
    
    avl_add(&sys.windows, window);
    return window;
}

static void window_destroy_private(window_t *window) {
    ASSERT3P(window, !=, NULL);
    window_unbind_cmd(window);
    XPLMDestroyWindow(window->ref);
    lacf_free(window);
}

void window_destroy(window_t *window) {
    ASSERT3P(window, !=, NULL);
    avl_remove(&sys.windows, window);
    window_destroy_private(window);
}

static int window_cmd_handler(XPLMCommandRef cmd, XPLMCommandPhase phase, void *userdata) {
    UNUSED(cmd);
    UNUSED(phase);
    if(phase == xplm_CommandBegin) {
        window_t *window = userdata;
        window_toggle(window);
    }
    return 1;
}

void window_unbind_cmd(window_t *window) {
    ASSERT3P(window, !=, NULL);
    if(!window->cmd) return;
    XPLMUnregisterCommandHandler(window->cmd, window_cmd_handler, 0, window);
}

XPLMCommandRef window_bind_cmd2(window_t *window, const char *cmd, const char *desc) {
    ASSERT3P(window, !=, NULL);
    window->cmd = XPLMCreateCommand(cmd, desc);
    if(window->cmd)
        XPLMRegisterCommandHandler(window->cmd, window_cmd_handler, 0, window);
    return window->cmd;
}

XPLMCommandRef window_bind_cmd(window_t *window, const char *cmd) {
    ASSERT3P(window, !=, NULL);
    window->cmd = XPLMFindCommand(cmd);
    if(window->cmd)
        XPLMRegisterCommandHandler(window->cmd, window_cmd_handler, 0, window);
    return window->cmd;
}

void window_pop_out(window_t *window) {
    ASSERT3P(window, !=, NULL);
	XPLMSetWindowPositioningMode(window->ref, xplm_WindowPopOut, -1);
}

void window_toggle(window_t *window) {
    ASSERT3P(window, !=, NULL);
    if(window_is_visible(window)) {
        window_hide(window);
    } else {
        window_show(window);
    }
}

static void window_center_mouse(window_t *window, int w, int h) {
    int x, y;
    XPLMGetMouseLocationGlobal(&x, &y);
    XPLMSetWindowGeometry(window->ref, x - w/2.0, y + h/2.0, x + w/2.0, y - h/2.0);
}

void window_show(window_t *window) {
    ASSERT3P(window, !=, NULL);
    
    if(XPLMWindowIsPoppedOut(window->ref)) {
        XPLMSetWindowPositioningMode(window->ref, xplm_WindowPositionFree, 0);
    }
    XPLMSetWindowIsVisible(window->ref, 1);
    if(window->conf.key) XPLMTakeKeyboardFocus(window->ref);
    
    // Check that the window is within the visible area, move it if not!
    
    if(XPLMWindowIsPoppedOut(window->ref)) return;
    
    if(is_out_of_bounds(window)) {
        int left, top, right, bottom;
        XPLMGetWindowGeometry(window->ref, &left, &top, &right, &bottom);
        window_center_mouse(window, right - left, top - bottom);
    }
}

void window_hide(window_t *window) {
    ASSERT3P(window, !=, NULL);
    XPLMSetWindowIsVisible(window->ref, 0);
    if(XPLMHasKeyboardFocus(window->ref)) XPLMTakeKeyboardFocus(NULL);
}

bool window_is_visible(const window_t *window) {
    ASSERT3P(window, !=, NULL);
    return XPLMGetWindowIsVisible(window->ref);
}

// We want to convert from X-plane's global coordinate system to a classic, -y window space
//
//      y    +---------->+ top
//      ^    |    |win.y |
// out.y|----|----x      | height
//      |    | win.x     |
//      |    v    |      |
//      |    +-----------+ bottom
//      |  left  width  right
//      |         |
//      |         |
//      +---------|----------> x
//                out.x

vect2_t window_desk2win(const window_t *window, vect2_t pos) {
    ASSERT3P(window, !=, NULL);
    
    int left, top, right, bottom;
    XPLMGetWindowGeometry(window->ref, &left, &top, &right, &bottom);
    UNUSED(right);
    UNUSED(bottom);

    return VECT2(pos.x-left, top-pos.y);
}

vect2_t window_win2desk(const window_t *window, vect2_t pos) {
    ASSERT3P(window, !=, NULL);
    
    int left, top, right, bottom;
    XPLMGetWindowGeometry(window->ref, &left, &top, &right, &bottom);
    UNUSED(right);
    UNUSED(bottom);

    return VECT2(pos.x+left, top-pos.y);
}

vect2_t window_get_size(const window_t *window) {
    ASSERT3P(window, !=, NULL);
    
    int left, top, right, bottom;
    XPLMGetWindowGeometry(window->ref, &left, &top, &right, &bottom);
    
    return VECT2(right - left, top - bottom);
}
