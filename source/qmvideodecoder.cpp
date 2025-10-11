#include "qmvideodecoder.h"
#include <QDebug>
#include <QFile>
#include <QImage>
#include <QSize>
#include <QThread>
#include <QTimer>
#include <chrono>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

namespace {
QByteArray decodeToYuv(AVFrame* frame, int width, int height)
{
    int y_size = width * height;
    int uv_size = (width / 2) * (height / 2);
    QByteArray yuv_data;
    yuv_data.resize(width * height * 3 / 2, 0);
    std::copy_n(frame->data[0], y_size, yuv_data.data());
    std::copy_n(frame->data[1], uv_size, yuv_data.data() + y_size);
    std::copy_n(frame->data[2], uv_size, yuv_data.data() + y_size + uv_size);
    return yuv_data;
}

QImage decodeToImage(SwsContext* sws_ctx, AVFrame* frame, AVFrame* rgb_frame, uint8_t* rgb_buffer, int width, int height)
{
    if (sws_scale(sws_ctx, frame->data, frame->linesize, 0, height, rgb_frame->data, rgb_frame->linesize) > 0) {

        QImage img(rgb_frame->data[0], width, height, rgb_frame->linesize[0], QImage::Format_RGB888);

        if (!img.isNull()) {
            return img.copy();
        }
    }
    return {};
}
}

struct QmVideoDecoderPrivate {
    AVFormatContext* fmt_ctx { nullptr };
    AVCodecContext* video_codec_ctx { nullptr };
    AVPacket* packet { nullptr };
    AVFrame* frame { nullptr };

    SwsContext* sws_ctx { nullptr };
    AVFrame* rgb_frame { nullptr };
    uint8_t* rgb_buffer { nullptr };

    QString video_path;
    double fps { 1 };
    // 视频时长（单位：ms）
    double duration { 0 };
    qint64 frame_count { 0 };
    int video_stream_idx = -1;
    QSize video_size { 0, 0 };
    QmVideoDecoder::Format format { QmVideoDecoder::Yuv420p };

    qint64 frame_index { 0 };
    std::atomic<qint64> frame_step { 1 };
    std::atomic_bool loop { false };

    QThread* thread { nullptr };
    std::mutex wait_mutex;
    std::stop_source stop_source;
    QmVideoDecoder::State state { QmVideoDecoder::Idle };
};

QmVideoDecoder::QmVideoDecoder()
    : d_(new QmVideoDecoderPrivate)
{
    d_->thread = new QThread();

    moveToThread(d_->thread);
    connect(d_->thread, &QThread::started, this, [this] {
        run(d_->stop_source.get_token());
    });
    connect(d_->thread, &QThread::finished, this, &QmVideoDecoder::finished);
}

QmVideoDecoder::~QmVideoDecoder() noexcept
{
    close();
    delete d_;
}

