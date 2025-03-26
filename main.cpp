#include "qmvideoplayer.h"
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QmVideoPlayer player;
    player.resize(1920, 1080);
    player.show();

    return app.exec();
}