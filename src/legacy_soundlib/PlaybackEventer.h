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

    //SetPatternEvent(const PATTERNINDEX pattern, const ROWINDEX row, const CHANNELINDEX column);

    void PatternTranstionChnSolo(const CHANNELINDEX chnIndex);
    void PatternTransitionChnUnmuteAll();

private:
    module_renderer& m_rSndFile;
};

#endif