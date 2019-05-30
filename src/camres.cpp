#include "camres.h"
#include <unistd.h>
#include <math.h>
#include <QDir>
#include <QDebug>
#include <QRect>

#include <gst/pbutils/encoding-profile.h>
#include <gst/pbutils/encoding-target.h>

Camres::Camres(QObject *parent) :
    QObject(parent)
{
    gst_init(0, 0);
}

Camres::~Camres()
{
}

QList<QPair<QString, int> > Camres::getCameras()
{
    QList<QPair<QString, int> > res;

    GstElement *elem = gst_element_factory_make("droidcamsrc", NULL);
    if (!elem)
    {
        qCritical("Camres error: Failed to create an instance of droidcamsrc.");
        return res;
    }

    GParamSpec *spec = g_object_class_find_property(G_OBJECT_GET_CLASS(elem), "camera-device");
    if (!spec)
    {
        qCritical("Camres error: Failed to get property camera-device");
        gst_object_unref(elem);
        return res;
    }

    if (!G_IS_PARAM_SPEC_ENUM(spec))
    {
        qCritical("Camres error: Property camera-device is not an enum.");
        gst_object_unref(elem);
        return res;
    }

    GParamSpecEnum *e = G_PARAM_SPEC_ENUM(spec);

    res << qMakePair<QString, int>(e->enum_class->values[e->default_value].value_name, (int)e->default_value);

    for (int x = e->enum_class->minimum; x <= e->enum_class->maximum; x++)
    {
        if (x != e->default_value)
        {
            res << qMakePair<QString, int>(e->enum_class->values[x].value_name, x);
        }
    }

    gst_object_unref(elem);

    return res;
}


