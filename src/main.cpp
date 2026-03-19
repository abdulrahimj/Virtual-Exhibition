/*  ============================================================
    Virtual Exhibition Space for Student Project Showcases
    Computer Graphics - Final Year Project
    
    Uses: OpenGL 4.3, GLFW, GLEW
    Build: make          (see Makefile)
    ============================================================ */

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::vector;
using std::ifstream;

/* ------------------------------------------------------------ */
/*  Constants                                                    */
/* ------------------------------------------------------------ */

static const int   WIN_W  = 1024;
static const int   WIN_H  = 768;
static const float PI     = 3.14159265358979323846f;
static const float DEG2RAD = PI / 180.0f;

/* Room dimensions */
static const float ROOM_W  = 14.0f;   /* width  (X) */
static const float ROOM_H  = 5.0f;    /* height (Y) */
static const float ROOM_D  = 20.0f;   /* depth  (Z) */

/* ------------------------------------------------------------ */
/*  Simple math helpers (column-major 4x4)                       */
/* ------------------------------------------------------------ */

static void mat4Identity(float m[16]) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void mat4Multiply(const float a[16], const float b[16], float out[16]) {
    float tmp[16];
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            tmp[c * 4 + r] = a[0 * 4 + r] * b[c * 4 + 0]
                            + a[1 * 4 + r] * b[c * 4 + 1]
                            + a[2 * 4 + r] * b[c * 4 + 2]
                            + a[3 * 4 + r] * b[c * 4 + 3];
    memcpy(out, tmp, sizeof(tmp));
}

static void mat4Perspective(float m[16], float fovY, float aspect,
                            float zNear, float zFar) {
    memset(m, 0, 16 * sizeof(float));
    float f = 1.0f / tanf(fovY * 0.5f);
    m[0]  = f / aspect;
    m[5]  = f;
    m[10] = (zFar + zNear) / (zNear - zFar);
    m[11] = -1.0f;
    m[14] = (2.0f * zFar * zNear) / (zNear - zFar);
}

static void mat4Translate(float m[16], float x, float y, float z) {
    mat4Identity(m);
    m[12] = x;  m[13] = y;  m[14] = z;
}

static void mat4Scale(float m[16], float sx, float sy, float sz) {
    mat4Identity(m);
    m[0] = sx;  m[5] = sy;  m[10] = sz;
}

static void mat4RotateY(float m[16], float rad) {
    mat4Identity(m);
    float c = cosf(rad), s = sinf(rad);
    m[0] = c;   m[2] = s;
    m[8] = -s;  m[10] = c;
}

static void mat4RotateX(float m[16], float rad) {
    mat4Identity(m);
    float c = cosf(rad), s = sinf(rad);
    m[5] = c;   m[6] = s;
    m[9] = -s;  m[10] = c;
}

static void mat4LookAt(float m[16],
                       float eyeX, float eyeY, float eyeZ,
                       float ctrX, float ctrY, float ctrZ,
                       float upX,  float upY,  float upZ) {
    float fx = ctrX - eyeX, fy = ctrY - eyeY, fz = ctrZ - eyeZ;
    float len = sqrtf(fx*fx + fy*fy + fz*fz);
    fx /= len; fy /= len; fz /= len;

    float sx = fy*upZ - fz*upY;
    float sy = fz*upX - fx*upZ;
    float sz = fx*upY - fy*upX;
    len = sqrtf(sx*sx + sy*sy + sz*sz);
    sx /= len; sy /= len; sz /= len;

    float ux = sy*fz - sz*fy;
    float uy = sz*fx - sx*fz;
    float uz = sx*fy - sy*fx;

    mat4Identity(m);
    m[0] = sx;  m[4] = sy;  m[8]  = sz;
    m[1] = ux;  m[5] = uy;  m[9]  = uz;
    m[2] = -fx; m[6] = -fy; m[10] = -fz;
    m[12] = -(sx*eyeX + sy*eyeY + sz*eyeZ);
    m[13] = -(ux*eyeX + uy*eyeY + uz*eyeZ);
    m[14] =  (fx*eyeX + fy*eyeY + fz*eyeZ);
}

