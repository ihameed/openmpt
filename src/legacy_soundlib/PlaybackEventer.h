#ifndef PLAYBACKEVENTER_H
#define PLAYBACKEVENTER_H

#include "pattern.h"

class module_renderer;

//====================
class CPlaybackEventer
//====================
{
public:
    CPlaybackEventer(module_renderer& sndf) : m_rSndFile(sndf) {}
    ~CPlaybackEventer();

    //SetPatternEvent(const PATTERNINDEX pattern, const modplug::tracker::rowindex_t row, const modplug::tracker::chnindex_t column);

    void PatternTranstionChnSolo(const modplug::tracker::chnindex_t chnIndex);
    void PatternTransitionChnUnmuteAll();

private:
    module_renderer& m_rSndFile;
};

#endif