QList<QPair<QString, QStringList> > Camres::getResolutions(int cam, QStringList whichCaps)
{
    QList<QPair<QString, QStringList> > res;

    GstElement *cameraBin = gst_element_factory_make("camerabin", NULL);

    if (!cameraBin)
    {
        qCritical("Camres error: Failed to create camerabin.");
        return res;
    }

    GstElement *videoSource = gst_element_factory_make("droidcamsrc", NULL);
    if (!videoSource)
    {
        qCritical("Camres error: Failed to create videoSource.");
        gst_object_unref(cameraBin);
        return res;
    }

    g_object_set(cameraBin, "camera-source", videoSource, NULL);
    g_object_set(videoSource, "camera-device", cam, NULL);

    GstElement *fakeviewfinder = gst_element_factory_make("fakesink", NULL);
    if (!fakeviewfinder)
    {
        {
            qCritical("Camres error: Failed to create fake viewfinder.");
            gst_object_unref(videoSource);
            gst_object_unref(cameraBin);
            return res;
        }
    }

    g_object_set(cameraBin, "viewfinder-sink", fakeviewfinder, NULL);

    GError *error = NULL;
    GstEncodingTarget *target = gst_encoding_target_load_from_file("/usr/share/droid-camres/video.gep", &error);

    if (!target)
    {
        qCritical("Camres error: Failed to load encoding target: %s", qPrintable(error->message));
        g_error_free(error);
        gst_object_unref(fakeviewfinder);
        gst_object_unref(videoSource);
        gst_object_unref(cameraBin);
        return res;
    }

    GstEncodingProfile *profile = gst_encoding_target_get_profile(target, "video-profile");
    if (!profile)
    {
        qCritical("Camres error: Failed to load encoding profile.");
        gst_object_unref(fakeviewfinder);
        gst_object_unref(videoSource);
        gst_object_unref(cameraBin);
        gst_encoding_target_unref(target);
        return res;
    }

    gst_encoding_target_unref(target);

    g_object_set(cameraBin, "video-profile", profile, NULL);

    if (gst_element_set_state (GST_ELEMENT (cameraBin), GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
    {
        qCritical("Camres error: Failed to start playback.");
        gst_object_unref(fakeviewfinder);
        gst_object_unref(videoSource);
        gst_object_unref(cameraBin);
        return res;
    }

    GstCaps *caps = NULL;

    int i;

    for (i=0 ; i<whichCaps.size() ; i++)
    {
        g_object_get(cameraBin, whichCaps.at(i).toLatin1().constData(), &caps, NULL);
        res.append(qMakePair<QString, QStringList>(whichCaps.at(i), parse(caps)));
    }

    gst_caps_unref(caps);
    gst_element_set_state (GST_ELEMENT (cameraBin), GST_STATE_NULL);
    gst_object_unref(fakeviewfinder);
    gst_object_unref(videoSource);
    gst_object_unref(cameraBin);

    return res;
}

QStringList Camres::parse(GstCaps *caps)
{
    QStringList res;
    QString tmp;

    if (!caps)
    {
        return res;
    }

    for (guint x = 0; x < gst_caps_get_size(caps); x++)
    {
        const GstStructure *s = gst_caps_get_structure(caps, x);
        const GValue *width = gst_structure_get_value(s, "width");
        const GValue *height = gst_structure_get_value(s, "height");
        const GValue *fps = gst_structure_get_value(s, "framerate");

        if (!width || !height)
        {
            continue;
        }

        bool width_is_list = GST_VALUE_HOLDS_LIST(width) ? true : false;
        bool height_is_list = GST_VALUE_HOLDS_LIST(height) ? true : false;
        bool fps_is_list = GST_VALUE_HOLDS_LIST(fps) ? true : false;

        for (guint wc = 0; wc == 0 || (width_is_list && wc < gst_value_list_get_size(width)); wc++)
        {
            int w = g_value_get_int(width_is_list?gst_value_list_get_value(width, wc): width);
            for (guint hc = 0; hc == 0 || (height_is_list && hc < gst_value_list_get_size(height)); hc++)
            {
                int h = g_value_get_int(height_is_list?gst_value_list_get_value(height, hc): height);
                for (guint fc = 0; fc == 0 || (fps_is_list && fc < gst_value_list_get_size(fps)); fc++)
                {
                    const GValue *fps_val = fps_is_list?gst_value_list_get_value(fps, fc): fps;
                    if (GST_VALUE_HOLDS_FRACTION(fps_val))
                    {
                        tmp = QString("%1x%2@%3/%4").arg(w).arg(h)
                            .arg(gst_value_get_fraction_numerator(fps_val))
                            .arg(gst_value_get_fraction_denominator(fps_val));
                    }
                    else if (GST_VALUE_HOLDS_FRACTION_RANGE(fps_val))
                    {
                        const GValue *fps_min = gst_value_get_fraction_range_min(fps_val);
                        const GValue *fps_max = gst_value_get_fraction_range_max(fps_val);
                        tmp = QString("%1x%2@%3/%4-%5/%6").arg(w).arg(h)
                            .arg(gst_value_get_fraction_numerator(fps_min))
                            .arg(gst_value_get_fraction_denominator(fps_min))
                            .arg(gst_value_get_fraction_numerator(fps_max))
                            .arg(gst_value_get_fraction_denominator(fps_max));
                    }
                    else
                    {
                        qWarning("Camres error: Unknown framerate type");
                        tmp = QString("%1x%2").arg(w).arg(h);
                    }
                    if (!res.contains(tmp))
                        res.append(tmp);
                }
            }
        }
    }

    return res;
}

QString Camres::aspectRatioForResolution(const QString& size)
{
    static QMap<float, QString> ratios;
    int width, height;

    width = size.split(QRegExp("[x@]")).at(0).toInt();
    height = size.split(QRegExp("[x@]")).at(1).toInt();

    if (ratios.isEmpty())
    {
        ratios[0.7] = "3:4";
        ratios[0.8] = "4:5";
        ratios[1.0] = "1:1";
        ratios[1.2] = "5:4";
        ratios[1.3] = "4:3";
        ratios[1.5] = "3:2";
        ratios[1.6] = "16:10";
        ratios[1.7] = "16:9";
        ratios[1.8] = "9:5";
    }

    float r = (width * 1.0) / height;
    r = floor(r * 10) / 10.0;

    for (QMap<float, QString>::const_iterator iter = ratios.constBegin(); iter != ratios.constEnd(); ++iter)
    {
        if (qFuzzyCompare (r, iter.key()))
        {
            return iter.value();
        }
    }

    qWarning("Camres error: Could not find aspect ratio for %dx%d", width, height);

    return QString("?:?");
}

QString Camres::findBestViewFinderForResolution(const QString& size, const QList<QPair<QString, QStringList> > &resolutions, const QRect &screenGeometry)
{
    int width, height;

    int j, m;

    for (j=0 ; j<resolutions.size(); j++)
    {
        if (resolutions.at(j).first.startsWith("viewfinder"))
        {
            for (m=0 ; m<resolutions.at(j).second.size(); m++)
            {
                width = resolutions.at(j).second.at(m).split(QRegExp("[x@]")).at(0).toInt();
                height = resolutions.at(j).second.at(m).split(QRegExp("[x@]")).at(1).toInt();

                if (qMin(screenGeometry.height(), screenGeometry.width()) >=
                    qMin(width, height) &&
                    qMax(screenGeometry.height(), screenGeometry.width()) >=
                    qMax(width, height))
                {
                    if (Camres::aspectRatioForResolution(resolutions.at(j).second.at(m)).compare(Camres::aspectRatioForResolution(size)) == 0)
                    {
                        return resolutions.at(j).second.at(m).split('@').first();
                    }
                }
            }
        }
    }

    qCritical("Camres error: Could not find viewfinder for %s", qPrintable(size));

    return QString("?:?");
}