/* ------------------------------------------------------------ */
/*  BMP loader (24-bit uncompressed, no dependency)              */
/* ------------------------------------------------------------ */

struct BMPImage {
    int width  = 0;
    int height = 0;
    vector<unsigned char> data;   /* RGB */
};

static bool loadBMP(const char* path, BMPImage& img) {
    ifstream f(path, std::ios::binary);
    if (!f.is_open()) { cerr << "Cannot open BMP: " << path << endl; return false; }

    unsigned char header[54];
    f.read(reinterpret_cast<char*>(header), 54);
    if (header[0] != 'B' || header[1] != 'M') {
        cerr << "Not a BMP file: " << path << endl; return false;
    }

    int dataOffset = *(int*)&header[10];
    img.width      = *(int*)&header[18];
    img.height     = *(int*)&header[22];
    short bpp      = *(short*)&header[28];

    if (bpp != 24) {
        cerr << "Only 24-bit BMP supported (" << path << " is " << bpp << "-bit)" << endl;
        return false;
    }

    int rowSize = ((img.width * 3 + 3) / 4) * 4;
    int dataSize = rowSize * img.height;
    vector<unsigned char> raw(dataSize);
    f.seekg(dataOffset);
    f.read(reinterpret_cast<char*>(raw.data()), dataSize);

    /* BMP stores BGR bottom-to-top (adjusted) - convert to RGB top-to-bottom */
    img.data.resize(img.width * img.height * 3);
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            int dstIdx = (y * img.width + x) * 3;
            int mirrorX = img.width - 1 - x;
            int mirrorIdx = y * rowSize + mirrorX * 3;
            img.data[dstIdx + 0] = raw[mirrorIdx + 2]; /* R */
            img.data[dstIdx + 1] = raw[mirrorIdx + 1]; /* G */
            img.data[dstIdx + 2] = raw[mirrorIdx + 0]; /* B */
        }
    }
    return true;
}

static GLuint uploadTexture(const BMPImage& img) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                 img.width, img.height, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, img.data.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    return tex;
}

/* ------------------------------------------------------------ */
/*  Shader helpers                                               */
/* ------------------------------------------------------------ */

static string readFile(const char* path) {
    ifstream f(path);
    if (!f.is_open()) { cerr << "Cannot open: " << path << endl; return ""; }
    string s((std::istreambuf_iterator<char>(f)),
              std::istreambuf_iterator<char>());
    return s;
}

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);

    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, 512, nullptr, log);
        cerr << "Shader compile error:\n" << log << endl;
    }
    return s;
}

static GLuint createProgram(const char* vertPath, const char* fragPath) {
    string vs = readFile(vertPath);
    string fs = readFile(fragPath);
    GLuint v = compileShader(GL_VERTEX_SHADER,   vs.c_str());
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fs.c_str());
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);

    GLint ok;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(p, 512, nullptr, log);
        cerr << "Program link error:\n" << log << endl;
    }
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

/* ------------------------------------------------------------ */
/*  Geometry builders                                            */
/* ------------------------------------------------------------ */

struct Mesh {
    GLuint vao = 0, vbo = 0;
    GLsizei vertexCount = 0;
};

/*  Each vertex: pos(3) + normal(3) + texcoord(2) = 8 floats */

static Mesh buildQuad() {
    float v[] = {
        /* pos              normal          uv        */
        -0.5f, -0.5f, 0.0f,  0, 0, 1,    0.0f, 0.0f,
         0.5f, -0.5f, 0.0f,  0, 0, 1,    1.0f, 0.0f,
         0.5f,  0.5f, 0.0f,  0, 0, 1,    1.0f, 1.0f,

        -0.5f, -0.5f, 0.0f,  0, 0, 1,    0.0f, 0.0f,
         0.5f,  0.5f, 0.0f,  0, 0, 1,    1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f,  0, 0, 1,    0.0f, 1.0f,
    };
    Mesh m;
    m.vertexCount = 6;
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glBindVertexArray(0);
    return m;
}

