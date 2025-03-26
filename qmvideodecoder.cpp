#include "qmvideodecoder.h"
#include <QDebug>
#include <QFile>
#include <QSize>
#include <QThread>
#include <QTimer>
#include <chrono>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

QmVideoDecoder::QmVideoDecoder()
    : thread_(new QThread)
{
    moveToThread(thread_);
    connect(thread_, &QThread::started, this, [this] {
        run(stop_source_.get_token());
    });
    connect(thread_, &QThread::finished, this, &QmVideoDecoder::finished);
}

QmVideoDecoder::~QmVideoDecoder() noexcept
{
    stop();
    delete thread_;
}

void QmVideoDecoder::start()
{
    if (stop_source_.stop_requested()) {
        stop_source_ = std::stop_source();
    }
    if (!thread_->isRunning()) {
        thread_->start();
    }
}

void QmVideoDecoder::stop()
{
    stop_source_.request_stop();
    thread_->quit();
    thread_->wait();
}

void QmVideoDecoder::setFilePath(const QString& video_path)
{
    file_path_ = video_path;
}

void QmVideoDecoder::run(std::stop_token st)
{
    if (!QFile::exists(file_path_)) {
        qDebug() << file_path_ << "does not exists!";
        return;
    }

    AVFormatContext* format_ctx = nullptr;
    if (avformat_open_input(&format_ctx, file_path_.toStdString().c_str(), nullptr, nullptr) < 0) {
        qDebug() << "Failed to open " << file_path_;

        return;
    }

    auto format_ctx_guard = qScopeGuard([&format_ctx] {
        avformat_close_input(&format_ctx);
    });

    if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
        return;
    }

    unsigned int video_index = -1;
    AVCodecParameters* video_codecpar = nullptr;
    for (unsigned int i = 0; i < format_ctx->nb_streams; ++i) {
        if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_index = i;
            video_codecpar = format_ctx->streams[i]->codecpar;
            break;
        }
    }

    if (video_index < 0) {
        qDebug() << "Faield to find video stream!";
        return;
    }

    emit loadFinished({ video_codecpar->width, video_codecpar->height });

    // 获取帧率
    AVStream* video_stream = format_ctx->streams[video_index];
    AVRational frame_rate = video_stream->avg_frame_rate;
    if (frame_rate.num == 0 || frame_rate.den == 0) {
        frame_rate = video_stream->r_frame_rate;
        if (frame_rate.num == 0 || frame_rate.den == 0) {
            frame_rate = av_inv_q(video_stream->time_base);
        }
    }
    qDebug() << "FPS: " << (frame_rate.num / frame_rate.den);

    // 初始化解码器
    const AVCodec* video_codec = avcodec_find_decoder(video_codecpar->codec_id);
    if (!video_codec) {
        qDebug() << "Failed to find video codec!";
        return;
    }

    AVCodecContext* video_codec_ctx = avcodec_alloc_context3(video_codec);
    avcodec_parameters_to_context(video_codec_ctx, video_codecpar);
    if (avcodec_open2(video_codec_ctx, video_codec, nullptr) < 0) {
        qDebug() << "Failed to open codec!";
        return;
    }

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    int width = video_codecpar->width;
    int height = video_codecpar->height;

    std::mutex mutex;
    do {
        while (!st.stop_requested() && av_read_frame(format_ctx, packet) >= 0) {
            if (packet->stream_index == video_index) {
                if (avcodec_send_packet(video_codec_ctx, packet) == 0) {
                    while (!st.stop_requested() && avcodec_receive_frame(video_codec_ctx, frame) == 0) {
                        int y_size = width * height;
                        int uv_size = (width / 2) * (height / 2);
                        QByteArray yuv_data;
                        yuv_data.resize(width * height * 3 / 2, 0);
                        std::copy_n(frame->data[0], y_size, yuv_data.data());
                        std::copy_n(frame->data[1], uv_size, yuv_data.data() + y_size);
                        std::copy_n(frame->data[2], uv_size, yuv_data.data() + y_size + uv_size);
                        emit frameReady(yuv_data);

                        // 条件等待
                        std::unique_lock<std::mutex> lock(mutex);
                        std::condition_variable_any().wait_for(lock, st, std::chrono::milliseconds(1000 / (frame_rate.num / frame_rate.den)), [] { return false; });
                    }
                }
            }
            av_packet_unref(packet);
        }
        av_seek_frame(format_ctx, -1, 0, AVSEEK_FLAG_BACKWARD);
    } while (!st.stop_requested() && loop_);

    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&video_codec_ctx);
}
