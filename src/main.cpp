#include <QApplication>
#include "TrayAgent.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    TrayAgent agent;

    return app.exec();
}