static Mesh buildCube() {
    float v[] = {
        /* ---- FRONT (+Z) ---- */
        -0.5f, -0.5f,  0.5f,   0, 0, 1,   0, 0,
         0.5f, -0.5f,  0.5f,   0, 0, 1,   1, 0,
         0.5f,  0.5f,  0.5f,   0, 0, 1,   1, 1,
        -0.5f, -0.5f,  0.5f,   0, 0, 1,   0, 0,
         0.5f,  0.5f,  0.5f,   0, 0, 1,   1, 1,
        -0.5f,  0.5f,  0.5f,   0, 0, 1,   0, 1,
        /* ---- BACK (-Z) ---- */
         0.5f, -0.5f, -0.5f,   0, 0,-1,   0, 0,
        -0.5f, -0.5f, -0.5f,   0, 0,-1,   1, 0,
        -0.5f,  0.5f, -0.5f,   0, 0,-1,   1, 1,
         0.5f, -0.5f, -0.5f,   0, 0,-1,   0, 0,
        -0.5f,  0.5f, -0.5f,   0, 0,-1,   1, 1,
         0.5f,  0.5f, -0.5f,   0, 0,-1,   0, 1,
        /* ---- LEFT (-X) ---- */
        -0.5f, -0.5f, -0.5f,  -1, 0, 0,   0, 0,
        -0.5f, -0.5f,  0.5f,  -1, 0, 0,   1, 0,
        -0.5f,  0.5f,  0.5f,  -1, 0, 0,   1, 1,
        -0.5f, -0.5f, -0.5f,  -1, 0, 0,   0, 0,
        -0.5f,  0.5f,  0.5f,  -1, 0, 0,   1, 1,
        -0.5f,  0.5f, -0.5f,  -1, 0, 0,   0, 1,
        /* ---- RIGHT (+X) ---- */
         0.5f, -0.5f,  0.5f,   1, 0, 0,   0, 0,
         0.5f, -0.5f, -0.5f,   1, 0, 0,   1, 0,
         0.5f,  0.5f, -0.5f,   1, 0, 0,   1, 1,
         0.5f, -0.5f,  0.5f,   1, 0, 0,   0, 0,
         0.5f,  0.5f, -0.5f,   1, 0, 0,   1, 1,
         0.5f,  0.5f,  0.5f,   1, 0, 0,   0, 1,
        /* ---- TOP (+Y) ---- */
        -0.5f,  0.5f,  0.5f,   0, 1, 0,   0, 0,
         0.5f,  0.5f,  0.5f,   0, 1, 0,   1, 0,
         0.5f,  0.5f, -0.5f,   0, 1, 0,   1, 1,
        -0.5f,  0.5f,  0.5f,   0, 1, 0,   0, 0,
         0.5f,  0.5f, -0.5f,   0, 1, 0,   1, 1,
        -0.5f,  0.5f, -0.5f,   0, 1, 0,   0, 1,
        /* ---- BOTTOM (-Y) ---- */
        -0.5f, -0.5f, -0.5f,   0,-1, 0,   0, 0,
         0.5f, -0.5f, -0.5f,   0,-1, 0,   1, 0,
         0.5f, -0.5f,  0.5f,   0,-1, 0,   1, 1,
        -0.5f, -0.5f, -0.5f,   0,-1, 0,   0, 0,
         0.5f, -0.5f,  0.5f,   0,-1, 0,   1, 1,
        -0.5f, -0.5f,  0.5f,   0,-1, 0,   0, 1,
    };

    Mesh m;
    m.vertexCount = 36;
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(6*sizeof(float)));
    glBindVertexArray(0);
    return m;
}

