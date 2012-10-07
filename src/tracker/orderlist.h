#pragma once

#include <vector>
#include "types.h"
#include "../legacy_soundlib/Snd_defs.h"
#include <algorithm>

class module_renderer;
class ModSequenceSet;

namespace modplug {
namespace tracker {

class orderlist;

void WriteModSequence(std::ostream& oStrm, const orderlist& seq);
void ReadModSequence(std::istream& iStrm, orderlist& seq, const size_t);

class orderlist {
public:
    friend class ModSequenceSet;
    typedef patternindex_t* iterator;
    typedef const patternindex_t* const_iterator;

    friend void WriteModSequence(std::ostream& oStrm, const orderlist& seq);
    friend void ReadModSequence(std::istream& iStrm, orderlist& seq, const size_t);

    virtual ~orderlist() {if (m_bDeletableArray) delete[] m_pArray;}
    orderlist(const orderlist&);
    orderlist(module_renderer& rSf, orderindex_t nSize);
    orderlist(module_renderer& rSf, patternindex_t* pArray, orderindex_t nSize, orderindex_t nCapacity, bool bDeletableArray);

    // Initialize default sized sequence.
    void Init();

    patternindex_t& operator[](const size_t i) {ASSERT(i < m_nSize); return m_pArray[i];}
    const patternindex_t& operator[](const size_t i) const {ASSERT(i < m_nSize); return m_pArray[i];}
    patternindex_t& At(const size_t i) {return (*this)[i];}
    const patternindex_t& At(const size_t i) const {return (*this)[i];}

    patternindex_t& Last() {ASSERT(m_nSize > 0); return m_pArray[m_nSize-1];}
    const patternindex_t& Last() const {ASSERT(m_nSize > 0); return m_pArray[m_nSize-1];}

    // Returns last accessible index, i.e. GetLength() - 1. Behaviour is undefined if length is zero.
    orderindex_t GetLastIndex() const {return m_nSize - 1;}

    void Append() {Append(GetInvalidPatIndex());}        // Appends InvalidPatIndex.
    void Append(patternindex_t nPat);                                        // Appends given patindex.

    // Inserts nCount orders starting from nPos using nFill as the pattern index for all inserted orders.
    // Sequence will automatically grow if needed and if it can't grow enough, some tail
    // orders will be discarded.
    // Return: Number of orders inserted.
    orderindex_t Insert(orderindex_t nPos, orderindex_t nCount) {return Insert(nPos, nCount, GetInvalidPatIndex());}
    orderindex_t Insert(orderindex_t nPos, orderindex_t nCount, patternindex_t nFill);

    // Removes orders from range [nPosBegin, nPosEnd].
    void Remove(orderindex_t nPosBegin, orderindex_t nPosEnd);

    void clear();
    void resize(orderindex_t nNewSize) {resize(nNewSize, GetInvalidPatIndex());}
    void resize(orderindex_t nNewSize, patternindex_t nFill);

    // Replaces all occurences of nOld with nNew.
    void Replace(patternindex_t nOld, patternindex_t nNew) {if (nOld != nNew) std::replace(begin(), end(), nOld, nNew);}

    void AdjustToNewModType(const MODTYPE oldtype);

    orderindex_t size() const {return GetLength();}
    orderindex_t GetLength() const {return m_nSize;}

    // Returns length of sequence without counting trailing '---' items.
    orderindex_t GetLengthTailTrimmed() const;

    // Returns length of sequence stopping counting on first '---' (or at the end of sequence).
    orderindex_t GetLengthFirstEmpty() const;

    patternindex_t GetInvalidPatIndex() const {return m_nInvalidIndex;} //To correspond 0xFF
    static patternindex_t GetInvalidPatIndex(const MODTYPE type);

    patternindex_t GetIgnoreIndex() const {return m_nIgnoreIndex;} //To correspond 0xFE
    static patternindex_t GetIgnoreIndex(const MODTYPE type);

    // Returns the previous/next order ignoring skip indeces(+++).
    // If no previous/next order exists, return first/last order, and zero
    // when orderlist is empty.
    orderindex_t GetPreviousOrderIgnoringSkips(const orderindex_t start) const;
    orderindex_t GetNextOrderIgnoringSkips(const orderindex_t start) const;

    orderlist& operator=(const orderlist& seq);

    // Read/write.
    size_t WriteAsByte(FILE* f, const uint16_t count);
    size_t WriteToByteArray(uint8_t* dest, const UINT numOfBytes, const UINT destSize);
    bool ReadAsByte(const uint8_t* pFrom, const int howMany, const int memLength);