bool QmVideoDecoder::open(const QString& video_path)
{
    QElapsedTimer elapsed_timer;
    elapsed_timer.start();
    auto elapsed_guard = qScopeGuard([&elapsed_timer] {
        qDebug() << "QmVideoDecoder::open. elapsed: " << elapsed_timer.elapsed() << "ms";
    });
    close();
    if (!QFile::exists(video_path)) {
        return false;
    }
    if (avformat_open_input(&d_->fmt_ctx, video_path.toStdString().c_str(), nullptr, nullptr) < 0) {
        qDebug() << "Failed to open " << video_path;
        return false;
    }
    if (avformat_find_stream_info(d_->fmt_ctx, nullptr) < 0) {
        return false;
    }

    const AVCodec* video_codec = nullptr;
    d_->video_stream_idx = av_find_best_stream(d_->fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &video_codec, 0);
    if (d_->video_stream_idx < 0 || !video_codec) {
        qDebug() << "Faield to find video stream!";
        return false;
    }
    // 获取帧率
    AVStream* video_stream = d_->fmt_ctx->streams[d_->video_stream_idx];

    AVRational frame_rate = video_stream->avg_frame_rate;
    if (frame_rate.num == 0 || frame_rate.den == 0) {
        frame_rate = video_stream->r_frame_rate;
        if (frame_rate.num == 0 || frame_rate.den == 0) {
            frame_rate = av_inv_q(video_stream->time_base);
        }
    }
    d_->fps = av_q2d(frame_rate);
    d_->duration = (static_cast<double>(d_->fmt_ctx->duration) / AV_TIME_BASE) * 1000.0;
    d_->frame_count = std::llround(d_->duration * d_->fps / 1000.0);
    d_->video_size = { video_stream->codecpar->width, video_stream->codecpar->height };
    d_->video_path = video_path;

    d_->video_codec_ctx = avcodec_alloc_context3(video_codec);
    avcodec_parameters_to_context(d_->video_codec_ctx, video_stream->codecpar);
    if (avcodec_open2(d_->video_codec_ctx, video_codec, nullptr) < 0) {
        qDebug() << "Failed to open codec!";
        return false;
    }
    d_->packet = av_packet_alloc();
    d_->frame = av_frame_alloc();

    if (d_->format == Image && !d_->sws_ctx) {
        // 初始化转换器
        d_->sws_ctx = sws_getContext(d_->video_size.width(), d_->video_size.height(), d_->video_codec_ctx->pix_fmt, d_->video_size.width(), d_->video_size.height(), AV_PIX_FMT_RGB24,
            SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
        d_->rgb_frame = av_frame_alloc();
        int rgb_buffer_size = av_image_get_buffer_size(AV_PIX_FMT_RGB24, d_->video_size.width(), d_->video_size.height(), 1);
        d_->rgb_buffer = (uint8_t*)av_malloc(rgb_buffer_size);
        av_image_fill_arrays(d_->rgb_frame->data, d_->rgb_frame->linesize, d_->rgb_buffer, AV_PIX_FMT_RGB24, d_->video_size.width(), d_->video_size.height(), 1);
    }

    d_->state = Waiting;

    emit loadFinished(d_->video_size);

    return true;
}

void QmVideoDecoder::close()
{
    d_->stop_source.request_stop();
    if (d_->thread->isRunning()) {
        d_->thread->quit();
        d_->thread->wait();
    }
    if (d_->frame) {
        av_frame_free(&d_->frame);
    }
    if (d_->rgb_frame) {
        av_frame_free(&d_->rgb_frame);
    }
    if (d_->rgb_buffer) {
        av_free(d_->rgb_buffer);
        d_->rgb_buffer = nullptr;
    }
    if (d_->packet) {
        av_packet_free(&d_->packet);
    }
    if (d_->sws_ctx) {
        sws_freeContext(d_->sws_ctx);
        d_->sws_ctx = nullptr;
    }
    if (d_->video_codec_ctx) {
        avcodec_free_context(&d_->video_codec_ctx);
    }
    if (d_->fmt_ctx) {
        avformat_close_input(&d_->fmt_ctx);
    }
    d_->video_stream_idx = -1;
    d_->video_path = "";
    d_->frame_count = 0;
    d_->duration = 0;
    d_->fps = 1.0;
    d_->state = Idle;
}

void QmVideoDecoder::setFrameStep(qint64 frame_step)
{
    if (frame_step == 0) {
        frame_step = 1;
    }
    d_->frame_step = frame_step;
}

void QmVideoDecoder::setLoop(bool loop)
{
    d_->loop.store(loop, std::memory_order_relaxed);
}

void QmVideoDecoder::setOutputFormat(Format format)
{
    d_->format = format;
    if (d_->state == Waiting && format == Image && !d_->sws_ctx) {
        // 初始化转换器
        d_->sws_ctx = sws_getContext(d_->video_size.width(), d_->video_size.height(), d_->video_codec_ctx->pix_fmt, d_->video_size.width(), d_->video_size.height(), AV_PIX_FMT_RGB24,
            SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
        d_->rgb_frame = av_frame_alloc();
        int rgb_buffer_size = av_image_get_buffer_size(AV_PIX_FMT_RGB24, d_->video_size.width(), d_->video_size.height(), 1);
        d_->rgb_buffer = (uint8_t*)av_malloc(rgb_buffer_size);
        av_image_fill_arrays(d_->rgb_frame->data, d_->rgb_frame->linesize, d_->rgb_buffer, AV_PIX_FMT_RGB24, d_->video_size.width(), d_->video_size.height(), 1);
    }
}

void QmVideoDecoder::play()
{
    if (d_->state == Idle) {
        return;
    }
    if (d_->state == Playing) {
        stop();
    }
    d_->state = Playing;
    if (!d_->thread->isRunning()) {
        d_->stop_source = std::stop_source();
        d_->thread->start();
    }
}

void QmVideoDecoder::resume()
{
    if (d_->state != Paused) {
        return;
    }
    d_->state = Playing;
    if (!d_->thread->isRunning()) {
        d_->stop_source = std::stop_source();
        d_->thread->start();
    }
}

void QmVideoDecoder::pause()
{
    if (d_->state != Playing) {
        return;
    }
    d_->state = Paused;
}

void QmVideoDecoder::stop()
{
    if (d_->state == Idle) {
        return;
    }
    d_->stop_source.request_stop();
    if (d_->thread->isRunning()) {
        d_->thread->quit();
        d_->thread->wait();
    }
    d_->frame_index = (d_->frame_step < 0) ? d_->frame_count : 0;
    std::ignore = seekToFrameImpl(0);
    d_->state = Waiting;
}

void QmVideoDecoder::seekToFrame(qint64 frame_no)
{
    if (seekToFrameImpl(frame_no)) {
        d_->frame_index = frame_no;
    }
}

QVariant QmVideoDecoder::nextFrame()
{
    auto processFrame = [this]() -> QVariant {
        // switch (d_->frame->pict_type) {
        // case AV_PICTURE_TYPE_I:
        //     qDebug() << "=> I:" << d_->frame->pts;
        //     break;
        // case AV_PICTURE_TYPE_P:
        //     qDebug() << "   P:" << d_->frame->pts;
        //     break;
        // case AV_PICTURE_TYPE_B:
        //     qDebug() << "   B:" << d_->frame->pts;
        //     break;
        // default:
        //     break;
        // }

        if (d_->format == Yuv420p) {
            return decodeToYuv(d_->frame,
                d_->video_size.width(),
                d_->video_size.height());
        } else {
            return decodeToImage(d_->sws_ctx,
                d_->frame,
                d_->rgb_frame,
                d_->rgb_buffer,
                d_->video_size.width(),
                d_->video_size.height());
        }
    };
    int ret = 0;
    int attempt_count = 0;

    while (attempt_count < 50) {
        ret = av_read_frame(d_->fmt_ctx, d_->packet);

        // 文件末尾，执行 flush
        if (ret == AVERROR_EOF) {
            avcodec_send_packet(d_->video_codec_ctx, nullptr);
            while ((ret = avcodec_receive_frame(d_->video_codec_ctx, d_->frame)) >= 0) {
                return processFrame();
            }
            return {};
        }

        // 其他错误跳过
        if (ret < 0) {
            ++attempt_count;
            continue;
        }
        attempt_count = 0;

        // 跳过非视频流
        if (d_->packet->stream_index != d_->video_stream_idx) {
            av_packet_unref(d_->packet);
            continue;
        }

        // 发送 packet
        ret = avcodec_send_packet(d_->video_codec_ctx, d_->packet);
        av_packet_unref(d_->packet);
        if (ret < 0) {
            ++attempt_count;
            continue;
        }

        // 接收帧
        while ((ret = avcodec_receive_frame(d_->video_codec_ctx, d_->frame)) >= 0) {
            return processFrame();
        }

        // 需要更多输入
        if (ret == AVERROR(EAGAIN)) {
            continue;
        }
        // 解码结束
        else if (ret == AVERROR_EOF) {
            return {};
        } else {
            ++attempt_count;
        }
    }

    return {};
}

QVariant QmVideoDecoder::readFrame(qint64 frame_no)
{
    seekToFrame(frame_no);
    return nextFrame();
}

bool QmVideoDecoder::seekToFrameImpl(qint64 frame_no)
{
    if (d_->state == Idle) {
        return false;
    }
    int64_t timestamp = static_cast<double>(frame_no) / d_->fps * AV_TIME_BASE;
    if (av_seek_frame(d_->fmt_ctx, -1, timestamp, AVSEEK_FLAG_BACKWARD) < 0) {
        return false;
    }
    avcodec_flush_buffers(d_->video_codec_ctx);
    return true;
}

bool QmVideoDecoder::isPlaying() const
{
    return d_->state == Playing;
}

bool QmVideoDecoder::isPaused() const
{
    return d_->state == Paused || d_->state == Waiting;
}

bool QmVideoDecoder::isWaiting() const
{
    return d_->state == Waiting;
}

QSize QmVideoDecoder::size() const
{
    return d_->video_size;
}

QVariant QmVideoDecoder::decodeFrame(qint64 frame_no, int* error)
{
    std::unique_ptr<int> ffmpeg_ret_guard(new int);
    int* ret { nullptr };
    if (error) {
        ret = error;
    } else {
        ret = ffmpeg_ret_guard.get();
    }
    if (d_->state == Idle) {
        return {};
    }
    if (d_->frame_step != 1) {
        if (!seekToFrameImpl(frame_no)) {
            return {};
        }
    }

    return nextFrame();
}

void QmVideoDecoder::run(std::stop_token st)
{
    if (d_->state != Playing) {
        return;
    }
    d_->frame_index = (d_->frame_step < 0) ? d_->frame_count : 0;
    QElapsedTimer elapsed_timer;

    auto frame_duration = std::chrono::duration<double, std::milli>(1000 / d_->fps);
    std::chrono::steady_clock::time_point frame_time = std::chrono::steady_clock::now();
    auto wait = [this, &st, &frame_time, &frame_duration] {
        std::unique_lock<std::mutex> lock(d_->wait_mutex);
        std::condition_variable_any().wait_until(lock, st, frame_time + frame_duration, [] { return false; });
        frame_time = std::chrono::steady_clock::now();
    };

    while (!st.stop_requested()) {
        elapsed_timer.restart();

        if (d_->state == Paused) {
            wait();
            continue;
        } else {
            int ret = 0;
            auto frame_data = decodeFrame(d_->frame_index, &ret);
            if (frame_data.isValid()) {
                emit frameReady(frame_data);
            }
            d_->frame_index += d_->frame_step;
            if ((d_->frame_index > d_->frame_count || d_->frame_index < 0) || ret == AVERROR_EOF) {
                if (d_->loop) {
                    d_->frame_index = (d_->frame_step < 0) ? d_->frame_count : 0;
                    std::ignore = seekToFrameImpl(d_->frame_index);
                } else {
                    break;
                }
            }
            wait();
        }
        // qDebug() << "Elapsed: " << elapsed_timer.elapsed();
    }
    d_->frame_index = (d_->frame_step < 0) ? d_->frame_count : 0;
    d_->state = Waiting;
}