/* Simple sphere made of lat/long strips */
static Mesh buildSphere(int stacks, int slices) {
    vector<float> verts;
    for (int i = 0; i < stacks; ++i) {
        float lat0 = PI * (-0.5f + (float)i / stacks);
        float lat1 = PI * (-0.5f + (float)(i + 1) / stacks);
        float y0 = sinf(lat0), y1 = sinf(lat1);
        float r0 = cosf(lat0), r1 = cosf(lat1);

        for (int j = 0; j < slices; ++j) {
            float lng0 = 2.0f * PI * (float)j / slices;
            float lng1 = 2.0f * PI * (float)(j + 1) / slices;

            float x00 = r0*cosf(lng0), z00 = r0*sinf(lng0);
            float x01 = r0*cosf(lng1), z01 = r0*sinf(lng1);
            float x10 = r1*cosf(lng0), z10 = r1*sinf(lng0);
            float x11 = r1*cosf(lng1), z11 = r1*sinf(lng1);

            float u0 = (float)j / slices, u1 = (float)(j+1) / slices;
            float v0 = (float)i / stacks, v1 = (float)(i+1) / stacks;

            auto push = [&](float px, float py, float pz, float u, float v) {
                float len = sqrtf(px*px+py*py+pz*pz);
                verts.insert(verts.end(), {px, py, pz,
                    px/len, py/len, pz/len, u, v});
            };

            push(x00, y0, z00, u0, v0);
            push(x10, y1, z10, u0, v1);
            push(x11, y1, z11, u1, v1);

            push(x00, y0, z00, u0, v0);
            push(x11, y1, z11, u1, v1);
            push(x01, y0, z01, u1, v0);
        }
    }

    Mesh m;
    m.vertexCount = (GLsizei)(verts.size() / 8);
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(6*sizeof(float)));
    glBindVertexArray(0);
    return m;
}

/* Cylinder along Y axis, height 1, radius 0.5 */
static Mesh buildCylinder(int slices) {
    vector<float> verts;
    for (int i = 0; i < slices; ++i) {
        float a0 = 2.0f*PI*(float)i/slices;
        float a1 = 2.0f*PI*(float)(i+1)/slices;
        float c0 = cosf(a0)*0.5f, s0 = sinf(a0)*0.5f;
        float c1 = cosf(a1)*0.5f, s1 = sinf(a1)*0.5f;
        float u0 = (float)i/slices, u1 = (float)(i+1)/slices;

        /* side */
        auto pushS = [&](float x, float z, float y, float u, float v) {
            float len = sqrtf(x*x+z*z);
            verts.insert(verts.end(), {x, y, z, x/len, 0, z/len, u, v});
        };
        pushS(c0, s0, -0.5f, u0, 0);
        pushS(c1, s1, -0.5f, u1, 0);
        pushS(c1, s1,  0.5f, u1, 1);
        pushS(c0, s0, -0.5f, u0, 0);
        pushS(c1, s1,  0.5f, u1, 1);
        pushS(c0, s0,  0.5f, u0, 1);

        /* top cap */
        verts.insert(verts.end(), {0,0.5f,0,  0,1,0,  0.5f,0.5f});
        verts.insert(verts.end(), {c0,0.5f,s0, 0,1,0,  u0,0});
        verts.insert(verts.end(), {c1,0.5f,s1, 0,1,0,  u1,0});

        /* bottom cap */
        verts.insert(verts.end(), {0,-0.5f,0,  0,-1,0, 0.5f,0.5f});
        verts.insert(verts.end(), {c1,-0.5f,s1, 0,-1,0, u1,0});
        verts.insert(verts.end(), {c0,-0.5f,s0, 0,-1,0, u0,0});
    }

    Mesh m;
    m.vertexCount = (GLsizei)(verts.size()/8);
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,8*sizeof(float),(void*)(6*sizeof(float)));
    glBindVertexArray(0);
    return m;
}

/* ------------------------------------------------------------ */
/*  Globals                                                      */
/* ------------------------------------------------------------ */

static GLuint g_program = 0;
static Mesh   g_quad, g_cube, g_sphere, g_cylinder;

/* Textures */
static GLuint g_texFloor = 0;
static GLuint g_texWall  = 0;
static const int NUM_PROJECTS = 6;
static GLuint g_texProject[NUM_PROJECTS];

/* Camera (first-person) */
static float g_camX =  0.0f;
static float g_camY =  1.6f;   /* eye height */
static float g_camZ =  8.0f;
static float g_camYaw   = PI;  /* looking toward -Z initially */
static float g_camPitch = 0.0f;

static bool  g_firstMouse = true;
static double g_lastMX = 0, g_lastMY = 0;
static bool  g_cursorCaptured = true;

/* ------------------------------------------------------------ */
/*  Draw helper                                                  */
/* ------------------------------------------------------------ */

