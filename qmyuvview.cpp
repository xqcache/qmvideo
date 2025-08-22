#include "qmyuvview.h"
#include "qmvideodecoder.h"
#include <QFile>
#include <QKeyEvent>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLPixelTransferOptions>
#include <QOpenGLShader>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QTimer>

namespace {

// clang-format off

// 纹理坐标左下角为(0, 0)，右上角为（1, 1）
// 顶点坐标左下角为（-1, -1），右上角为（1，1）
constexpr GLfloat kVertices[] = {
        // 位置              // 纹理坐标（修正顺序）
        -1.0f,  1.0f,  0.0f, 1.0f,  // 左上角
        -1.0f, -1.0f, 0.0f, 0.0f,  // 左下角
         1.0f, -1.0f, 1.0f, 0.0f,  // 右下角
         1.0f,  1.0f,  1.0f, 1.0f   // 右上角
};
constexpr GLuint kIndices[] = {
    0, 1, 2,
    0, 2, 3
};
// clang-format on

constexpr std::string_view kVertexShaderCode = R"(
    #version 330 core
    layout (location = 0) in vec2 aPos;
    layout (location = 1) in vec2 aTexCoord;
    out vec2 TexCoord;
    void main() {
        gl_Position = vec4(aPos, 0, 1);
        TexCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y);
    }
)";

constexpr std::string_view kFragmentShaderCode = R"(
    #version 330 core
    out vec4 FragColor;
    in vec2 TexCoord;
    uniform sampler2D y_tex;
    uniform sampler2D u_tex;
    uniform sampler2D v_tex;
    void main() {
        // YUV数据转换为RGB数据
        float y = texture(y_tex, TexCoord).r;
        float u = texture(u_tex, TexCoord).r - 0.5;
        float v = texture(v_tex, TexCoord).r - 0.5;
        float r = y + 1.402 * v;
        float g = y - 0.344 * u - 0.714 * v;
        float b = y + 1.772 * u;
        FragColor = vec4(r, g, b, 1.0);
    }
)";

GLuint tex_y, tex_u, tex_v;
}

class QmYuvViewPrivate : private QOpenGLFunctions_3_3_Core {
public:
    QmYuvViewPrivate(QmYuvView* q);
    ~QmYuvViewPrivate() noexcept;

    void init();
    void paint();

    void setSize(const QSize& yuv_size);
    void setBuffer(const QByteArray& yuv_buf);

private:
    QmYuvView* q_ { nullptr };
    QOpenGLShaderProgram* program_ { nullptr };
    QOpenGLVertexArrayObject* vao_ { nullptr };
    std::unique_ptr<QOpenGLBuffer> vbo_;
    std::unique_ptr<QOpenGLBuffer> ebo_;

    std::unique_ptr<QOpenGLTexture> tex_y_;
    std::unique_ptr<QOpenGLTexture> tex_u_;
    std::unique_ptr<QOpenGLTexture> tex_v_;

    QSize yuv_size_ { 1254, 940 };
    QByteArray yuv_buf_;
};

QmYuvViewPrivate::QmYuvViewPrivate(QmYuvView* q)
    : q_(q)
    , program_(new QOpenGLShaderProgram(q))
    , vao_(new QOpenGLVertexArrayObject(q))
    , vbo_(new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer))
    , ebo_(new QOpenGLBuffer(QOpenGLBuffer::IndexBuffer))
    , tex_y_(new QOpenGLTexture(QOpenGLTexture::Target2D))
    , tex_u_(new QOpenGLTexture(QOpenGLTexture::Target2D))
    , tex_v_(new QOpenGLTexture(QOpenGLTexture::Target2D))
{
}

QmYuvViewPrivate::~QmYuvViewPrivate() noexcept
{
}

void QmYuvViewPrivate::init()
{
    initializeOpenGLFunctions();

    vao_->create();
    vao_->bind();

    vbo_->create();
    vbo_->bind();
    vbo_->setUsagePattern(QOpenGLBuffer::StaticDraw);
    vbo_->allocate(kVertices, sizeof(kVertices));

    ebo_->create();
    ebo_->bind();
    ebo_->setUsagePattern(QOpenGLBuffer::StaticDraw);
    ebo_->allocate(kIndices, sizeof(kIndices));

    program_->create();
    program_->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShaderCode.data());
    program_->addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShaderCode.data());
    program_->link();
    program_->bind();

    program_->setAttributeArray(0, GL_FLOAT, nullptr, 2, 4 * sizeof(GLfloat));
    program_->enableAttributeArray(0);

    program_->setAttributeArray(1, GL_FLOAT, (void*)(2 * sizeof(GLfloat)), 2, 4 * sizeof(GLfloat));
    program_->enableAttributeArray(1);

    // 在OpenGL上下文中初始化纹理
    tex_y_->create();
    tex_y_->setFormat(QOpenGLTexture::R8_UNorm);
    tex_y_->setMinificationFilter(QOpenGLTexture::Linear);
    tex_y_->setMagnificationFilter(QOpenGLTexture::Linear);
    tex_y_->setWrapMode(QOpenGLTexture::WrapMode::ClampToEdge);
    tex_y_->setSize(yuv_size_.width(), yuv_size_.height());
    tex_y_->allocateStorage(QOpenGLTexture::Red, QOpenGLTexture::UInt8);

    tex_u_->create();
    tex_u_->setFormat(QOpenGLTexture::R8_UNorm);
    tex_u_->setMinificationFilter(QOpenGLTexture::Linear);
    tex_u_->setMagnificationFilter(QOpenGLTexture::Linear);
    tex_u_->setWrapMode(QOpenGLTexture::WrapMode::ClampToEdge);
    tex_u_->setSize(yuv_size_.width() / 2, yuv_size_.height() / 2);
    tex_u_->allocateStorage(QOpenGLTexture::Red, QOpenGLTexture::UInt8);

    tex_v_->create();
    tex_v_->setFormat(QOpenGLTexture::R8_UNorm);
    tex_v_->setMinificationFilter(QOpenGLTexture::Linear);
    tex_v_->setMagnificationFilter(QOpenGLTexture::Linear);
    tex_v_->setWrapMode(QOpenGLTexture::WrapMode::ClampToEdge);
    tex_v_->setSize(yuv_size_.width() / 2, yuv_size_.height() / 2);
    tex_v_->allocateStorage(QOpenGLTexture::Red, QOpenGLTexture::UInt8);
}

