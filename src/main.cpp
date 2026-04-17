#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <string>
#include <array>
#include <cmath>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

struct Camera
{
    float x, y, z;             // position
    float u[3], v[3], n[3];    // camera axes (right, up, back)
    float hFov, vFov;          // horizontal and vertical field of view
    float nearPlane, farPlane; // near and far clipping planes

    Camera()
        : x(0), y(0), z(3),
          u{1, 0, 0}, v{0, 1, 0}, n{0, 0, -1},
          hFov(60.0f), vFov(60.0f),
          nearPlane(0.1f), farPlane(100.0f) {}
};

struct Vec4
{
    float x, y, z, w;

    Vec4(float x = 0, float y = 0, float z = 0, float w = 0)
        : x(x), y(y), z(z), w(w) {}
};

class Matrix4
{
public:
    std::array<float, 16> m;

    Matrix4()
    {
        m.fill(0.0f);
    }

    static Matrix4 identity()
    {
        Matrix4 mat;
        mat.m[0] = 1.0f;
        mat.m[5] = 1.0f;
        mat.m[10] = 1.0f;
        mat.m[15] = 1.0f;
        return mat;
    }

    float &operator()(int row, int col)
    {
        return m[row * 4 + col];
    }

    float operator()(int row, int col) const
    {
        return m[row * 4 + col];
    }

    // Matrix * Vec
    Vec4 operator*(const Vec4 &v) const
    {
        return Vec4(
            (*this)(0, 0) * v.x + (*this)(0, 1) * v.y + (*this)(0, 2) * v.z + (*this)(0, 3) * v.w,
            (*this)(1, 0) * v.x + (*this)(1, 1) * v.y + (*this)(1, 2) * v.z + (*this)(1, 3) * v.w,
            (*this)(2, 0) * v.x + (*this)(2, 1) * v.y + (*this)(2, 2) * v.z + (*this)(2, 3) * v.w,
            (*this)(3, 0) * v.x + (*this)(3, 1) * v.y + (*this)(3, 2) * v.z + (*this)(3, 3) * v.w);
    }

    // Matrix * Matrix
    Matrix4 operator*(const Matrix4 &other) const
    {
        Matrix4 result;

        for (int row = 0; row < 4; ++row)
        {
            for (int col = 0; col < 4; ++col)
            {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k)
                {
                    sum += (*this)(row, k) * other(k, col);
                }
                result(row, col) = sum;
            }
        }

        return result;
    }
};

float dot3(const float a[3], float x, float y, float z)
{
    return a[0] * x + a[1] * y + a[2] * z;
}

Matrix4 viewMatrix(const Camera &cam)
{
    Matrix4 view = Matrix4::identity();

    // Set rotation part (camera axes)
    for (int i = 0; i < 3; ++i)
    {
        view(0, i) = cam.u[i];
        view(1, i) = cam.v[i];
        view(2, i) = cam.n[i];
    }

    // Set translation part: -dot(axis, eye)
    view(0, 3) = -dot3(cam.u, cam.x, cam.y, cam.z);
    view(1, 3) = -dot3(cam.v, cam.x, cam.y, cam.z);
    view(2, 3) = -dot3(cam.n, cam.x, cam.y, cam.z);

    return view;
}

Matrix4 projectionMatrix(const Camera &cam, float aspect)
{
    float top = cam.nearPlane * tanf(glm::radians(cam.vFov) * 0.5f);
    float right = top * aspect;
    float left = -right;
    float bottom = -top;

    Matrix4 proj = Matrix4::identity();
    proj(0, 0) = 2.0f * cam.nearPlane / (right - left);
    proj(0, 2) = (right + left) / (right - left);
    proj(1, 1) = 2.0f * cam.nearPlane / (top - bottom);
    proj(1, 2) = (top + bottom) / (top - bottom);
    proj(2, 2) = -(cam.farPlane + cam.nearPlane) / (cam.farPlane - cam.nearPlane);
    proj(2, 3) = -(2.0f * cam.farPlane * cam.nearPlane) / (cam.farPlane - cam.nearPlane);
    proj(3, 2) = -1.0f; // perspective divide by -w
    proj(3, 3) = 0.0f;

    return proj;
}

Matrix4 viewportMatrix(int width, int height)
{
    Matrix4 viewport = Matrix4::identity();
    viewport(0, 0) = width / 2.0f;
    viewport(1, 1) = -height / 2.0f; // flip Y for screen space
    viewport(0, 3) = width / 2.0f;
    viewport(1, 3) = height / 2.0f;
    return viewport;
}