static void setModel(GLuint prog, const float m[16]) {
    glUniformMatrix4fv(glGetUniformLocation(prog, "u_model"), 1, GL_FALSE, m);
}

static void setColor(GLuint prog, float r, float g, float b) {
    glUniform3f(glGetUniformLocation(prog, "u_objectColor"), r, g, b);
}

static void setUseTexture(GLuint prog, bool use) {
    glUniform1i(glGetUniformLocation(prog, "u_useTexture"), use ? 1 : 0);
}

static void drawMesh(const Mesh& m) {
    glBindVertexArray(m.vao);
    glDrawArrays(GL_TRIANGLES, 0, m.vertexCount);
}

/* Build model matrix = Translation * RotationY * RotationX * Scale */
static void buildModelMatrix(float out[16],
                              float tx, float ty, float tz,
                              float rx, float ry, float rz,
                              float sx, float sy, float sz) {
    float T[16], Rx[16], Ry[16], S[16], tmp1[16], tmp2[16];
    mat4Translate(T, tx, ty, tz);
    mat4RotateX(Rx, rx);
    mat4RotateY(Ry, ry);
    mat4Scale(S, sx, sy, sz);

    mat4Multiply(T, Ry, tmp1);
    mat4Multiply(tmp1, Rx, tmp2);
    mat4Multiply(tmp2, S, out);
}

/* ------------------------------------------------------------ */
/*  Scene drawing                                                */
/* ------------------------------------------------------------ */

static void drawRoom(GLuint prog) {
    float model[16];

    /* --- Floor --- */
    setUseTexture(prog, true);
    glBindTexture(GL_TEXTURE_2D, g_texFloor);
    buildModelMatrix(model,
        0.0f, 0.0f, ROOM_D*0.5f,
        -PI*0.5f, 0.0f, 0.0f,
        ROOM_W, ROOM_D, 1.0f);
    setModel(prog, model);
    drawMesh(g_quad);

    /* --- Ceiling --- */
    glBindTexture(GL_TEXTURE_2D, g_texWall);
    buildModelMatrix(model,
        0.0f, ROOM_H, ROOM_D*0.5f,
        PI*0.5f, 0.0f, 0.0f,
        ROOM_W, ROOM_D, 1.0f);
    setModel(prog, model);
    drawMesh(g_quad);

    /* --- Back wall (-Z) --- */
    buildModelMatrix(model,
        0.0f, ROOM_H*0.5f, 0.0f,
        0.0f, 0.0f, 0.0f,
        ROOM_W, ROOM_H, 1.0f);
    setModel(prog, model);
    drawMesh(g_quad);

    /* --- Front wall (+Z) --- */
    buildModelMatrix(model,
        0.0f, ROOM_H*0.5f, ROOM_D,
        0.0f, PI, 0.0f,
        ROOM_W, ROOM_H, 1.0f);
    setModel(prog, model);
    drawMesh(g_quad);

    /* --- Left wall --- */
    buildModelMatrix(model,
        -ROOM_W*0.5f, ROOM_H*0.5f, ROOM_D*0.5f,
        0.0f, PI*0.5f, 0.0f,
        ROOM_D, ROOM_H, 1.0f);
    setModel(prog, model);
    drawMesh(g_quad);

    /* --- Right wall --- */
    buildModelMatrix(model,
        ROOM_W*0.5f, ROOM_H*0.5f, ROOM_D*0.5f,
        0.0f, -PI*0.5f, 0.0f,
        ROOM_D, ROOM_H, 1.0f);
    setModel(prog, model);
    drawMesh(g_quad);
}

