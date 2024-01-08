/*===--------------------------------------------------------------------------------------------===
 * capture.c
 *
 * Created by Amy Parent <amy@amyparent.com>
 * Copyright (c) 2024 Amy Parent. All rights reserved
 *
 * Licensed under the MIT License
 *===--------------------------------------------------------------------------------------------===
*/
#include "capture_impl.h"
#include <XPLMGraphics.h>

#define CACHE_SIZE (1 << 10)

panel_cap_t *panel_cap_new(vect2_t size) {
    panel_cap_t *cap = safe_calloc(1, sizeof(*cap));
    
    fdr_find(&cap->fbo_dr, "sim/graphics/view/current_gl_fbo");
    list_create(&cap->windows, sizeof(cap_window_t), offsetof(cap_window_t, list));
    
    cap->size = size;
    cap->tex = 0;
    cap->fbo = 0;
    
    return cap;
}

void panel_cap_destroy(panel_cap_t *cap) {
    ASSERT(cap);
    
    for(cap_window_t *w = list_head(&cap->windows); w;) {
        cap_window_t *next = list_next(&cap->windows, w);
        window_destroy(w->window);
        if(w->shader) glDeleteProgram(w->shader);
        if(w->cache) glutils_cache_destroy(w->cache);
        
        lacf_free(w);
        w = next;
    }
    
    if(cap->tex) glDeleteTextures(1, &cap->tex);
    if(cap->fbo) glDeleteFramebuffers(1, &cap->fbo);
    lacf_free(cap);
}

static void draw_cap_window(window_t *window, vect2_t pos, vect2_t size, void *userdata) {
    cap_window_t *win = userdata;
    panel_cap_t *cap = win->cap;
    
    UNUSED(window);
    UNUSED(pos);
    UNUSED(size);
    
    if(cap->tex == 0) return;
    
    mat4 proj_matrix, mv_matrix, pvm;
    
    VERIFY3F(dr_getvf32(&win->proj_matrix, (float *)proj_matrix, 0, 16), ==, 16);
	VERIFY3F(dr_getvf32(&win->mv_matrix, (float *)mv_matrix, 0, 16), ==, 16);
	glm_mat4_mul(proj_matrix, mv_matrix, pvm);
    
    if(!win->shader) {
		win->shader = shader_prog_from_text("panel_win_shader",
		    vert_shader, frag_shader,
		    "vtx_pos", VTX_ATTRIB_POS,
		    "vtx_tex0", VTX_ATTRIB_TEX0, NULL);
		ASSERT(win->shader != 0);
    }
    
    
    double left = win->pos.x / cap->size.x;
    double bottom = win->pos.y / cap->size.y;
    double right = left + (win->size.x / cap->size.x);
    double top = bottom + (win->size.y / cap->size.y);
    
    const vect2_t t[4] = {
	    VECT2(left, bottom),
        VECT2(left, top),
        VECT2(right, top),
        VECT2(right, bottom)
	};
    
    const vect2_t p[4] = {
        VECT2(pos.x, pos.y),
        VECT2(pos.x, pos.y + size.y),
        VECT2(pos.x + size.x, pos.y + size.y),
        VECT2(pos.x + size.x, pos.y)
    };
    
    if(win->cache == NULL) {
        win->cache = glutils_cache_new(CACHE_SIZE);
    }
    
    glutils_quads_t *quads = glutils_cache_get_2D_quads(win->cache, p, t, 4);
    
    glUseProgram(win->shader);
    XPLMBindTexture2d(cap->tex, 0);
    glUniform1i(glGetUniformLocation(win->shader, "tex"), 0);
	glUniformMatrix4fv(glGetUniformLocation(win->shader, "pvm"), 1, GL_FALSE, (const GLfloat *)pvm);
	glutils_draw_quads(quads, win->shader);
	XPLMBindTexture2d(0, 0);
    glUseProgram(0);
}

window_t *panel_cap_add_window(panel_cap_t *cap, const char *name, const char *id, vect2_t pos, vect2_t size) {
    cap_window_t *win = safe_calloc(1, sizeof(*win));
    
    win->cap = cap;
    win->pos = pos;
    win->size = size;
    
    window_conf_t conf = {
        .size = size,
        .min_scale = 0.5,
        .max_scale = 10.0,
        .is_decorated = false,
        .is_aspect_cstr = true,
        .draw = draw_cap_window
    };
    lacf_strlcpy(conf.name, name, sizeof(conf.name));
    lacf_strlcpy(conf.id, id, sizeof(conf.id));
    
	fdr_find(&win->proj_matrix, "sim/graphics/view/projection_matrix");
	fdr_find(&win->mv_matrix, "sim/graphics/view/modelview_matrix");
    
    win->window = window_new(&conf, win);
    
    list_insert_tail(&cap->windows, win);
    
    return win->window;
}

void panel_cap_update(panel_cap_t *cap) {
    // UNUSED(cap);
    // return;
    
    glBindFramebuffer(GL_READ_FRAMEBUFFER, dr_geti(&cap->fbo_dr));
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    
    if(cap->tex == 0) {
        glGenTextures(1, &cap->tex);
        glBindTexture(GL_TEXTURE_2D, cap->tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, cap->size.x, cap->size.y, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    
    if(cap->fbo == 0) {
        glGenFramebuffers(1, &cap->fbo);
        glBindFramebufferEXT(GL_FRAMEBUFFER, cap->fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cap->tex, 0);
    }
    
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, cap->fbo);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);

    // Copy the pixels over
    glBlitFramebuffer(
        0, 0, cap->size.x, cap->size.y,
        0, 0, cap->size.x, cap->size.y,
        GL_COLOR_BUFFER_BIT, GL_NEAREST);
        
    glBindFramebuffer(GL_FRAMEBUFFER, dr_geti(&cap->fbo_dr));
}