char *loadShader(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        printf("Cannot open shader: %s\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    char *src = (char *)malloc(len + 1);
    fread(src, 1, len, f);
    src[len] = '\0';
    fclose(f);
    return src;
}

struct Settings
{
    // Camera parameters
    glm::vec3 eye;
    glm::vec3 target;
    float yaw, pitch, roll;
    float fov;
    float nearPlane, farPlane;

    // Model parameters
    int currentModel;
    float modelScale[2];
    glm::vec4 color;

    // Render settings
    int drawModeIdx;
    bool backfaceCulling;
    int windingIdx;

    Settings()
        : eye(0.0f, 0.0f, 3.0f),
          target(0.0f, 0.0f, 0.0f),
          yaw(0.0f), pitch(0.0f), roll(0.0f),
          fov(45.0f), nearPlane(0.1f), farPlane(100.0f),
          currentModel(0), modelScale{1.0f, 0.1f}, color(1.0f),
          drawModeIdx(1), backfaceCulling(false), windingIdx(0) {}
};
struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
};

std::vector<Vertex> loadObj(const char *path)
{
    std::vector<Vertex> vertices;
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        printf("Cannot open obj: %s\n", path);
        return vertices;
    }

    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        if (line[0] == 'v' && (line[1] == '0' || line[1] == '1' || line[1] == '2'))
        {
            Vertex vert;
            int colorIdx;
            sscanf(line, "v%*d %f %f %f %f %f %f %d",
                   &vert.position.x, &vert.position.y, &vert.position.z,
                   &vert.normal.x, &vert.normal.y, &vert.normal.z,
                   &colorIdx);
            vertices.push_back(vert);
        }
    }
    fclose(f);
    return vertices;
}