static void drawProjectFrames(GLuint prog) {
    float model[16];
    float frameW = 2.5f, frameH = 1.8f;

    for (int i = 0; i < NUM_PROJECTS; ++i) {
        bool leftWall = (i < 3);
        int idx = leftWall ? i : (i - 3);

        float x = leftWall ? (-ROOM_W * 0.5f + 0.05f) : (ROOM_W * 0.5f - 0.05f);
        float y = ROOM_H * 0.55f;
        float z = 3.0f + idx * 5.5f;
        float rotY = leftWall ? (PI * 0.5f) : (-PI * 0.5f);

        /* Dark frame border */
        setUseTexture(prog, false);
        setColor(prog, 0.15f, 0.10f, 0.05f);
        buildModelMatrix(model, x, y, z, 0, rotY, 0,
                         0.1f, frameH + 0.2f, frameW + 0.2f);
        setModel(prog, model);
        drawMesh(g_cube);

        /* Project image */
        float offset = leftWall ? 0.08f : -0.08f;
        setUseTexture(prog, true);
        glBindTexture(GL_TEXTURE_2D, g_texProject[i]);
        buildModelMatrix(model, x + offset, y, z, 0, rotY, 0,
                         frameW, frameH, 1.0f);
        setModel(prog, model);
        drawMesh(g_quad);
    }
}

static void drawPedestals(GLuint prog) {
    float model[16];

    struct PedestalObj {
        float z;
        float r, g, b;
        int   type;       /* 0 = sphere, 1 = cube, 2 = cylinder */
    };

    PedestalObj peds[] = {
        { 4.0f,   0.9f, 0.2f, 0.2f,  0 },  /* red sphere */
        { 10.0f,  0.2f, 0.7f, 0.3f,  1 },  /* green cube */
        { 16.0f,  0.2f, 0.3f, 0.9f,  2 },  /* blue cylinder */
    };

    for (auto& p : peds) {
        /* Pedestal base */
        setUseTexture(prog, false);
        setColor(prog, 0.75f, 0.75f, 0.78f);
        buildModelMatrix(model, 0, 0.5f, p.z, 0, 0, 0,
                         1.0f, 1.0f, 1.0f);
        setModel(prog, model);
        drawMesh(g_cube);

        /* Object on top */
        setColor(prog, p.r, p.g, p.b);
        float objY = 1.0f + 0.35f;

        if (p.type == 0) {
            buildModelMatrix(model, 0, objY + 0.15f, p.z, 0, 0, 0,
                             0.6f, 0.6f, 0.6f);
            setModel(prog, model);
            drawMesh(g_sphere);
        } else if (p.type == 1) {
            float angle = (float)glfwGetTime() * 0.8f;
            buildModelMatrix(model, 0, objY + 0.1f, p.z, 0.3f, angle, 0,
                             0.5f, 0.5f, 0.5f);
            setModel(prog, model);
            drawMesh(g_cube);
        } else {
            buildModelMatrix(model, 0, objY + 0.1f, p.z, 0, 0, 0,
                             0.5f, 0.7f, 0.5f);
            setModel(prog, model);
            drawMesh(g_cylinder);
        }
    }
}

static void drawScene(GLuint prog) {
    drawRoom(prog);
    drawProjectFrames(prog);
    drawPedestals(prog);
}

/* ------------------------------------------------------------ */
/*  GLFW callbacks                                               */
/* ------------------------------------------------------------ */

static void mouseCallback(GLFWwindow* w, double xpos, double ypos) {
    if (!g_cursorCaptured) return;
    if (g_firstMouse) { g_lastMX = xpos; g_lastMY = ypos; g_firstMouse = false; }
    float dx = (float)(xpos - g_lastMX);
    float dy = (float)(g_lastMY - ypos);
    g_lastMX = xpos;
    g_lastMY = ypos;

    float sensitivity = 0.003f;
    g_camYaw   += dx * sensitivity;
    g_camPitch += dy * sensitivity;
    if (g_camPitch >  1.4f) g_camPitch =  1.4f;
    if (g_camPitch < -1.4f) g_camPitch = -1.4f;
}

static void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        g_cursorCaptured = !g_cursorCaptured;
        glfwSetInputMode(w, GLFW_CURSOR,
            g_cursorCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        g_firstMouse = true;
    }
}

/* ------------------------------------------------------------ */
/*  Main                                                         */
/* ------------------------------------------------------------ */

