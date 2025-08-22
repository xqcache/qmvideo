#pragma once

#include <QOpenGLWidget>

class QmYuvViewPrivate;

class QmYuvView : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit QmYuvView(QWidget* parent = nullptr);
    ~QmYuvView() noexcept override;

    void setData(const QByteArray& yuv_data, const QSize& yuv_size);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    QmYuvViewPrivate* d_ { nullptr };
};