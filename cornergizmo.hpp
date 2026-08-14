#pragma once

//  cornergizmo.hpp  -  Geometry of the corner axis triad.
//
//  Read by two unrelated classes: RenderOpenGL::drawCornerAxes() draws the
//  triad from these numbers, and OpenGLWidgetQML::projectAxisLabel() derives
//  the screen positions of the QML X/Y/Z labels from the same ones. Kept in
//  their own header rather than inside either class, so that neither has to
//  widen its public interface for the other.

namespace matviz_gizmo {

inline constexpr int   kSize    = 80;    // px, side of the square viewport
inline constexpr int   kMargin  = 10;    // px, inset from the widget edge
inline constexpr float kCamDist = 2.8f;  // camera pull-back along -Z
inline constexpr float kFov     = 45.0f; // perspective field of view, degrees
inline constexpr float kAxisLen = 0.8f;  // axis shaft length (L)

} // namespace matviz_gizmo