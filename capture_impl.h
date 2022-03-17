/*===--------------------------------------------------------------------------------------------===
 * capture_impl.h
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
#ifndef _CAPTURE_IMPL_H_
#define _CAPTURE_IMPL_H_

#include <acfutils/avl.h>
#include <acfutils/list.h>
#include <acfutils/dr.h>
#include <acfutils/glew.h>
#include <acfutils/log.h>
#include <acfutils/safe_alloc.h>
#include <acfutils/glutils.h>
#include <acfutils/shader.h>
#include <window/capture.h>

struct panel_cap_t {
    dr_t fbo_dr;
    vect2_t size;
    GLuint tex;
    GLuint fbo;
    list_t windows;
};

typedef struct {
    panel_cap_t     *cap;
    
    window_t        *window;
    vect2_t         pos;
    vect2_t         size;

    GLuint          shader;
    glutils_cache_t *cache;
    dr_t            proj_matrix;
    dr_t            mv_matrix;
    
    list_node_t list;
} cap_window_t;

static const char *vert_shader =
    "#version 120\n"
    "uniform mat4       pvm;\n"
    "attribute vec3     vtx_pos;\n"
    "attribute vec2     vtx_tex0;\n"
    "varying vec2       tex_coord;\n"
    "void main() {\n"
    "   tex_coord = vtx_tex0;\n"
    "   gl_Position = pvm * vec4(vtx_pos, 1.0);\n"
    "}\n";

static const char *frag_shader =
    "#version 120\n"
    "uniform sampler2D  tex;\n"
    "varying vec2       tex_coord;\n"
    "void main() {\n"
    "   gl_FragColor = texture2D(tex, tex_coord);\n"
    "}\n";

#endif /* ifndef _CAPTURE_IMPL_H_ */

