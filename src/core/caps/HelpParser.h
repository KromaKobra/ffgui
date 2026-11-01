#pragma once

#include <catalog/Catalog.h>

namespace ffgui
{

// Parses the AVOptions section of filter, encoder and muxer help. Constants
// belong to the preceding option; their symbolic names are valid ffmpeg values.
class HelpParser
{
public:
    static QList<OptionSpec> parse(const QString &text);
};

} // namespace ffgui
