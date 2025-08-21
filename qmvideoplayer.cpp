#include "qmvideoplayer.h"
#include "qmvideodecoder.h"
#include <QFile>
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

class QmVideoPlayerPrivate : private QOpenGLFunctions_3_3_Core {
public:
    QmVideoPlayerPrivate(QmVideoPlayer* q);
    ~QmVideoPlayerPrivate() noexcept;

    void init();
    void paint();

private:
    QmVideoPlayer* q_ { nullptr };
    QOpenGLShaderProgram* program_ { nullptr };
    QOpenGLVertexArrayObject* vao_ { nullptr };
    std::unique_ptr<QOpenGLBuffer> vbo_;
    std::unique_ptr<QOpenGLBuffer> ebo_;

    std::unique_ptr<QOpenGLTexture> tex_y_;
    std::unique_ptr<QOpenGLTexture> tex_u_;
    std::unique_ptr<QOpenGLTexture> tex_v_;

    std::unique_ptr<QmVideoDecoder> decoder_ { nullptr };

    int video_w_ = 1254;
    int video_h_ = 940;
    QByteArray yuv_buf_;
};

QmVideoPlayerPrivate::QmVideoPlayerPrivate(QmVideoPlayer* q)
    : q_(q)
    , program_(new QOpenGLShaderProgram(q))
    , vao_(new QOpenGLVertexArrayObject(q))
    , vbo_(new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer))
    , ebo_(new QOpenGLBuffer(QOpenGLBuffer::IndexBuffer))
    , tex_y_(new QOpenGLTexture(QOpenGLTexture::Target2D))
    , tex_u_(new QOpenGLTexture(QOpenGLTexture::Target2D))
    , tex_v_(new QOpenGLTexture(QOpenGLTexture::Target2D))
    , decoder_(new QmVideoDecoder)
{
    QObject::connect(decoder_.get(), &QmVideoDecoder::loadFinished, q, [this](const QSize& size) {
        qDebug() << "Video load finished: " << size;

        video_w_ = size.width();
        video_h_ = size.height();

        tex_y_->setSize(video_w_, video_h_);
        tex_u_->setSize(video_w_ / 2, video_h_ / 2);
        tex_v_->setSize(video_w_ / 2, video_h_ / 2);

        tex_y_->allocateStorage(QOpenGLTexture::Red, QOpenGLTexture::UInt8);
        tex_u_->allocateStorage(QOpenGLTexture::Red, QOpenGLTexture::UInt8);
        tex_v_->allocateStorage(QOpenGLTexture::Red, QOpenGLTexture::UInt8);
    });

    QObject::connect(decoder_.get(), &QmVideoDecoder::frameReady, q, [this](const QByteArray& yuv) {
        yuv_buf_ = yuv;
        q_->update();
    });

    decoder_->setFilePath(R"(C:\Users\xqliang\Desktop\a_sky_full_of_stars-480p.mp4)");
    decoder_->setLoop();
    decoder_->start();
}

QmVideoPlayerPrivate::~QmVideoPlayerPrivate() noexcept
{
}

void QmVideoPlayerPrivate::init()
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

    tex_u_->create();
    tex_u_->setFormat(QOpenGLTexture::R8_UNorm);
    tex_u_->setMinificationFilter(QOpenGLTexture::Linear);
    tex_u_->setMagnificationFilter(QOpenGLTexture::Linear);
    tex_u_->setWrapMode(QOpenGLTexture::WrapMode::ClampToEdge);

    tex_v_->create();
    tex_v_->setFormat(QOpenGLTexture::R8_UNorm);
    tex_v_->setMinificationFilter(QOpenGLTexture::Linear);
    tex_v_->setMagnificationFilter(QOpenGLTexture::Linear);
    tex_v_->setWrapMode(QOpenGLTexture::WrapMode::ClampToEdge);
}

void QmVideoPlayerPrivate::paint()
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
    tex_u_->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, yuv_buf_.constData() + video_w_ * video_h_, &options);
    tex_v_->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, yuv_buf_.constData() + video_w_ * video_h_ * 5 / 4, &options);

    tex_y_->bind(0);
    tex_u_->bind(1);
    tex_v_->bind(2);

    program_->setUniformValue("y_tex", 0);
    program_->setUniformValue("u_tex", 1);
    program_->setUniformValue("v_tex", 2);

    vao_->bind();
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
}

QmVideoPlayer::QmVideoPlayer(QWidget* parent)
    : QOpenGLWidget(parent)
    , d_(new QmVideoPlayerPrivate(this))
{
}

QmVideoPlayer::~QmVideoPlayer() noexcept
{
    delete d_;
}

void QmVideoPlayer::initializeGL()
{
    d_->init();
}

void QmVideoPlayer::resizeGL(int w, int h)
{
}

void QmVideoPlayer::paintGL()
{
    d_->paint();
}