    // Deprecated function used for MPTm's created in 1.17.02.46 - 1.17.02.48.
    uint32_t Deserialize(const uint8_t* const src, const uint32_t memLength);

    //Returns true if the IT orderlist datafield is not sufficient to store orderlist information.
    bool NeedsExtraDatafield() const;

protected:
    iterator begin() {return m_pArray;}
    const_iterator begin() const {return m_pArray;}
    iterator end() {return m_pArray + m_nSize;}
    const_iterator end() const {return m_pArray + m_nSize;}

public:
    CString m_sName;                                // Sequence name.

protected:
    patternindex_t* m_pArray;                        // Pointer to sequence array.
    orderindex_t m_nSize;                                // Sequence length.
    orderindex_t m_nCapacity;                        // Capacity in m_pArray.
    patternindex_t m_nInvalidIndex;        // Invalid pat index.
    patternindex_t m_nIgnoreIndex;        // Ignore pat index.
    bool m_bDeletableArray;                        // True if m_pArray points the deletable(with delete[]) array.
    module_renderer* m_pSndFile;                        // Pointer to associated CSoundFile.

    static const bool NoArrayDelete = false;
};


inline patternindex_t orderlist::GetInvalidPatIndex(const MODTYPE type) {return type == MOD_TYPE_MPT ?  UINT16_MAX : 0xFF;}
inline patternindex_t orderlist::GetIgnoreIndex(const MODTYPE type) {return type == MOD_TYPE_MPT ? UINT16_MAX - 1 : 0xFE;}

}
}

//=======================================
class ModSequenceSet : public modplug::tracker::orderlist
//=======================================
{
    friend void WriteModSequenceOld(std::ostream& oStrm, const ModSequenceSet& seq);
    friend void ReadModSequenceOld(std::istream& iStrm, ModSequenceSet& seq, const size_t);
    friend void WriteModSequences(std::ostream& oStrm, const ModSequenceSet& seq);
    friend void ReadModSequences(std::istream& iStrm, ModSequenceSet& seq, const size_t);
    friend void ReadModSequence(std::istream& iStrm, orderlist& seq, const size_t);

public:
    ModSequenceSet(module_renderer& sndFile);

    const orderlist& GetSequence() {return GetSequence(GetCurrentSequenceIndex());}
    const orderlist& GetSequence(modplug::tracker::sequenceindex_t nSeq) const;
    orderlist& GetSequence(modplug::tracker::sequenceindex_t nSeq);
    modplug::tracker::sequenceindex_t GetNumSequences() const {return static_cast<modplug::tracker::sequenceindex_t>(m_Sequences.size());}
    void SetSequence(modplug::tracker::sequenceindex_t);                        // Sets working sequence.
    modplug::tracker::sequenceindex_t AddSequence(bool bDuplicate = true);        // Adds new sequence. If bDuplicate is true, new sequence is a duplicate of the old one. Returns the ID of the new sequence.
    void RemoveSequence() {RemoveSequence(GetCurrentSequenceIndex());}
    void RemoveSequence(modplug::tracker::sequenceindex_t);                // Removes given sequence
    modplug::tracker::sequenceindex_t GetCurrentSequenceIndex() const {return m_nCurrentSeq;}

    void OnModTypeChanged(const MODTYPE oldtype);

    ModSequenceSet& operator=(const orderlist& seq) {orderlist::operator=(seq); return *this;}

    // Merges multiple sequences into one and destroys all other sequences.
    // Returns false if there were no sequences to merge, true otherwise.
    bool MergeSequences();

    // If there are subsongs (separated by "---" or "+++" patterns) in the module,
    // asks user whether to convert these into multiple sequences (given that the
    // modformat supports multiple sequences).
    // Returns true if sequences were modified, false otherwise.
    bool ConvertSubsongsToMultipleSequences();

    static const modplug::tracker::orderindex_t s_nCacheSize = MAX_ORDERS;

private:
    void CopyCacheToStorage();
    void CopyStorageToCache();

    modplug::tracker::patternindex_t m_Cache[s_nCacheSize];                // Local cache array.
    std::vector<orderlist> m_Sequences;        // Array of sequences.
    modplug::tracker::sequenceindex_t m_nCurrentSeq;                        // Index of current sequence.
};


const char FileIdSequences[] = "mptSeqC";
const char FileIdSequence[] = "mptSeq";

void WriteModSequences(std::ostream& oStrm, const ModSequenceSet& seq);
void ReadModSequences(std::istream& iStrm, ModSequenceSet& seq, const size_t nSize = 0);

void WriteModSequenceOld(std::ostream& oStrm, const ModSequenceSet& seq);
void ReadModSequenceOld(std::istream& iStrm, ModSequenceSet& seq, const size_t);