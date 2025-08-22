#include "qmvideodecoder.h"
#include "qmyuvview.h"
#include <QApplication>
#include <QImage>
#include <QLabel>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QmVideoDecoder image_decoder;
    image_decoder.setOutputFormat(QmVideoDecoder::Image);
    image_decoder.open(R"(C:\Users\xqliang\Desktop\video1.mp4)");
    image_decoder.play();

    QLabel label;
    label.resize(image_decoder.size());
    label.show();

    QObject::connect(&image_decoder, &QmVideoDecoder::frameReady, &label, [&label](const QVariant& variant) {
        if (variant.canConvert(QMetaType::fromType<QImage>())) {
            label.setPixmap(QPixmap::fromImage(variant.value<QImage>()));
        }
    });

    QmVideoDecoder yuv_decoder;
    yuv_decoder.setOutputFormat(QmVideoDecoder::Yuv420p);
    yuv_decoder.open(R"(C:\Users\xqliang\Desktop\video1.mp4)");
    yuv_decoder.play();
    QmYuvView view;
    view.resize(640, 480);
    QObject::connect(&yuv_decoder, &QmVideoDecoder::frameReady, &view, [&view, &yuv_decoder](const QVariant& variant) {
        if (variant.canConvert(QMetaType::fromType<QByteArray>())) {
            view.setData(variant.value<QByteArray>(), yuv_decoder.size());
        }
    });
    view.show();

    return app.exec();
}