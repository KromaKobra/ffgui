#include "ChannelLayouts.h"

#include <QHash>

namespace ffgui::layouts {

namespace {

// `ffmpeg -layouts`, standard layouts.
const QHash<QString, QString> &table()
{
    static const QHash<QString, QString> t {
        { "mono", "FC" }, { "stereo", "FL+FR" }, { "2.1", "FL+FR+LFE" }, { "3.0", "FL+FR+FC" },
        { "3.0(back)", "FL+FR+BC" }, { "4.0", "FL+FR+FC+BC" }, { "quad", "FL+FR+BL+BR" },
        { "quad(side)", "FL+FR+SL+SR" }, { "3.1", "FL+FR+FC+LFE" }, { "5.0", "FL+FR+FC+BL+BR" },
        { "5.0(side)", "FL+FR+FC+SL+SR" }, { "4.1", "FL+FR+FC+LFE+BC" }, { "5.1", "FL+FR+FC+LFE+BL+BR" },
        { "5.1(side)", "FL+FR+FC+LFE+SL+SR" }, { "6.0", "FL+FR+FC+BC+SL+SR" },
        { "6.0(front)", "FL+FR+FLC+FRC+SL+SR" }, { "3.1.2", "FL+FR+FC+LFE+TFL+TFR" },
        { "hexagonal", "FL+FR+FC+BL+BR+BC" }, { "6.1", "FL+FR+FC+LFE+BC+SL+SR" },
        { "6.1(back)", "FL+FR+FC+LFE+BL+BR+BC" }, { "6.1(front)", "FL+FR+LFE+FLC+FRC+SL+SR" },
        { "7.0", "FL+FR+FC+BL+BR+SL+SR" }, { "7.0(front)", "FL+FR+FC+FLC+FRC+SL+SR" },
        { "7.1", "FL+FR+FC+LFE+BL+BR+SL+SR" }, { "7.1(wide)", "FL+FR+FC+LFE+BL+BR+FLC+FRC" },
        { "7.1(wide-side)", "FL+FR+FC+LFE+FLC+FRC+SL+SR" }, { "5.1.2", "FL+FR+FC+LFE+SL+SR+TFL+TFR" },
        { "5.1.2(back)", "FL+FR+FC+LFE+BL+BR+TFL+TFR" }, { "octagonal", "FL+FR+FC+BL+BR+BC+SL+SR" },
        { "cube", "FL+FR+BL+BR+TFL+TFR+TBL+TBR" }, { "5.1.4", "FL+FR+FC+LFE+SL+SR+TFL+TFR+TBL+TBR" },
        { "7.1.2", "FL+FR+FC+LFE+BL+BR+SL+SR+TFL+TFR" }, { "7.1.4", "FL+FR+FC+LFE+BL+BR+SL+SR+TFL+TFR+TBL+TBR" },
        { "binaural", "BIL+BIR" },
    };
    return t;
}

} // namespace

QString defaultLayout(int channels)
{
    switch (channels) {
    case 1: return QStringLiteral("mono");
    case 2: return QStringLiteral("stereo");
    case 3: return QStringLiteral("2.1");
    case 4: return QStringLiteral("4.0");
    case 5: return QStringLiteral("5.0");
    case 6: return QStringLiteral("5.1");
    case 7: return QStringLiteral("6.1");
    case 8: return QStringLiteral("7.1");
    default: return {};
    }
}

QStringList channelNames(const QString &layout, int channels)
{
    // "5.1(side)" and also explicit "FL+FR+LFE" forms.
    QString decomposition = table().value(layout);
    if (decomposition.isEmpty() && layout.contains(u'+'))
        decomposition = layout;
    QStringList names = decomposition.isEmpty() ? QStringList() : decomposition.split(u'+');
    if (names.size() != channels || channels <= 0) {
        names = table().value(defaultLayout(channels)).split(u'+', Qt::SkipEmptyParts);
        if (names.size() != channels) {
            names.clear();
            for (int i = 0; i < channels; ++i)
                names << QStringLiteral("c%1").arg(i);
        }
    }
    return names;
}

} // namespace ffgui::layouts
