#pragma once

#include <QOpenGLWidget>

class QmVideoPlayerPrivate;

class QmVideoPlayer : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit QmVideoPlayer(QWidget* parent = nullptr);
    ~QmVideoPlayer() noexcept override;

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    QmVideoPlayerPrivate* d_ { nullptr };
};