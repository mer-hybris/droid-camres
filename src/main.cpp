#include <stdio.h>
#include <QtGui/QGuiApplication>
#include <QtGlobal>
#include <QScreen>

#include "camres.h"
#include "outputgen.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QString jsonFilename = QString();
    QString camhwFilename = QString();
    int genJson = 0;
    int genCamhw = 0;
    bool printUsage = true;
    bool exitError = false;

    qInfo("Camres version %s", APP_VERSION);

    if (argc == 1)
        printUsage = false;

    if (argc > 1)
    {
        int i;
        for (i=1 ; i < argc ; i++)
        {
            if (QString(argv[i]).compare("-e") == 0)
                exitError = true;
            if (QString(argv[i]).compare("-o") == 0)
                genJson = i;
            if (QString(argv[i]).compare("-w") == 0)
                genCamhw = i;
        }
    }

    if (genJson)
    {
        jsonFilename = "camera-resolutions.json";
        if (argc-1 > genJson)
        {
            if (argc-1 > genJson)
            {
                if (!QString(argv[genJson+1]).startsWith("-"))
                    jsonFilename = QString(argv[genJson+1]);
            }
        }
        printUsage = false;
    }

    if (genCamhw)
    {
        camhwFilename = "jolla-camera-hw.txt";
        if (argc-1 > genCamhw)
        {
            if (!QString(argv[genCamhw+1]).startsWith("-"))
                camhwFilename = QString(argv[genCamhw+1]);
        }
        printUsage = false;
    }

    if (printUsage)
    {
        qInfo("Usage: camres [OPTION]\n");
        qInfo("  -e                  Exit with error code if resolutions are not found");
        qInfo("  -o [filename]       Generate json for camera-settings-plugin");
        qInfo("  -w [filename]       Generate dconf for jolla-camera-hw.txt");

        return EXIT_FAILURE;
    }

    int i;
    Camres cr;

    qInfo("Searching cameras...");

    QList<QPair<QString, int> > cameras = cr.getCameras();

    if (cameras.isEmpty())
    {
        qFatal("Camres error: No cameras found.");
        return EXIT_FAILURE;
    }

    QStringList caps;
    caps << "image-capture-supported-caps";
    caps << "video-capture-supported-caps";
    caps << "viewfinder-supported-caps";

    QList<QList<QPair<QString, QStringList> > > resolutions;

    for (i=0 ; i<cameras.size() ; i++)
    {
        qInfo("Searching resolutions for %s...", qPrintable(cameras.at(i).first));
        QList<QPair<QString, QStringList> > res = cr.getResolutions(cameras.at(i).second, caps);

        if (exitError && res.isEmpty())
            return EXIT_FAILURE;

        resolutions.append(res);
    }

    OutputGen og;

    if (jsonFilename.isEmpty() && camhwFilename.isEmpty())
        og.dump(cameras, resolutions);

    if (!jsonFilename.isEmpty())
        og.makeJson(cameras, resolutions, app.primaryScreen()->availableGeometry(), jsonFilename);

    if (!camhwFilename.isEmpty())
        og.makeCamhw(cameras, resolutions, app.primaryScreen()->availableGeometry(), camhwFilename);

    return EXIT_SUCCESS;
}
