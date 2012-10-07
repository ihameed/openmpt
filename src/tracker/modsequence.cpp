#include "stdafx.h"


#include <functional>

#include "modsequence.h"
#include "modevent.h"
#include "../moddoc.h"
#include "../version.h"
#include "../serialization_utils.h"
#include "../legacy_soundlib/Sndfile.h"

#define str_SequenceTruncationNote (GetStrI18N((_TEXT("Module has sequence of length %u; it will be truncated to maximum supported length, %u."))))

#define new DEBUG_NEW

namespace modplug {
namespace tracker {

modsequence_t::modsequence_t(
    module_renderer& rSf,
    patternindex_t* pArray,
    orderindex_t nSize,
    orderindex_t nCapacity,
    const bool bDeletableArray
) : m_pSndFile(&rSf),
    m_pArray(pArray),
    m_nSize(nSize),
    m_nCapacity(nCapacity),
    m_bDeletableArray(bDeletableArray),
    m_nInvalidIndex(0xFF),
    m_nIgnoreIndex(0xFE)
{}


modsequence_t::modsequence_t(module_renderer& rSf, orderindex_t nSize) :
    m_pSndFile(&rSf),
    m_bDeletableArray(true),
    m_nInvalidIndex(invalid_index_of_modtype(MOD_TYPE_MPT)),
    m_nIgnoreIndex(ignore_index_of_modtype(MOD_TYPE_MPT))
{
    m_nSize = nSize;
    m_nCapacity = m_nSize;
    m_pArray = new patternindex_t[m_nCapacity];
    std::fill(begin(), end(), invalid_index_of_modtype(MOD_TYPE_MPT));
}


modsequence_t::modsequence_t(const modsequence_t& seq) :
    m_pSndFile(seq.m_pSndFile),
    m_bDeletableArray(false),
    m_nInvalidIndex(0xFF),
    m_nIgnoreIndex(0xFE),
    m_nSize(0),
    m_nCapacity(0),
    m_pArray(nullptr)
{
    *this = seq;
}

modsequence_t::~modsequence_t() {
    if (m_bDeletableArray) {
        delete[] m_pArray;
    }
}


bool modsequence_t::NeedsExtraDatafield() const {
    if (m_pSndFile->GetType() == MOD_TYPE_MPT && m_pSndFile->Patterns.Size() > 0xFD)
        return true;
    else
        return false;
}

orderindex_t modsequence_t::GetLastIndex() const {
    return m_nSize - 1;
}

namespace
{
    // Functor for detecting non-valid patterns from sequence.
    struct IsNotValidPat
    {
            IsNotValidPat(module_renderer& sndFile) : rSndFile(sndFile) {}
            bool operator()(patternindex_t i) {return !rSndFile.Patterns.IsValidPat(i);}
            module_renderer& rSndFile;
    };
}


void modsequence_t::AdjustToNewModType(const MODTYPE oldtype)
//---------------------------------------------------------
{
    const CModSpecifications specs = m_pSndFile->GetModSpecifications();
    const MODTYPE newtype = m_pSndFile->GetType();

    m_nInvalidIndex = invalid_index_of_modtype(newtype);
    m_nIgnoreIndex = ignore_index_of_modtype(newtype);

    // If not supported, remove "+++" separator order items.
    if (specs.hasIgnoreIndex == false)
    {
            if (oldtype != MOD_TYPE_NONE)
            {
                    iterator i = std::remove_if(
                        begin(),
                        end(),
                        std::bind2nd(
                            std::equal_to<patternindex_t>(),
                            ignore_index_of_modtype(oldtype)
                        )
                    );
                    std::fill(i, end(), GetInvalidPatIndex());
            }
    }
    else
            Replace(ignore_index_of_modtype(oldtype), GetIgnoreIndex());

    //Resize orderlist if needed.
    if (specs.ordersMax < GetLength())
    {
            // Order list too long? -> remove unnecessary order items first.
            if (oldtype != MOD_TYPE_NONE && specs.ordersMax < GetLengthTailTrimmed())
            {
                    iterator iter = std::remove_if(begin(), end(), IsNotValidPat(*m_pSndFile));
                    std::fill(iter, end(), GetInvalidPatIndex());
                    if(GetLengthTailTrimmed() > specs.ordersMax)
                    {
                            if (m_pSndFile->GetpModDoc())
                                    m_pSndFile->GetpModDoc()->AddToLog("WARNING: Order list has been trimmed!\n");
                    }
            }
            resize(specs.ordersMax);
    }

    // Replace items used to denote end of song order.
    Replace(invalid_index_of_modtype(oldtype), GetInvalidPatIndex());
}


orderindex_t modsequence_t::GetLengthTailTrimmed() const
//--------------------------------------------------
{
    orderindex_t nEnd = GetLength();
    if(nEnd == 0) return 0;
    nEnd--;
    const patternindex_t iInvalid = GetInvalidPatIndex();
    while(nEnd > 0 && (*this)[nEnd] == iInvalid)
            nEnd--;
    return ((*this)[nEnd] == iInvalid) ? 0 : nEnd+1;
}


orderindex_t modsequence_t::GetLengthFirstEmpty() const
//-------------------------------------------------
{
    return static_cast<orderindex_t>(std::find(begin(), end(), GetInvalidPatIndex()) - begin());
}


orderindex_t modsequence_t::GetNextOrderIgnoringSkips(const orderindex_t start) const
//-----------------------------------------------------------------------------
{
    const orderindex_t nLength = GetLength();
    if(nLength == 0) return 0;
    orderindex_t next = min(nLength-1, start+1);
    while(next+1 < nLength && (*this)[next] == GetIgnoreIndex()) next++;
    return next;
}


orderindex_t modsequence_t::GetPreviousOrderIgnoringSkips(const orderindex_t start) const
//---------------------------------------------------------------------------------
{
    const orderindex_t nLength = GetLength();
    if(start == 0 || nLength == 0) return 0;
    orderindex_t prev = min(start-1, nLength-1);
    while(prev > 0 && (*this)[prev] == GetIgnoreIndex()) prev--;
    return prev;
}


void modsequence_t::Init()
//----------------------
{
    resize(MAX_ORDERS);
    std::fill(begin(), end(), GetInvalidPatIndex());
}


void modsequence_t::Remove(orderindex_t nPosBegin, orderindex_t nPosEnd)
//----------------------------------------------------------------
{
    const orderindex_t nLengthTt = GetLengthTailTrimmed();
    if (nPosEnd < nPosBegin || nPosEnd >= nLengthTt)
            return;
    const orderindex_t nMoveCount = nLengthTt - (nPosEnd + 1);
    // Move orders left.
    if (nMoveCount > 0)
            memmove(m_pArray + nPosBegin, m_pArray + nPosEnd + 1, sizeof(patternindex_t) * nMoveCount);
    // Clear tail orders.
    std::fill(m_pArray + nPosBegin + nMoveCount, m_pArray + nLengthTt, GetInvalidPatIndex());
}


orderindex_t modsequence_t::Insert(orderindex_t nPos, orderindex_t nCount) {
    return Insert(nPos, nCount, GetInvalidPatIndex());
}

orderindex_t modsequence_t::Insert(orderindex_t nPos, orderindex_t nCount,
                                   patternindex_t nFill)
{
    if (nPos >= m_pSndFile->GetModSpecifications().ordersMax || nCount == 0)
            return (nCount = 0);
    const orderindex_t nLengthTt = GetLengthTailTrimmed();
    // Limit number of orders to be inserted.
    LimitMax(nCount, orderindex_t(m_pSndFile->GetModSpecifications().ordersMax - nPos));
    // Calculate new length.
    const orderindex_t nNewLength = min(nLengthTt + nCount, m_pSndFile->GetModSpecifications().ordersMax);
    // Resize if needed.
    if (nNewLength > GetLength())
            resize(nNewLength);
    // Calculate how many orders would need to be moved(nNeededSpace) and how many can actually
    // be moved(nFreeSpace).
    const orderindex_t nNeededSpace = nLengthTt - nPos;
    const orderindex_t nFreeSpace = GetLength() - (nPos + nCount);
    // Move orders nCount steps right starting from nPos.
    if (nPos < nLengthTt)
            memmove(m_pArray + nPos + nCount, m_pArray + nPos, min(nFreeSpace, nNeededSpace) * sizeof(patternindex_t));
    // Set nFill to new orders.
    std::fill(begin() + nPos, begin() + nPos + nCount, nFill);
    return nCount;
}


void modsequence_t::Append() {
    Append(GetInvalidPatIndex());
}

void modsequence_t::Append(patternindex_t nPat) {
    resize(m_nSize + 1, nPat);
}

void modsequence_t::resize(orderindex_t nNewSize) {
    resize(nNewSize, GetInvalidPatIndex());
}

void modsequence_t::resize(orderindex_t nNewSize, patternindex_t nFill) {
    if (nNewSize == m_nSize) return;
    if (nNewSize <= m_nCapacity)
    {
            if (nNewSize > m_nSize)
                    std::fill(begin() + m_nSize, begin() + nNewSize, nFill);
            m_nSize = nNewSize;
    }
    else
    {
            const patternindex_t* const pOld = m_pArray;
            m_nCapacity = nNewSize + 100;
            m_pArray = new patternindex_t[m_nCapacity];
        if (m_nSize) {
            std::copy_n(pOld, m_nSize, m_pArray);
        }
            std::fill(m_pArray + m_nSize, m_pArray + nNewSize, nFill);
            m_nSize = nNewSize;
            if (m_bDeletableArray)
                    delete[] pOld;
            m_bDeletableArray = true;
    }
}


void modsequence_t::clear() {
    m_nSize = 0;
}


modsequence_t& modsequence_t::operator = (const modsequence_t& seq) {
    if (&seq == this)
            return *this;
    m_nIgnoreIndex = seq.m_nIgnoreIndex;
    m_nInvalidIndex = seq.m_nInvalidIndex;
    resize(seq.GetLength());
    std::copy_n(seq.begin(), m_nSize, begin());
    m_sName = seq.m_sName;
    return *this;
}

/////////////////////////////////////
// Read/Write
/////////////////////////////////////


uint32_t modsequence_t::Deserialize(const uint8_t* const src, const uint32_t memLength)
//--------------------------------------------------------------------------
{
    if(memLength < 2 + 4) return 0;
    uint16_t version = 0;
    uint16_t s = 0;
    uint32_t memPos = 0;
    memcpy(&version, src, sizeof(version));
    memPos += sizeof(version);
    if(version != 0) return memPos;
    memcpy(&s, src+memPos, sizeof(s));
    memPos += 4;
    if(s > 65000) return true;
    if(memLength < memPos+s*4) return memPos;

    const uint16_t nOriginalSize = s;
    LimitMax(s, ModSpecs::mptm.ordersMax);

    resize(max(s, MAX_ORDERS));
    for(size_t i = 0; i<s; i++, memPos +=4 )
    {
            uint32_t temp;
            memcpy(&temp, src+memPos, 4);
            (*this)[i] = static_cast<patternindex_t>(temp);
    }
    memPos += 4*(nOriginalSize - s);
    return memPos;
}


size_t modsequence_t::WriteToByteArray(uint8_t* dest, const UINT numOfBytes, const UINT destSize)
//-----------------------------------------------------------------------------
{
    if(numOfBytes > destSize || numOfBytes > MAX_ORDERS) return true;
    if(GetLength() < numOfBytes) resize(orderindex_t(numOfBytes), 0xFF);
    UINT i = 0;
    for(i = 0; i<numOfBytes; i++)
    {
            dest[i] = static_cast<uint8_t>((*this)[i]);
    }
    return i; //Returns the number of bytes written.
}


size_t modsequence_t::WriteAsByte(FILE* f, const uint16_t count)
//----------------------------------------------------------
{
    if(GetLength() < count) resize(count);

    size_t i = 0;

    for(i = 0; i<count; i++)
    {
            const patternindex_t pat = (*this)[i];
            uint8_t temp = static_cast<uint8_t>((*this)[i]);

            if(pat > 0xFD)
            {
                    if(pat == GetInvalidPatIndex()) temp = 0xFF;
                    else temp = 0xFE;
            }
            fwrite(&temp, 1, 1, f);
    }
    return i; //Returns the number of bytes written.
}


bool modsequence_t::ReadAsByte(const uint8_t* pFrom, const int howMany, const int memLength)
//-------------------------------------------------------------------------------------
{
    if(howMany < 0 || howMany > memLength) return true;
    if(m_pSndFile->GetType() != MOD_TYPE_MPT && howMany > MAX_ORDERS) return true;

    if(GetLength() < static_cast<size_t>(howMany))
            resize(orderindex_t(howMany));

    for(int i = 0; i<howMany; i++, pFrom++)
            (*this)[i] = *pFrom;
    return false;
}


void ReadModSequence(std::istream& iStrm, modsequence_t& seq, const size_t)
//-----------------------------------------------------------------------
{
    srlztn::Ssb ssb(iStrm);
    ssb.BeginRead(FileIdSequence, MptVersion::num);
    if ((ssb.m_Status & srlztn::SNT_FAILURE) != 0)
            return;
    std::string str;
    ssb.ReadItem(str, "n");
    seq.m_sName = str.c_str();
    uint16_t nSize = MAX_ORDERS;
    ssb.ReadItem<uint16_t>(nSize, "l");
    LimitMax(nSize, ModSpecs::mptm.ordersMax);
    seq.resize(max(nSize, deprecated_modsequence_list_t::s_nCacheSize));
    ssb.ReadItem(seq.m_pArray, "a", 1, srlztn::ArrayReader<uint16_t>(nSize));
}

void WriteModSequence(std::ostream& oStrm, const modsequence_t& seq)
//----------------------------------------------------------------
{
    srlztn::Ssb ssb(oStrm);
    ssb.BeginWrite(FileIdSequence, MptVersion::num);
    ssb.WriteItem((LPCSTR)seq.m_sName.c_str(), "n");
    const uint16_t nLength = seq.GetLengthTailTrimmed();
    ssb.WriteItem<uint16_t>(nLength, "l");
    ssb.WriteItem(seq.m_pArray, "a", 1, srlztn::ArrayWriter<uint16_t>(nLength));
    ssb.FinishWrite();
}

deprecated_modsequence_list_t::deprecated_modsequence_list_t(module_renderer& sndFile)
    : modsequence_t(sndFile, m_Cache, s_nCacheSize, s_nCacheSize, deprecated_NoArrayDelete),
      m_nCurrentSeq(0)
//-------------------------------------------------------------------
{
    m_Sequences.push_back(modsequence_t(sndFile, s_nCacheSize));
}


const modsequence_t& deprecated_modsequence_list_t::GetSequence(sequenceindex_t nSeq) const
//----------------------------------------------------------------------
{
    if (nSeq == GetCurrentSequenceIndex())
            return *this;
    else
            return m_Sequences[nSeq];
}


modsequence_t& deprecated_modsequence_list_t::GetSequence(sequenceindex_t nSeq)
//----------------------------------------------------------
{
    if (nSeq == GetCurrentSequenceIndex())
            return *this;
    else
            return m_Sequences[nSeq];
}


void deprecated_modsequence_list_t::CopyCacheToStorage()
//---------------------------------------
{
    m_Sequences[m_nCurrentSeq] = *this;
}


void deprecated_modsequence_list_t::CopyStorageToCache()
//---------------------------------------
{
    const modsequence_t& rSeq = m_Sequences[m_nCurrentSeq];
    if (rSeq.GetLength() <= s_nCacheSize)
    {
            patternindex_t* pOld = m_pArray;
            m_pArray = m_Cache;
            m_nSize = rSeq.GetLength();
            m_nCapacity = s_nCacheSize;
            m_sName = rSeq.m_sName;
        std::copy_n(rSeq.m_pArray, m_nSize, m_pArray);
            if (m_bDeletableArray)
                    delete[] pOld;
            m_bDeletableArray = false;
    }
    else
            modsequence_t::operator=(rSeq);
}


void deprecated_modsequence_list_t::SetSequence(sequenceindex_t n)
//-----------------------------------------------
{
    CopyCacheToStorage();
    m_nCurrentSeq = n;
    CopyStorageToCache();
}


sequenceindex_t deprecated_modsequence_list_t::AddSequence(bool bDuplicate)
//--------------------------------------------------------
{
    if(GetNumSequences() == MAX_SEQUENCES)
            return SequenceIndexInvalid;
    m_Sequences.push_back(modsequence_t(*m_pSndFile, s_nCacheSize));
    if (bDuplicate)
    {
            m_Sequences.back() = *this;
            m_Sequences.back().m_sName = ""; // Don't copy sequence name.
    }
    SetSequence(GetNumSequences() - 1);
    return GetNumSequences() - 1;
}


void deprecated_modsequence_list_t::RemoveSequence(sequenceindex_t i)
//--------------------------------------------------
{
    // Do nothing if index is invalid or if there's only one sequence left.
    if (i >= m_Sequences.size() || m_Sequences.size() <= 1)
            return;
    const bool bSequenceChanges = (i == m_nCurrentSeq);
    m_Sequences.erase(m_Sequences.begin() + i);
    if (i < m_nCurrentSeq || m_nCurrentSeq >= GetNumSequences())
            m_nCurrentSeq--;
    if (bSequenceChanges)
            CopyStorageToCache();
}


void deprecated_modsequence_list_t::OnModTypeChanged(const MODTYPE oldtype)
//----------------------------------------------------------
{
    const MODTYPE newtype = m_pSndFile->GetType();
    const sequenceindex_t nSeqs = GetNumSequences();
    for(sequenceindex_t n = 0; n < nSeqs; n++)
    {
            GetSequence(n).AdjustToNewModType(oldtype);
    }
    // Multisequences not suppported by other formats
    if (oldtype != MOD_TYPE_NONE && newtype != MOD_TYPE_MPT)
            MergeSequences();

    // Convert sequence with separator patterns into multiple sequences?
    if (oldtype != MOD_TYPE_NONE && newtype == MOD_TYPE_MPT && GetNumSequences() == 1)
            ConvertSubsongsToMultipleSequences();
}


bool deprecated_modsequence_list_t::ConvertSubsongsToMultipleSequences()
//-------------------------------------------------------
{
    // Allow conversion only if there's only one sequence.
    if (GetNumSequences() != 1 || m_pSndFile->GetType() != MOD_TYPE_MPT)
            return false;

    bool hasSepPatterns = false;
    const orderindex_t nLengthTt = GetLengthTailTrimmed();
    for(orderindex_t nOrd = 0; nOrd < nLengthTt; nOrd++)
    {
            if(!m_pSndFile->Patterns.IsValidPat(At(nOrd)) && At(nOrd) != GetIgnoreIndex())
            {
                    hasSepPatterns = true;
                    break;
            }
    }
    bool modified = false;

    if(hasSepPatterns &&
            ::MessageBox(NULL,
            "The order list contains separator items.\nThe new format supports multiple sequences, do you want to convert those separate tracks into multiple song sequences?",
            "Order list conversion", MB_YESNO | MB_ICONQUESTION) == IDYES)
    {

            SetSequence(0);
            for(orderindex_t nOrd = 0; nOrd < GetLengthTailTrimmed(); nOrd++)
            {
                    // end of subsong?
                    if(!m_pSndFile->Patterns.IsValidPat(At(nOrd)) && At(nOrd) != GetIgnoreIndex())
                    {
                            orderindex_t oldLength = GetLengthTailTrimmed();
                            // remove all separator patterns between current and next subsong first
                            while(nOrd < oldLength && (!m_pSndFile->Patterns.IsValidIndex(At(nOrd))))
                            {
                                    At(nOrd) = GetInvalidPatIndex();
                                    nOrd++;
                                    modified = true;
                            }
                            if(nOrd >= oldLength) break;
                            orderindex_t startOrd = nOrd;
                            modified = true;

                            sequenceindex_t newSeq = AddSequence(false);
                            SetSequence(newSeq);

                            // resize new seqeuence if necessary
                            if(GetLength() < oldLength - startOrd)
                            {
                                    resize(oldLength - startOrd);
                            }

                            // now, move all following orders to the new sequence
                            while(nOrd < oldLength)
                            {
                                    patternindex_t copyPat = GetSequence(newSeq - 1)[nOrd];
                                    At(nOrd - startOrd) = copyPat;
                                    nOrd++;

                                    // is this a valid pattern? adjust pattern jump commands, if necessary.
                                    if(m_pSndFile->Patterns.IsValidPat(copyPat))
                                    {
                                            modevent_t *m = m_pSndFile->Patterns[copyPat];
                                            for (UINT len = m_pSndFile->Patterns[copyPat].GetNumRows() * m_pSndFile->m_nChannels; len; m++, len--)
                                            {
                                                    if(m->command == CmdPositionJump && m->param >= startOrd)
                                                    {
                                                            m->param = static_cast<uint8_t>(m->param - startOrd);
                                                    }
                                            }
                                    }
                            }
                            SetSequence(newSeq - 1);
                            Remove(startOrd, oldLength - 1);
                            SetSequence(newSeq);
                            // start from beginning...
                            nOrd = 0;
                            if(GetLengthTailTrimmed() == 0 || !m_pSndFile->Patterns.IsValidIndex(At(nOrd))) break;
                    }
            }
            SetSequence(0);
    }
    return modified;
}

bool deprecated_modsequence_list_t::MergeSequences()
//-----------------------------------
{
    if(GetNumSequences() <= 1)
            return false;

    CHAR s[256];
    SetSequence(0);
    resize(GetLengthTailTrimmed());
    sequenceindex_t removedSequences = 0; // sequence count
    vector <sequenceindex_t> patternsFixed; // pattern fixed by other sequence already?
    patternsFixed.resize(m_pSndFile->Patterns.Size(), SequenceIndexInvalid);
    // Set up vector
    for(orderindex_t nOrd = 0; nOrd < GetLengthTailTrimmed(); nOrd++)
    {
            patternindex_t nPat = At(nOrd);
            if(!m_pSndFile->Patterns.IsValidPat(nPat)) continue;
            patternsFixed[nPat] = 0;
    }

    while(GetNumSequences() > 1)
    {
            removedSequences++;
            const orderindex_t nFirstOrder = GetLengthTailTrimmed() + 1; // +1 for separator item
            if(nFirstOrder + GetSequence(1).GetLengthTailTrimmed() > m_pSndFile->GetModSpecifications().ordersMax)
            {
                    wsprintf(s, "WARNING: Cannot merge Sequence %d (too long!)\n", removedSequences);
                    if (m_pSndFile->GetpModDoc())
                            m_pSndFile->GetpModDoc()->AddToLog(s);
                    RemoveSequence(1);
                    continue;
            }
            Append(GetInvalidPatIndex()); // Separator item
            for(orderindex_t nOrd = 0; nOrd < GetSequence(1).GetLengthTailTrimmed(); nOrd++)
            {
                    patternindex_t nPat = GetSequence(1)[nOrd];
                    Append(nPat);

                    // Try to fix patterns (Bxx commands)
                    if(!m_pSndFile->Patterns.IsValidPat(nPat)) continue;

                    modevent_t *m = m_pSndFile->Patterns[nPat];
                    for (UINT len = 0; len < m_pSndFile->Patterns[nPat].GetNumRows() * m_pSndFile->m_nChannels; m++, len++)
                    {
                            if(m->command == CmdPositionJump)
                            {
                                    if(patternsFixed[nPat] != SequenceIndexInvalid && patternsFixed[nPat] != removedSequences)
                                    {
                                            // Oops, some other sequence uses this pattern already.
                                            const patternindex_t nNewPat = m_pSndFile->Patterns.Insert(m_pSndFile->Patterns[nPat].GetNumRows());
                                            if(nNewPat != SequenceIndexInvalid)
                                            {
                                                    // could create new pattern - copy data over and continue from here.
                                                    At(nFirstOrder + nOrd) = nNewPat;
                                                    modevent_t *pSrc = m_pSndFile->Patterns[nPat];
                                                    modevent_t *pDest = m_pSndFile->Patterns[nNewPat];
                                                    memcpy(pDest, pSrc, m_pSndFile->Patterns[nPat].GetNumRows() * m_pSndFile->m_nChannels * sizeof(modevent_t));
                                                    m = pDest + len;
                                                    patternsFixed.resize(max(nNewPat + 1, (patternindex_t)patternsFixed.size()), SequenceIndexInvalid);
                                                    nPat = nNewPat;
                                            } else
                                            {
                                                    // cannot create new pattern: notify the user
                                                    wsprintf(s, "CONFLICT: Pattern break commands in Pattern %d might be broken since it has been used in several sequences!", nPat);
                                                    if (m_pSndFile->GetpModDoc())
                                                            m_pSndFile->GetpModDoc()->AddToLog(s);
                                            }
                                    }
                                    m->param  = static_cast<uint8_t>(m->param + nFirstOrder);
                                    patternsFixed[nPat] = removedSequences;
                            }
                    }

            }
            RemoveSequence(1);
    }
    // Remove order name + fill up with empty patterns.
    m_sName = "";
    const orderindex_t nMinLength = (std::min)(deprecated_modsequence_list_t::s_nCacheSize, m_pSndFile->GetModSpecifications().ordersMax);
    if (GetLength() < nMinLength)
            resize(nMinLength);
    return true;
}

void ReadModSequenceOld(std::istream& iStrm, deprecated_modsequence_list_t& seq, const size_t)
//-----------------------------------------------------------------------------
{
    uint16_t size;
    srlztn::Binaryread<uint16_t>(iStrm, size);
    if(size > ModSpecs::mptm.ordersMax)
    {
            // Hack: Show message here if trying to load longer sequence than what is supported.
            CString str; str.Format(str_SequenceTruncationNote, size, ModSpecs::mptm.ordersMax);
            AfxMessageBox(str, MB_ICONWARNING);
            size = ModSpecs::mptm.ordersMax;
    }
    seq.resize(max(size, MAX_ORDERS));
    if(size == 0)
            { seq.Init(); return; }

    for(size_t i = 0; i < size; i++)
    {
            uint16_t temp;
            srlztn::Binaryread<uint16_t>(iStrm, temp);
            seq[i] = temp;
    }
}


void WriteModSequenceOld(std::ostream& oStrm, const deprecated_modsequence_list_t& seq)
//-------------------------------------------------------------------------
{
    const uint16_t size = seq.GetLength();
    srlztn::Binarywrite<uint16_t>(oStrm, size);
    const deprecated_modsequence_list_t::const_iterator endIter = seq.end();
    for(deprecated_modsequence_list_t::const_iterator citer = seq.begin(); citer != endIter; citer++)
    {
            const uint16_t temp = static_cast<uint16_t>(*citer);
            srlztn::Binarywrite<uint16_t>(oStrm, temp);
    }
}

void ReadModSequences(std::istream& iStrm, deprecated_modsequence_list_t& seq, const size_t)
//---------------------------------------------------------------------------
{
    srlztn::Ssb ssb(iStrm);
    ssb.BeginRead(FileIdSequences, MptVersion::num);
    if ((ssb.m_Status & srlztn::SNT_FAILURE) != 0)
            return;
    uint8_t nSeqs;
    uint8_t nCurrent;
    ssb.ReadItem(nSeqs, "n");
    if (nSeqs == 0)
            return;
    LimitMax(nSeqs, MAX_SEQUENCES);
    ssb.ReadItem(nCurrent, "c");
    if (seq.GetNumSequences() < nSeqs)
            seq.m_Sequences.resize(nSeqs, modsequence_t(*seq.m_pSndFile, seq.s_nCacheSize));

    for(uint8_t i = 0; i < nSeqs; i++)
            ssb.ReadItem(seq.m_Sequences[i], &i, sizeof(i), &ReadModSequence);
    seq.m_nCurrentSeq = (nCurrent < seq.GetNumSequences()) ? nCurrent : 0;
    seq.CopyStorageToCache();
}

void WriteModSequences(std::ostream& oStrm, const deprecated_modsequence_list_t& seq)
//--------------------------------------------------------------------
{
    srlztn::Ssb ssb(oStrm);
    ssb.BeginWrite(FileIdSequences, MptVersion::num);
    const uint8_t nSeqs = seq.GetNumSequences();
    const uint8_t nCurrent = seq.GetCurrentSequenceIndex();
    ssb.WriteItem(nSeqs, "n");
    ssb.WriteItem(nCurrent, "c");
    for(uint8_t i = 0; i < nSeqs; i++)
    {
            if (i == seq.GetCurrentSequenceIndex())
                    ssb.WriteItem(seq, &i, sizeof(i), &WriteModSequence);
            else
                    ssb.WriteItem(seq.m_Sequences[i], &i, sizeof(i), &WriteModSequence);
    }
    ssb.FinishWrite();
}


}
}