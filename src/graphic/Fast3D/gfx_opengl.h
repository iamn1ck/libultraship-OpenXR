#ifndef GFX_OPENGL_H
#define GFX_OPENGL_H

#include "gfx_rendering_api.h"

extern struct GfxRenderingAPI gfx_opengl_api;

void gfx_opengl_set_vr_rendering_mode(bool enabled);
bool gfx_opengl_get_vr_rendering_mode();

#endif
