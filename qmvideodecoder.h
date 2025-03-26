#pragma

#include <QObject>
#include <stop_token>

class QmVideoDecoder : public QObject {
    Q_OBJECT
public:
    QmVideoDecoder();
    ~QmVideoDecoder() noexcept override;

    void start();
    void stop();
    void setFilePath(const QString& video_path);

    void setLoop(bool loop = true);

signals:
    void finished();
    void loadFinished(const QSize& size);
    void frameReady(const QByteArray& yuv);

private:
    void run(std::stop_token st);

private:
    std::stop_source stop_source_;
    QThread* thread_ { nullptr };
    QString file_path_;
    bool loop_ { false };
};