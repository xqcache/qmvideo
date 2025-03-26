#include "qmvideoplayer.h"
#include <QFile>
#include <QOpenGLShader>
#include <QOpenGLShaderProgram>

namespace {
const char* vertexShaderSrc = R"(
        #version 330 core
        layout(location = 0) in vec3 position;
        layout(location = 1) in vec2 texCoord;
        out vec2 TexCoord;
        void main() {
            gl_Position = vec4(position, 1.0);
            TexCoord = vec2(texCoord.x, 1.0 - texCoord.y);
        }
    )";

const char* fragmentShaderSrc = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoord;
        uniform sampler2D tex_y, tex_u, tex_v;
        void main() {
            float y = texture(tex_y, TexCoord).r;
            float u = texture(tex_u, TexCoord).r - 0.5;
            float v = texture(tex_v, TexCoord).r - 0.5;
            float r = y + 1.402 * v;
            float g = y - 0.344 * u - 0.714 * v;
            float b = y + 1.772 * u;
            FragColor = vec4(r, g, b, 1.0);
        }
    )";

// 设置四边形顶点数据
// clang-format off
float vertices[] = {
        // 位置              // 纹理坐标（修正顺序）
        -1.0f,  1.0f, 0.0f,  0.0f, 1.0f,  // 左上角
        -1.0f, -1.0f, 0.0f,  0.0f, 0.0f,  // 左下角
         1.0f, -1.0f, 0.0f,  1.0f, 0.0f,  // 右下角
         1.0f,  1.0f, 0.0f,  1.0f, 1.0f   // 右上角
};

GLuint indices[] = { 0, 1, 2,  0, 2, 3 }; // 采用索引方式绘制四边形
// clang-format on

QOpenGLShaderProgram shaderProgram;
const int videoWidth = 1254;
const int videoHeight = 940;
GLuint vao, vbo, ebo;
GLuint tex_y, tex_u, tex_v;

char* yuvData;
}

QmVideoPlayer::QmVideoPlayer(QWidget* parent) { }

QmVideoPlayer::~QmVideoPlayer() noexcept
{
}

void QmVideoPlayer::initializeGL()
{
    initializeOpenGLFunctions();
    yuvData = new char[videoWidth * videoHeight * 3 / 2];
    {
        QFile file("d:/first_frame.yuv");
        file.open(QFile::ReadOnly);
        file.read(yuvData, videoWidth * videoHeight * 3 / 2);
        qDebug() << file.size() << (videoWidth * videoHeight * 3 / 2);
    }

    shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSrc);
    shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSrc);
    qDebug() << "Shader program link: " << shaderProgram.link();

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glGenTextures(1, &tex_y);
    glGenTextures(1, &tex_u);
    glGenTextures(1, &tex_v);

    // Y 纹理
    glBindTexture(GL_TEXTURE_2D, tex_y);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, videoWidth, videoHeight, 0, GL_RED, GL_UNSIGNED_BYTE, yuvData);

    // U 纹理
    glBindTexture(GL_TEXTURE_2D, tex_u);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, videoWidth / 2, videoHeight / 2, 0, GL_RED, GL_UNSIGNED_BYTE, yuvData + videoWidth * videoHeight);

    // V 纹理
    glBindTexture(GL_TEXTURE_2D, tex_v);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, videoWidth / 2, videoHeight / 2, 0, GL_RED, GL_UNSIGNED_BYTE, yuvData + videoWidth * videoHeight * 5 / 4);
}

void QmVideoPlayer::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void QmVideoPlayer::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);

    shaderProgram.bind();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_y);
    shaderProgram.setUniformValue("tex_y", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex_u);
    shaderProgram.setUniformValue("tex_u", 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, tex_v);
    shaderProgram.setUniformValue("tex_v", 2);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}