// glm::mat4 MVP(float Translate, glm::vec2 const &Rotate)
// {
//     glm::mat4 Projection = glm::perspective(glm::radians(45.0f), 4.0f / 3.0f, 0.1f, 100.f);
//     glm::mat4 View = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -Translate));
//     View = glm::rotate(View, Rotate.y, glm::vec3(-1.0f, 0.0f, 0.0f));
//     View = glm::rotate(View, Rotate.x, glm::vec3(0.0f, 1.0f, 0.0f));
//     glm::mat4 Model = glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));
//     return Projection * View * Model;
// }

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(1920, 1080, "Close2GL Without Rasterization", NULL, NULL);
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        printf("Failed to init GLAD\n");
        return -1;
    }

    // ImGui setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    int systemIdx = 0; // 0 = OpenGL, 1 = Close2GL

    // Load shaders texts
    char *vertSrc = loadShader(SHADER_DIR "/vertex.glsl");
    char *fragSrc = loadShader(SHADER_DIR "/fragment.glsl");
    char *leanVertSrc = loadShader(SHADER_DIR "/lean_vertex.glsl");

    // Compile vertex shader
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, (const char **)&vertSrc, NULL);
    glCompileShader(vs);

    // Compile fragment shader
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, (const char **)&fragSrc, NULL);
    glCompileShader(fs);

    // Link OpenGL program (full MVP in shader)
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    // Compile lean vertex shader (passthrough — CPU already produced NDC)
    GLuint leanVS = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(leanVS, 1, (const char **)&leanVertSrc, NULL);
    glCompileShader(leanVS);

    GLuint leanFS = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(leanFS, 1, (const char **)&fragSrc, NULL);
    glCompileShader(leanFS);

    // Link Close2GL program (no MVP uniform needed)
    GLuint leanProgram = glCreateProgram();
    glAttachShader(leanProgram, leanVS);
    glAttachShader(leanProgram, leanFS);
    glLinkProgram(leanProgram);
    glDeleteShader(leanVS);
    glDeleteShader(leanFS);

    GLint uMVP = glGetUniformLocation(program, "uMVP");
    GLint uColor = glGetUniformLocation(program, "uColor");
    GLint uColorLean = glGetUniformLocation(leanProgram, "uColor");

    static int currentModel = 0;
    const char *const modelPaths[2] = {
        MODEL_DIR "/cube.in",
        MODEL_DIR "/cow_up.in",
    };

    std::vector<Vertex> models[2] = {
        loadObj(modelPaths[0]),
        loadObj(modelPaths[1]),
    };

    glm::vec4 color(1.0f, 1.0f, 1.0f, 1.0f);

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, models[0].size() * sizeof(Vertex), models[0].data(), GL_DYNAMIC_DRAW);

    // location 0: position — 3 floats, stride = sizeof(Vertex), offset = 0
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
    glEnableVertexAttribArray(0);

    // location 1: normal — 3 floats, stride = sizeof(Vertex), offset = sizeof(vec3)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    glUseProgram(program);
    glEnable(GL_DEPTH_TEST);

    int prevModel = currentModel;

    // Camera parameters
    glm::vec3 eye(0.0f, 0.0f, 3.0f);
    glm::vec3 target(0.0f, 0.0f, 0.0f);
    float yaw = 0.0f;
    float pitch = 0.0f;
    float roll = 0.0f;
    // float orbitDistance = 3.0f;
    float translateStep = 0.1f;
    float fov = 45.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    float modelScale[2] = {1.0f, 0.1f}; // cube, cow

    // Render settings
    static const GLenum polyModes[] = {GL_FILL, GL_LINE, GL_POINT};
    int drawModeIdx = 1; // default: wireframe
    bool backfaceCulling = false;
    int windingIdx = 0; // 0 = CCW (OpenGL default), 1 = CW

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Compute MVP
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        float aspect = (float)width / (float)height;

        // Camera Forward Axis (points backwards from target to eye)
        glm::vec3 n = glm::vec3(cosf(pitch) * sinf(yaw), sinf(pitch), cosf(pitch) * cosf(yaw));

        // Camera Right Axis
        glm::vec3 u = glm::normalize(glm::cross(glm::vec3(0, 1, 0), n));

        if (glm::length(u) < 0.001f)
            u = glm::vec3(1, 0, 0); // degenerate guard

        glm::vec3 v_base = glm::normalize(glm::cross(n, u));

        // Apply roll: rotate both u and v around n simultaneously
        glm::vec3 v = cosf(roll) * v_base - sinf(roll) * u;
        u = cosf(roll) * u + sinf(roll) * v_base;

        if (systemIdx == 0)
        {
            glUseProgram(program);

            glm::mat4 view = glm::lookAt(eye, eye - n, v);
            glm::mat4 proj = glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
            glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(modelScale[currentModel]));
            glm::mat4 mvp = proj * view * model;

            glUniformMatrix4fv(uMVP, 1, GL_FALSE, glm::value_ptr(mvp));
            glUniform4f(uColor, color.r, color.g, color.b, color.a);

            glViewport(0, 0, width, height);
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            if (currentModel != prevModel)
            {
                glBindBuffer(GL_ARRAY_BUFFER, VBO);
                glBufferData(GL_ARRAY_BUFFER,
                             models[currentModel].size() * sizeof(Vertex),
                             models[currentModel].data(),
                             GL_DYNAMIC_DRAW);
                prevModel = currentModel;
            }

            glFrontFace(windingIdx == 1 ? GL_CW : GL_CCW);
            if (backfaceCulling)
            {
                glEnable(GL_CULL_FACE);
                glCullFace(GL_BACK);
            }
            else
            {
                glDisable(GL_CULL_FACE);
            }

            glBindVertexArray(VAO);
            glPolygonMode(GL_FRONT_AND_BACK, polyModes[drawModeIdx]);
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)models[currentModel].size());
        }
        else if (systemIdx == 1)
        {
            // Build Camera from current UI params
            Camera cam;
            cam.x = eye.x;
            cam.y = eye.y;
            cam.z = eye.z;
            cam.u[0] = u.x;
            cam.u[1] = u.y;
            cam.u[2] = u.z;
            cam.v[0] = v.x;
            cam.v[1] = v.y;
            cam.v[2] = v.z;
            cam.n[0] = n.x;
            cam.n[1] = n.y;
            cam.n[2] = n.z;
            cam.hFov = fov;
            cam.vFov = fov;
            cam.nearPlane = nearPlane;
            cam.farPlane = farPlane;

            // Build model (scale only)
            float s = modelScale[currentModel];
            Matrix4 modelMat = Matrix4::identity();
            modelMat(0, 0) = s;
            modelMat(1, 1) = s;
            modelMat(2, 2) = s;

            Matrix4 view = viewMatrix(cam);
            Matrix4 proj = projectionMatrix(cam, aspect);
            Matrix4 mvp = proj * view * modelMat;

            // Transform vertices on CPU to NDC
            const auto &srcVerts = models[currentModel];
            std::vector<Vertex> transformed(srcVerts.size());
            for (size_t i = 0; i < srcVerts.size(); ++i)
            {
                Vec4 p(srcVerts[i].position.x, srcVerts[i].position.y, srcVerts[i].position.z, 1.0f);
                Vec4 clip = mvp * p;
                float invW = (clip.w != 0.0f) ? 1.0f / clip.w : 0.0f;
                transformed[i].position = glm::vec3(clip.x * invW, clip.y * invW, clip.z * invW);
                transformed[i].normal = srcVerts[i].normal;
            }

            // CPU-side backface culling for Close2GL
            // Compute 2D signed area in NDC to determine triangle winding,
            // then discard back-facing triangles before uploading to GPU.
            std::vector<Vertex> visible;
            visible.reserve(transformed.size());
            for (size_t i = 0; i + 2 < transformed.size(); i += 3)
            {
                const glm::vec3 &v0 = transformed[i].position;
                const glm::vec3 &v1 = transformed[i + 1].position;
                const glm::vec3 &v2 = transformed[i + 2].position;

                // 2D cross product of edges in NDC (xy plane)
                float cross_z = (v1.x - v0.x) * (v2.y - v0.y) - (v1.y - v0.y) * (v2.x - v0.x);

                // cross_z > 0 → CCW winding, cross_z < 0 → CW winding
                bool isFrontFacing = (windingIdx == 0) ? (cross_z > 0.0f) : (cross_z < 0.0f);

                if (backfaceCulling && !isFrontFacing)
                    continue;

                visible.push_back(transformed[i]);
                visible.push_back(transformed[i + 1]);
                visible.push_back(transformed[i + 2]);
            }

            // Upload pre-transformed (NDC) vertices; lean shader passes them through
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER,
                         visible.size() * sizeof(Vertex),
                         visible.data(),
                         GL_DYNAMIC_DRAW);

            glUseProgram(leanProgram);
            glUniform4f(uColorLean, color.r, color.g, color.b, color.a);

            glViewport(0, 0, width, height);
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Culling already handled on CPU; disable GPU-side culling for Close2GL
            glDisable(GL_CULL_FACE);

            glBindVertexArray(VAO);
            glPolygonMode(GL_FRONT_AND_BACK, polyModes[drawModeIdx]);
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)visible.size());

            // Force VBO re-upload on next OpenGL frame
            prevModel = -1;
        }

        // ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Settings");

        // ── System ───────────────────────────────────────────────────────────
        if (ImGui::Button("Reset"))
        {
            eye = glm::vec3(0.0f, 0.0f, 3.0f);
            target = glm::vec3(0.0f, 0.0f, 0.0f);
            yaw = 0.0f;
            pitch = 0.0f;
            roll = 0.0f;
            fov = 45.0f;
            nearPlane = 0.1f;
            farPlane = 100.0f;
            modelScale[0] = 1.0f;
            modelScale[1] = 0.1f;
            color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            drawModeIdx = 1;
            backfaceCulling = false;
            windingIdx = 0;
        }
        {
            float frame_h = ImGui::GetFrameHeight();
            float inner_sp = ImGui::GetStyle().ItemInnerSpacing.x;
            float item_sp = ImGui::GetStyle().ItemSpacing.x;
            float radioW = frame_h + inner_sp + ImGui::CalcTextSize("OpenGL").x + item_sp + frame_h + inner_sp + ImGui::CalcTextSize("Close2GL").x;
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - radioW);
        }
        ImGui::RadioButton("OpenGL", &systemIdx, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Close2GL", &systemIdx, 1);
        ImGui::Separator();

        // ── Camera ───────────────────────────────────────────────────────────
        if (ImGui::CollapsingHeader("Camera"))
        {
            ImGui::SeparatorText("Info");
            ImGui::Text("Position: (%.3f, %.3f, %.3f)", eye.x, eye.y, eye.z);
            ImGui::Text("Target:   (%.3f, %.3f, %.3f)", target.x, target.y, target.z);
            ImGui::Text("u (right): (%.3f, %.3f, %.3f)", u.x, u.y, u.z);
            ImGui::Text("v (up):    (%.3f, %.3f, %.3f)", v.x, v.y, v.z);
            ImGui::Text("n (back):  (%.3f, %.3f, %.3f)", n.x, n.y, n.z);

            // Rotate around camera's own axes
            ImGui::SeparatorText("Rotate");
            ImGui::SliderAngle("Yaw (v axis)", &yaw, -180.0f, 180.0f);
            ImGui::SliderAngle("Pitch (u axis)", &pitch, -89.0f, 89.0f);
            ImGui::SliderAngle("Roll (n axis)", &roll, -180.0f, 180.0f);

            // Translate freely along camera axes (eye + target move together)
            ImGui::SeparatorText("Free Translate");
            ImGui::DragFloat("Step##b", &translateStep, 0.01f, 0.001f, 10.0f);
            ImGui::Text("u (right):");
            ImGui::SameLine();
            if (ImGui::Button("-u##b"))
            {
                eye -= translateStep * u;
                target -= translateStep * u;
            }
            ImGui::SameLine();
            if (ImGui::Button("+u##b"))
            {
                eye += translateStep * u;
                target += translateStep * u;
            }
            ImGui::Text("v (up):");
            ImGui::SameLine();
            if (ImGui::Button("-v##b"))
            {
                eye -= translateStep * v;
                target -= translateStep * v;
            }
            ImGui::SameLine();
            if (ImGui::Button("+v##b"))
            {
                eye += translateStep * v;
                target += translateStep * v;
            }
            ImGui::Text("n (back):");
            ImGui::SameLine();
            if (ImGui::Button("-n##b"))
            {
                eye -= translateStep * n;
                target -= translateStep * n;
            }
            ImGui::SameLine();
            if (ImGui::Button("+n##b"))
            {
                eye += translateStep * n;
                target += translateStep * n;
            }

            // Translate along camera axes while always looking at object center (target fixed)
            ImGui::SeparatorText("Look-at Translate");
            ImGui::DragFloat("Step##c", &translateStep, 0.01f, 0.001f, 10.0f);
            auto lookAtTranslate = [&](glm::vec3 axis, float delta)
            {
                glm::vec3 new_eye = eye + delta * axis;
                glm::vec3 d = new_eye - target;
                float dist = glm::length(d);
                if (dist < 0.001f)
                    return;
                glm::vec3 nd = d / dist;
                pitch = asinf(glm::clamp(nd.y, -1.0f, 1.0f));
                yaw = atan2f(nd.x, nd.z);
                eye = new_eye;
            };
            ImGui::Text("u (right):");
            ImGui::SameLine();
            if (ImGui::Button("-u##c"))
                lookAtTranslate(u, -translateStep);
            ImGui::SameLine();
            if (ImGui::Button("+u##c"))
                lookAtTranslate(u, translateStep);
            ImGui::Text("v (up):");
            ImGui::SameLine();
            if (ImGui::Button("-v##c"))
                lookAtTranslate(v, -translateStep);
            ImGui::SameLine();
            if (ImGui::Button("+v##c"))
                lookAtTranslate(v, translateStep);
            ImGui::Text("n (back):");
            ImGui::SameLine();
            if (ImGui::Button("-n##c"))
                lookAtTranslate(n, -translateStep);
            ImGui::SameLine();
            if (ImGui::Button("+n##c"))
                lookAtTranslate(n, translateStep);

            ImGui::SeparatorText("Projection");
            ImGui::SliderFloat("FOV", &fov, 10.0f, 120.0f);
            ImGui::SliderFloat("Near", &nearPlane, 0.01f, 10.0f);
            ImGui::SliderFloat("Far", &farPlane, 10.0f, 500.0f);
        }

        // ── Model ────────────────────────────────────────────────────────────
        if (ImGui::CollapsingHeader("Model"))
        {
            ImGui::Separator();
            ImGui::Combo("Type", &currentModel, "Cube\0Cow\0");
            ImGui::SliderFloat("Scale", &modelScale[currentModel], 0.001f, 0.01f);
            ImGui::ColorEdit3("Color", &color.r);
        }

        // ── Render ───────────────────────────────────────────────────────────
        if (ImGui::CollapsingHeader("Render"))
        {
            ImGui::Separator();
            ImGui::Combo("Draw Mode", &drawModeIdx, "Fill\0Wireframe\0Points\0");
            ImGui::Checkbox("Backface Culling", &backfaceCulling);
            ImGui::Combo("Front Face", &windingIdx, "CCW\0CW\0");
        }
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(program);
    glDeleteProgram(leanProgram);
    glfwTerminate();
    return 0;
}
