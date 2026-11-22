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
//  GLSL #version 150 (OpenGL 3.2) on every platform: in / out instead of
//  attribute / varying and a declared fragment output instead of gl_FragColor.
//  That is the lowest version a Core Profile context accepts (macOS gives
//  nothing between a legacy 2.1 context and Core Profile, main.cpp asks for the
//  latter there), and a compatibility context of OpenGL 3.2 or later, which is
//  what Windows and Linux drivers give by default, takes the same text. The
//  shaders use no feature newer than GLSL 1.30, so the only requirement the
//  version adds is OpenGL 3.2 instead of 3.0.
//
//  The lit program's attribute locations must be bound explicitly (there is no
//  layout(location=) qualifier at this GLSL version):
//      aPosition -> 0, aColor -> 1, aNormal -> 2, aAO -> 3   (see glvertex.hpp)
//  aAO is bound by the voxel view only; a renderer that leaves it unbound must
//  keep uShadingModel at 0, which never reads it.
//  The axis program binds aPosition -> 0, aColor -> 1.
// ============================================================================
#pragma once

namespace matviz_gl {

// ---------------------------------------------------------------------------
//  Lit program: Blinn-Phong with six directional lights.
// ---------------------------------------------------------------------------
inline constexpr const char* kLitVertexShader = R"(
        #version 150
        in vec3 aPosition;
        in vec4 aColor;
        in vec3 aNormal;
        in float aAO;

        uniform mat4 uMVP;
        uniform mat4 uModel;
        uniform mat4 uView;
        uniform mat4 uProjection;

        out vec3 FragPos;
        out vec3 Normal;
        out vec4 Color;
        out float AO;
        out vec3 ViewPos;       // camera space, for the camera-fixed lights
        out vec3 ViewNormal;

        void main()
        {
            FragPos = vec3(uModel * vec4(aPosition, 1.0));
            Normal = normalize(mat3(uModel) * aNormal);
            Color = aColor;
            AO = aAO;
            ViewPos = vec3(uView * uModel * vec4(aPosition, 1.0));
            ViewNormal = mat3(uView) * mat3(uModel) * aNormal;   // rotation only, no scale
            gl_Position = uMVP * vec4(aPosition, 1.0);
        }
    )";

inline constexpr const char* kLitFragmentShader = R"(
        #version 150
        out vec4 FragColor;
        in vec3 FragPos;
        in vec3 Normal;
        in vec4 Color;
        in float AO;
        in vec3 ViewPos;
        in vec3 ViewNormal;

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

        // 0 = the original six object-fixed lights (also what every renderer
        //     that does not set it gets: uniforms default to 0);
        // 1 = depth shading: camera-fixed key + fill light, hemispherical
        //     ambient, per-vertex ambient occlusion, depth cueing.
        uniform int   uShadingModel;
        uniform float uAOStrength;   // 0 = off, 1 = full
        uniform float uDepthCue;     // 0 = off, 1 = far side at the strongest
        uniform vec2  uDepthRange;   // camera distance of the scene's near / far side
        uniform float uMinLight;     // brightness floor, as a fraction of the base colour

        vec3 depthShading()
        {
            vec3 n = normalize(ViewNormal);
            vec3 v = normalize(-ViewPos);

            // AO level 0..1 -> 0.45..1, blended in by the strength.
            float ao = mix(1.0, 0.45 + 0.55 * AO, uAOStrength);

            // Qt Quick shows this FBO with GL's bottom row on top, so camera
            // -Y is "up" on screen: the sky light comes from -Y.
            float up = 0.5 - 0.5 * n.y;
            vec3 ambient = mix(vec3(0.28), vec3(0.44), up);

            // Key light from the upper left in front, a weaker fill from the
            // right: the three faces of a cube in view come out clearly apart.
            // In the default dimetric view: top 0.97, left 0.76, right 0.60
            // of the base colour; a face toward the camera 0.89.
            vec3 keyDir  = normalize(vec3(-0.65, -0.55, 0.55));
            vec3 fillDir = normalize(vec3( 0.80,  0.10, 0.60));
            float dk = max(dot(n, keyDir),  0.0);
            float df = max(dot(n, fillDir), 0.0);
            vec3 direct = vec3(0.66 * dk + 0.28 * df);
            float spec = pow(max(dot(n, normalize(keyDir + v)), 0.0), 32.0) * 0.12;

            // AO darkens the ambient fully and the direct light partly: a pore
            // wall still catches some light, but clearly less than an outer face.
            vec3 c = Color.rgb * (ambient * ao + direct * mix(1.0, ao, 0.6)) + vec3(spec * ao);

            // Floor: a saturated colour on a face turned away must not sink
            // into a dark background.
            c = max(c, Color.rgb * uMinLight);

            // Depth cueing: the far side greyer and darker.
            float d = -ViewPos.z;
            float t = clamp((d - uDepthRange.x) / max(uDepthRange.y - uDepthRange.x, 1e-3), 0.0, 1.0);
            float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
            vec3 farColor = mix(vec3(lum), c, 0.6) * 0.62;
            c = mix(c, farColor, uDepthCue * t);

            return clamp(c, 0.0, 1.0);
        }

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
                FragColor = Color;

            } else if (uDebugMode == 2) {
                // Absolute normal as RGB. Each axis pair shows as one color:
                //   X faces = red, Y faces = green, Z faces = blue.
                // If any face shows wrong color, normal encoding or winding is wrong.
                // If all faces show same dark color, GLbyte normals are near zero —
                // check that n[] array uses +-127 not +-1.
                FragColor = vec4(abs(norm.x), abs(norm.y), abs(norm.z), 1.0);

            } else if (uDebugMode == 3) {
                // Flat white — confirms geometry is being drawn and depth test works.
                // If scene disappears here, the issue is in geometry not lighting.
                FragColor = vec4(1.0, 1.0, 1.0, Color.a);

            } else if (uWireframe == 1) {
                // Flat gray for wireframe lines, ignore lighting and voxel color
                FragColor = vec4(0.6, 0.6, 0.6, 1.0);
                } else {
                    // Normal lighting mode (uDebugMode == 0)
                    if (uShadingModel == 1) result = depthShading();
                    FragColor = vec4(result, Color.a * uAlphaScale);
                }
        }
 )";

// ---------------------------------------------------------------------------
//  Axis program: unlit position + colour, used for every line/gizmo pass.
// ---------------------------------------------------------------------------
inline constexpr const char* kAxisVertexShader = R"(
        #version 150
        in vec3 aPosition;
        in vec3 aColor;

        uniform mat4 uMVP;

        out vec3 Color;

        void main()
        {
            Color = aColor;
            gl_Position = uMVP * vec4(aPosition, 1.0);
        }
    )";

inline constexpr const char* kAxisFragmentShader = R"(
        #version 150
        out vec4 FragColor;
        in vec3 Color;
        void main()
        {
            FragColor = vec4(Color, 1.0);
        }
    )";

} // namespace matviz_gl
