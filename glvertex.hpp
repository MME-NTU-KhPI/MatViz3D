// ============================================================================
//  glvertex.hpp  -  The one interleaved vertex layout used by every mesh in
//                   MatViz3D, shared by all renderers.
//
//  This started life as RenderOpenGL::Voxel and is promoted here unchanged so
//  that code which builds geometry (tensor glyphs, streamline tubes, elastic
//  surfaces) does not have to include the whole voxel renderer just to name a
//  vertex type.  RenderOpenGL::Voxel remains as an alias, so existing code and
//  the existing VBO layout are untouched.
//
//  Attribute layout expected by glshaders.hpp's lit program:
//      location 0  aPosition  3 x GL_FLOAT
//      location 1  aColor     4 x GL_UNSIGNED_BYTE, normalized
//      location 2  aNormal    3 x GL_BYTE,          normalized
//      location 3  aAO        1 x GL_UNSIGNED_BYTE, normalized (voxel view only)
//
//  Normals are stored as GLbyte in [-127, 127] (i.e. round(127 * n)), which
//  costs at most ~0.45 degrees of angular error -- invisible under this
//  shading model, and it keeps one vertex format for every mesh in the app.
//
//  `ao` is the per-vertex ambient-occlusion level of a voxel face (255 = open,
//  0 = in a fully occluded corner), filled by the voxel mesh builder. It sits
//  in what used to be the struct's padding byte, so sizeof stays 20; every
//  other mesh leaves it at 255 ("not occluded").
// ============================================================================
#pragma once

#include <QOpenGLFunctions>

struct GlVertex
{
    GLfloat x, y, z;        ///< position, world space
    GLubyte r, g, b, a;     ///< colour, normalized to [0,1] by the attrib pointer
    GLbyte  nx, ny, nz;     ///< normal, normalized to [-1,1] by the attrib pointer
    GLubyte ao = 255;       ///< ambient occlusion, normalized to [0,1]; 255 = none
};
static_assert(sizeof(GlVertex) == 20, "GlVertex is uploaded as-is; keep it 20 bytes");

/// Pack a unit-length normal into the GLbyte triple this layout expects.
inline void glVertexSetNormal(GlVertex& v, float nx, float ny, float nz)
{
    v.nx = static_cast<GLbyte>(nx < 0 ? nx * 127.0f - 0.5f : nx * 127.0f + 0.5f);
    v.ny = static_cast<GLbyte>(ny < 0 ? ny * 127.0f - 0.5f : ny * 127.0f + 0.5f);
    v.nz = static_cast<GLbyte>(nz < 0 ? nz * 127.0f - 0.5f : nz * 127.0f + 0.5f);
}