int main() {
    /* ---- GLFW init ---- */
    if (!glfwInit()) { cerr << "GLFW init failed" << endl; return 1; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WIN_W, WIN_H,
        "Virtual Exhibition Space - Student Project Showcase", nullptr, nullptr);
    if (!window) { cerr << "Window creation failed" << endl; glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);

    /* ---- GLEW init ---- */
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { cerr << "GLEW init failed" << endl; return 1; }

    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSwapInterval(1);

    /* ---- Shader ---- */
    g_program = createProgram("shaders/vertShader.glsl", "shaders/fragShader.glsl");

    /* ---- Geometry ---- */
    g_quad     = buildQuad();
    g_cube     = buildCube();
    g_sphere   = buildSphere(24, 36);
    g_cylinder = buildCylinder(36);

    /* ---- Textures ---- */
    {
        BMPImage img;
        if (loadBMP("textures/floor.bmp", img)) g_texFloor = uploadTexture(img);
        if (loadBMP("textures/wall.bmp",  img)) g_texWall  = uploadTexture(img);
        for (int i = 0; i < NUM_PROJECTS; ++i) {
            char path[64];
            snprintf(path, sizeof(path), "textures/project%d.bmp", i + 1);
            if (loadBMP(path, img))
                g_texProject[i] = uploadTexture(img);
        }
    }

    /* ---- GL state ---- */
    glEnable(GL_DEPTH_TEST);

    cout << "=== Virtual Exhibition Space ===" << endl;
    cout << "Controls:" << endl;
    cout << "  W/A/S/D  - Walk forward/left/backward/right" << endl;
    cout << "  Mouse    - Look around" << endl;
    cout << "  ESC      - Release/capture mouse" << endl;
    cout << "================================" << endl;

    /* ---- Main loop ---- */
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;

        /* -- Input -- */
        float speed = 3.5f * dt;

        /* Forward direction on XZ plane */
        float fwdX = -sinf(g_camYaw);
        float fwdZ =  cosf(g_camYaw);

        /* Strafe direction */
        float rightX = cosf(g_camYaw);
        float rightZ = sinf(g_camYaw);

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            g_camX += fwdX * speed;
            g_camZ += fwdZ * speed;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            g_camX -= fwdX * speed;
            g_camZ -= fwdZ * speed;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            g_camX -= rightX * speed;
            g_camZ -= rightZ * speed;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            g_camX += rightX * speed;
            g_camZ += rightZ * speed;
        }

        /* Simple room boundary clamping */
        float margin = 0.5f;
        if (g_camX < -ROOM_W*0.5f + margin) g_camX = -ROOM_W*0.5f + margin;
        if (g_camX >  ROOM_W*0.5f - margin) g_camX =  ROOM_W*0.5f - margin;
        if (g_camZ < margin)                 g_camZ = margin;
        if (g_camZ > ROOM_D - margin)        g_camZ = ROOM_D - margin;

        /* -- Render -- */
        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(g_program);

        /* Projection */
        float proj[16];
        float aspect = (fbH > 0) ? (float)fbW / (float)fbH : 1.0f;
        mat4Perspective(proj, 60.0f * DEG2RAD, aspect, 0.1f, 100.0f);
        glUniformMatrix4fv(glGetUniformLocation(g_program, "u_proj"),
                           1, GL_FALSE, proj);

        /* View */
        float lookX = g_camX + cosf(g_camPitch) * (-sinf(g_camYaw));
        float lookY = g_camY + sinf(g_camPitch);
        float lookZ = g_camZ + cosf(g_camPitch) * cosf(g_camYaw);

        float view[16];
        mat4LookAt(view,
                   g_camX, g_camY, g_camZ,
                   lookX, lookY, lookZ,
                   0, 1, 0);
        glUniformMatrix4fv(glGetUniformLocation(g_program, "u_view"),
                           1, GL_FALSE, view);

        /* Lighting */
        glUniform3f(glGetUniformLocation(g_program, "u_lightPos"),
                    0.0f, ROOM_H - 0.3f, ROOM_D * 0.5f);
        glUniform3f(glGetUniformLocation(g_program, "u_viewPos"),
                    g_camX, g_camY, g_camZ);
        glUniform3f(glGetUniformLocation(g_program, "u_lightColor"),
                    1.0f, 0.95f, 0.9f);
        glUniform1f(glGetUniformLocation(g_program, "u_ambientStrength"), 0.25f);

        drawScene(g_program);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}