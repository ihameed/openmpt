#ifndef PATTERNCONTAINER_H
#define PATTERNCONTAINER_H

#include "pattern.h"
#include "Snd_defs.h"

class module_renderer;
typedef CPattern MODPATTERN;

//=====================
class CPatternContainer
//=====================
{
//BEGIN: TYPEDEFS
public:
    typedef vector<MODPATTERN> PATTERNVECTOR;
//END: TYPEDEFS


//BEGIN: OPERATORS
public:
    //To mimic old pattern == modplug::tracker::modcommand_t* behavior.
    MODPATTERN& operator[](const int pat) {return m_Patterns[pat];}
    const MODPATTERN& operator[](const int pat) const {return m_Patterns[pat];}
//END: OPERATORS

//BEGIN: INTERFACE METHODS
public:
    CPatternContainer(module_renderer& sndFile) : m_rSndFile(sndFile) {m_Patterns.assign(MAX_PATTERNS, MODPATTERN(*this));}

    // Clears existing patterns and resizes array to default size.
    void Init();

    // Empty and initialize all patterns.
    void ClearPatterns();
    // Delete all patterns.
    void DestroyPatterns();

    //Insert (default)pattern to given position. If pattern already exists at that position,
    //ignoring request. Returns true on failure, false otherwise.
    bool Insert(const modplug::tracker::patternindex_t index, const modplug::tracker::rowindex_t rows);

    //Insert pattern to position with the lowest index, and return that index, modplug::tracker::PATTERNINDEX_INVALID
    //on failure.
    modplug::tracker::patternindex_t Insert(const modplug::tracker::rowindex_t rows);

    //Remove pattern from given position. Currently it actually makes the pattern
    //'invisible' - the pattern data is cleared but the actual pattern object won't get removed.
    bool Remove(const modplug::tracker::patternindex_t index);

    // Applies function object for modcommands in patterns in given range.
    // Return: Copy of the function object.
    template <class Func>
    Func ForEachModCommand(modplug::tracker::patternindex_t nStartPat, modplug::tracker::patternindex_t nLastPat, Func func);
    template <class Func>
    Func ForEachModCommand(Func func) {return ForEachModCommand(0, Size() - 1, func);}

    modplug::tracker::patternindex_t Size() const {return static_cast<modplug::tracker::patternindex_t>(m_Patterns.size());}

    module_renderer& GetSoundFile() {return m_rSndFile;}
    const module_renderer& GetSoundFile() const {return m_rSndFile;}

    //Returns the index of given pattern, Size() if not found.
    modplug::tracker::patternindex_t GetIndex(const MODPATTERN* const pPat) const;

    // Return true if pattern can be accessed with operator[](iPat), false otherwise.
    bool IsValidIndex(const modplug::tracker::patternindex_t iPat) const {return (iPat < Size());}

    // Return true if IsValidIndex() is true and the corresponding pattern has allocated modcommand array, false otherwise.
    bool IsValidPat(const modplug::tracker::patternindex_t iPat) const {return IsValidIndex(iPat) && (*this)[iPat];}

    // Returns true if the pattern is empty, i.e. there are no notes/effects in this pattern
    bool IsPatternEmpty(const modplug::tracker::patternindex_t nPat) const;

    void ResizeArray(const modplug::tracker::patternindex_t newSize);

    void OnModTypeChanged(const MODTYPE oldtype);

    // Returns index of highest pattern with pattern named + 1.
    modplug::tracker::patternindex_t GetNumNamedPatterns() const;

//END: INTERFACE METHODS


//BEGIN: DATA MEMBERS
private:
    PATTERNVECTOR m_Patterns;
    module_renderer& m_rSndFile;
//END: DATA MEMBERS

};


template <class Func>
Func CPatternContainer::ForEachModCommand(modplug::tracker::patternindex_t nStartPat, modplug::tracker::patternindex_t nLastPat, Func func)
//-------------------------------------------------------------------------------------------------
{
    if (nStartPat > nLastPat || nLastPat >= Size())
            return func;
    for (modplug::tracker::patternindex_t nPat = nStartPat; nPat <= nLastPat; nPat++) if (m_Patterns[nPat])
            std::for_each(m_Patterns[nPat].Begin(), m_Patterns[nPat].End(), func);
    return func;
}


const char FileIdPatterns[] = "mptPc";

void ReadModPatterns(std::istream& iStrm, CPatternContainer& patc, const size_t nSize = 0);
void WriteModPatterns(std::ostream& oStrm, const CPatternContainer& patc);

#endif