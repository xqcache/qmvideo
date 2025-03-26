#include "qmvideoplayer.h"
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QmVideoPlayer player;
    player.resize(640, 480);
    player.show();

    return app.exec();
}