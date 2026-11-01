#pragma once
#include <QVariantMap>
#include <media/Fft.h>
#include <vector>
namespace ffgui
{
class SpectrumAnalyzer
{
  public:
    static std::vector<double> bands(const float *samples, int frames, int channels, int rate,
                                     bool musical = false);
};
class Histogram
{
  public:
    static std::vector<double> bins(const float *samples, int count);
};
class BitScope
{
  public:
    static std::vector<double> activity(const float *samples, int count);
};
class Vectorscope
{
  public:
    static QVariantList points(const float *samples, int frames, int channels);
};
class ScopeAnalysis
{
  public:
    static QVariantMap analyze(const float *samples, int frames, int channels, int rate);
};
} // namespace ffgui