void QmYuvViewPrivate::setSize(const QSize& size)
{
    if (yuv_size_ == size) {
        return;
    }
    yuv_size_ = size;
    q_->makeCurrent();
    // 在OpenGL上下文中初始化纹理
    if (tex_y_->isStorageAllocated()) {
        tex_y_->destroy();
        tex_y_->create();
        tex_y_->setFormat(QOpenGLTexture::R8_UNorm);
        tex_y_->setMinificationFilter(QOpenGLTexture::Linear);
        tex_y_->setMagnificationFilter(QOpenGLTexture::Linear);
        tex_y_->setWrapMode(QOpenGLTexture::WrapMode::ClampToEdge);
        tex_y_->setSize(size.width(), size.height());
        tex_y_->allocateStorage(QOpenGLTexture::Red, QOpenGLTexture::UInt8);
    } else {
        tex_y_->setSize(size.width(), size.height());
        tex_y_->allocateStorage(QOpenGLTexture::Red, QOpenGLTexture::UInt8);
    }
    if (tex_u_->isStorageAllocated()) {
        tex_u_->destroy();
        tex_u_->create();
        tex_u_->setFormat(QOpenGLTexture::R8_UNorm);
        tex_u_->setMinificationFilter(QOpenGLTexture::Linear);
        tex_u_->setMagnificationFilter(QOpenGLTexture::Linear);
        tex_u_->setWrapMode(QOpenGLTexture::WrapMode::ClampToEdge);
        tex_u_->setSize(size.width() / 2, size.height() / 2);
        tex_u_->allocateStorage(QOpenGLTexture::Red, QOpenGLTexture::UInt8);
    } else {
        tex_u_->setSize(size.width() / 2, size.height() / 2);
        tex_u_->allocateStorage(QOpenGLTexture::Red, QOpenGLTexture::UInt8);
    }
    if (tex_v_->isStorageAllocated()) {
        tex_v_->destroy();
        tex_v_->create();
        tex_v_->setFormat(QOpenGLTexture::R8_UNorm);
        tex_v_->setMinificationFilter(QOpenGLTexture::Linear);
        tex_v_->setMagnificationFilter(QOpenGLTexture::Linear);
        tex_v_->setWrapMode(QOpenGLTexture::WrapMode::ClampToEdge);
        tex_v_->setSize(size.width() / 2, size.height() / 2);
        tex_v_->allocateStorage(QOpenGLTexture::Red, QOpenGLTexture::UInt8);
    } else {
        tex_v_->setSize(size.width() / 2, size.height() / 2);
        tex_v_->allocateStorage(QOpenGLTexture::Red, QOpenGLTexture::UInt8);
    }
    q_->doneCurrent();
}

void QmYuvViewPrivate::setBuffer(const QByteArray& yuv_buf)
{
    yuv_buf_ = yuv_buf;
}

void QmYuvViewPrivate::paint()
{
    glClear(GL_COLOR_BUFFER_BIT);

    // 检查是否有有效的YUV数据
    if (yuv_buf_.isEmpty()) {
        return; // 没有有效数据时不渲染
    }

    program_->bind();

    QOpenGLPixelTransferOptions options;
    options.setAlignment(1);
    tex_y_->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, yuv_buf_.constData(), &options);
    tex_u_->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, yuv_buf_.constData() + yuv_size_.width() * yuv_size_.height(), &options);
    tex_v_->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, yuv_buf_.constData() + yuv_size_.width() * yuv_size_.height() * 5 / 4, &options);

    tex_y_->bind(0);
    tex_u_->bind(1);
    tex_v_->bind(2);

    program_->setUniformValue("y_tex", 0);
    program_->setUniformValue("u_tex", 1);
    program_->setUniformValue("v_tex", 2);

    vao_->bind();
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
}

QmYuvView::QmYuvView(QWidget* parent)
    : QOpenGLWidget(parent)
    , d_(new QmYuvViewPrivate(this))
{
}

QmYuvView::~QmYuvView() noexcept
{
    delete d_;
}

void QmYuvView::initializeGL()
{
    d_->init();
}

void QmYuvView::resizeGL(int w, int h)
{
}

void QmYuvView::paintGL()
{
    d_->paint();
}

void QmYuvView::setData(const QByteArray& yuv_data, const QSize& yuv_size)
{
    d_->setBuffer(yuv_data);
    d_->setSize(yuv_size);
    update();
}