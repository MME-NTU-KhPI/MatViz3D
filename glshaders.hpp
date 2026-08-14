// ============================================================================
//  glshaders.hpp  -  The GLSL sources shared by every renderer in MatViz3D.
//
//  Extracted verbatim from RenderOpenGL::initializeGL() so that a second
//  renderer (the elastic surface view in the material database window) shades
//  its geometry identically to the main voxel view instead of growing its own
//  copy.  There is prior art for what happens otherwise: the abandoned
//  qml-tensor-visualization branch duplicated the renderer and immediately
//  drifted to fixed-function glBegin/glVertex, which is why this header exists.
//
//  GLSL #version 130 with legacy attribute/varying/gl_FragColor, i.e. a
//  compatibility profile context (main.cpp requests QSGRendererInterface::OpenGL
//  and never asks for a core profile).  Do not "modernize" these in place --
//  the whole app depends on the compatibility path.
//
//  The lit program's attribute locations must be bound explicitly (there is no
//  layout(location=) qualifier at this GLSL version):
//      aPosition -> 0, aColor -> 1, aNormal -> 2      (see glvertex.hpp)
//  The axis program binds aPosition -> 0, aColor -> 1.
// ============================================================================
#pragma once

namespace matviz_gl {

// ---------------------------------------------------------------------------
//  Lit program: Blinn-Phong with six directional lights.
// ---------------------------------------------------------------------------
inline constexpr const char* kLitVertexShader = R"(
        #version 130
        attribute vec3 aPosition;
        attribute vec4 aColor;
        attribute vec3 aNormal;

        uniform mat4 uMVP;
        uniform mat4 uModel;
        uniform mat4 uView;
        uniform mat4 uProjection;

        varying vec3 FragPos;
        varying vec3 Normal;
        varying vec4 Color;

        void main()
        {
            FragPos = vec3(uModel * vec4(aPosition, 1.0));
            Normal = normalize(mat3(uModel) * aNormal);
            Color = aColor;
            gl_Position = uMVP * vec4(aPosition, 1.0);
        }
    )";

inline constexpr const char* kLitFragmentShader = R"(
        #version 130
        varying vec3 FragPos;
        varying vec3 Normal;
        varying vec4 Color;

        uniform vec3 uLightDirections[6];
        uniform float uLightWeights[6];
        uniform vec3 uViewPos;
        uniform int uDebugMode;
        uniform int uWireframe;

        // Multiplies the vertex alpha. Lets the voxel block be faded back so
        // overlay geometry drawn inside it (tensor glyphs, streamline tubes)
        // stays visible, without touching the per-vertex colours.
        //
        // GLSL uniforms default to 0, i.e. fully transparent -- so EVERY pass
        // that binds this program must set it. There are two: RenderOpenGL and
        // StiffnessSurfaceRenderer.
        uniform float uAlphaScale;
        void main()
        {
            // --- Geometry ---
            vec3 norm    = normalize(Normal);
            // if (!gl_FrontFacing) norm = -norm;
            vec3 viewDir = normalize(uViewPos - FragPos);

            // --- Ambient ---
            // Tinted by face color so shadowed faces still show their hue,
            // not a flat gray. Low enough to not wash out diffuse.
            vec3 ambient = 0.50 * Color.rgb;
            vec3 result  = ambient;

            // --- Lights ---

            for (int i = 0; i < 6; i++)
            {
                vec3 lightDir = uLightDirections[i];
                vec3 halfDir  = normalize(lightDir + viewDir);

                // max(dot, 0) not abs() — abs() creates dark bands at 90deg
                // because it mirrors the lighting response, causing entire
                // layers of faces to go dark simultaneously at certain angles.
                float diff = max(dot(norm, lightDir), 0.0);

                // Specular: reduced exponent (16 vs 32) for wider softer highlight,
                // reduced intensity (0.15) so 3 lights don't blow out bright faces.
                float spec = pow(max(dot(norm, halfDir), 0.0), 16.0);

                vec3 diffuse  = diff * uLightWeights[i] * Color.rgb;
                vec3 specular = spec * uLightWeights[i] * 0.15 * vec3(1.0);
                result += (diffuse + specular);
            }

            // --- Clamp ---
            // Prevents overbright faces making adjacent dark faces look
            // even darker by contrast. Hard requirement when accumulating
            // multiple lights without HDR tonemapping.
            result = clamp(result, 0.0, 1.0);

            // --- Debug modes ---
            if (uDebugMode == 1) {
                // Raw vertex color — confirms color data is reaching shader correctly.
                // If all faces same color here, node_colors assignment is broken.
                gl_FragColor = Color;

            } else if (uDebugMode == 2) {
                // Absolute normal as RGB. Each axis pair shows as one color:
                //   X faces = red, Y faces = green, Z faces = blue.
                // If any face shows wrong color, normal encoding or winding is wrong.
                // If all faces show same dark color, GLbyte normals are near zero —
                // check that n[] array uses +-127 not +-1.
                gl_FragColor = vec4(abs(norm.x), abs(norm.y), abs(norm.z), 1.0);

            } else if (uDebugMode == 3) {
                // Flat white — confirms geometry is being drawn and depth test works.
                // If scene disappears here, the issue is in geometry not lighting.
                gl_FragColor = vec4(1.0, 1.0, 1.0, Color.a);

            } else if (uWireframe == 1) {
                // Flat gray for wireframe lines, ignore lighting and voxel color
                gl_FragColor = vec4(0.6, 0.6, 0.6, 1.0);
                } else {
                    // Normal lighting mode (uDebugMode == 0)
                    gl_FragColor = vec4(result, Color.a * uAlphaScale);
                }
        }
 )";

// ---------------------------------------------------------------------------
//  Axis program: unlit position + colour, used for every line/gizmo pass.
// ---------------------------------------------------------------------------
inline constexpr const char* kAxisVertexShader = R"(
        #version 130
        attribute vec3 aPosition;
        attribute vec3 aColor;

        uniform mat4 uMVP;

        varying vec3 Color;

        void main()
        {
            Color = aColor;
            gl_Position = uMVP * vec4(aPosition, 1.0);
        }
    )";

inline constexpr const char* kAxisFragmentShader = R"(
        #version 130
        varying vec3 Color;
        void main()
        {
            gl_FragColor = vec4(Color, 1.0);
        }
    )";

} // namespace matviz_gl
