#pragma

#include <QObject>
#include <QVariant>
#include <stop_token>

#include "qmvideo_global.h"

struct QmVideoDecoderPrivate;

class QMVIDEO_LIB_EXPORT QmVideoDecoder : public QObject {
    Q_OBJECT
public:
    enum State {
        Idle,
        Waiting,
        Playing,
        Paused
    };

    enum Format {
        Yuv420p,
        Image,
    };

    QmVideoDecoder();
    ~QmVideoDecoder() noexcept override;

    QSize size() const;
    QString path() const;
    State state() const;
    bool isPlaying() const;
    bool isPaused() const;
    bool isWaiting() const;
    double fps() const;
    qint64 frameCount() const;

    bool open(const QString& video_path);
    void close();
    void setLoop(bool loop = true);
    void setFrameStep(qint64 frame_step);
    void setOutputFormat(Format format);

    void play();
    void pause();
    void stop();

signals:
    void finished();
    void loadFinished(const QSize& size);
    void frameReady(const QVariant& frame_data);

private:
    void run(std::stop_token st);
    QVariant decodeFrame(qint64 frame_no, int* error = nullptr) const;

private:
    QmVideoDecoderPrivate* d_ { nullptr };
};