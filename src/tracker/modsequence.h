#pragma once

#include <vector>
#include "types.h"
#include "../legacy_soundlib/Snd_defs.h"
#include <algorithm>
#include <cassert>

class module_renderer;

namespace modplug {
namespace tracker {

class modsequence_t;
class deprecated_modsequence_list_t;

void WriteModSequence(std::ostream&, const modsequence_t&);
void ReadModSequence(std::istream&, modsequence_t&, const size_t);

void WriteModSequences(std::ostream&, const deprecated_modsequence_list_t&);
void ReadModSequences(std::istream&, deprecated_modsequence_list_t&, const size_t);

void WriteModSequenceOld(std::ostream&, const deprecated_modsequence_list_t&);
void ReadModSequenceOld(std::istream&, deprecated_modsequence_list_t&, const size_t);

class modsequence_t {
public:
    friend class deprecated_modsequence_list_t;
    typedef patternindex_t* iterator;
    typedef const patternindex_t* const_iterator;

    friend void WriteModSequence(std::ostream&, const modsequence_t&);
    friend void ReadModSequence(std::istream&, modsequence_t&, const size_t);

    modsequence_t(const modsequence_t&);
    modsequence_t(module_renderer&, orderindex_t);
    modsequence_t(module_renderer&, patternindex_t*, orderindex_t,
                  orderindex_t, bool);
    virtual ~modsequence_t();

    // Initialize default sized sequence.
    void Init();

    patternindex_t& operator [] (const orderindex_t i) {
        assert(i < m_nSize);
        return m_pArray[modplug::pervasives::unwrap(i)];
    }
    const patternindex_t& operator [] (const orderindex_t i) const {
        assert(i < m_nSize);
        return m_pArray[modplug::pervasives::unwrap(i)];
    }
    patternindex_t& At(const orderindex_t i) {
        return (*this)[i];
    }
    const patternindex_t& At(const orderindex_t i) const {
        return (*this)[i];
    }

    patternindex_t& Last() {
        auto size = modplug::pervasives::unwrap(m_nSize);
        assert(size > 0);
        return m_pArray[size - 1];
    }
    const patternindex_t& Last() const {
        auto size = modplug::pervasives::unwrap(m_nSize);
        assert(size > 0);
        return m_pArray[size - 1];
    }

    // Returns last accessible index, i.e. GetLength() - 1.
    // Behaviour is undefined if length is zero.
    orderindex_t GetLastIndex() const;

    // Appends InvalidPatIndex.
    void Append();

    // Appends given patindex.
    void Append(patternindex_t nPat);

    // Inserts nCount orders starting from nPos using nFill as the pattern
    // index for all inserted orders.
    // Sequence will automatically grow if needed and if it can't grow enough,
    // some tail orders will be discarded.
    // Return: Number of orders inserted.
    orderindex_t Insert(orderindex_t, orderindex_t);
    orderindex_t Insert(orderindex_t, orderindex_t, patternindex_t);

    // Removes orders from range [nPosBegin, nPosEnd].
    void Remove(orderindex_t nPosBegin, orderindex_t nPosEnd);

    void clear();
    void resize(orderindex_t nNewSize);
    void resize(orderindex_t nNewSize, patternindex_t nFill);

    // Replaces all occurences of nOld with nNew.
    void Replace(patternindex_t nOld, patternindex_t nNew) {
        if (nOld != nNew) {
            std::replace(begin(), end(), nOld, nNew);
        }
    }

    void AdjustToNewModType(const MODTYPE oldtype);

    orderindex_t size() const {return GetLength();}
    orderindex_t GetLength() const {return m_nSize;}

    // Returns length of sequence without counting trailing '---' items.
    orderindex_t GetLengthTailTrimmed() const;

    // Returns length of sequence stopping counting on first '---'
    // (or at the end of sequence).
    orderindex_t GetLengthFirstEmpty() const;

    //To correspond 0xFF
    patternindex_t GetInvalidPatIndex() const {
        return m_nInvalidIndex;
    }

    //To correspond 0xFE
    patternindex_t GetIgnoreIndex() const {
        return m_nIgnoreIndex;
    }

    // Returns the previous/next order ignoring skip indeces(+++).
    // If no previous/next order exists, return first/last order, and zero
    // when orderlist is empty.
    orderindex_t GetPreviousOrderIgnoringSkips(const orderindex_t start) const;
    orderindex_t GetNextOrderIgnoringSkips(const orderindex_t start) const;

    modsequence_t& operator = (const modsequence_t& seq);

    // Read/write.
    size_t WriteAsByte(FILE* f, const uint16_t count);
    size_t WriteToByteArray(uint8_t* dest, const uint32_t numOfBytes, const uint32_t destSize);
    bool ReadAsByte(const uint8_t* pFrom, const int howMany, const int memLength);

    // Deprecated function used for MPTm's created in 1.17.02.46 - 1.17.02.48.
    uint32_t Deserialize(const uint8_t* const src, const uint32_t memLength);

    //Returns true if the IT orderlist datafield is not sufficient to store orderlist information.
    bool NeedsExtraDatafield() const;

protected:
    iterator begin() {return m_pArray;}
    const_iterator begin() const {return m_pArray;}
    iterator end() {
        return m_pArray + modplug::pervasives::unwrap(m_nSize);
    }
    const_iterator end() const {
        return m_pArray + modplug::pervasives::unwrap(m_nSize);
    }

public:
    std::string m_sName;                // Sequence name.

protected:
    patternindex_t* m_pArray;       // Pointer to sequence array.
    orderindex_t m_nSize;           // Sequence length.
    orderindex_t m_nCapacity;       // Capacity in m_pArray.
    patternindex_t m_nInvalidIndex; // Invalid pat index.
    patternindex_t m_nIgnoreIndex;  // Ignore pat index.
    bool m_bDeletableArray;         // True if m_pArray points the
                                    // deletable(with delete[]) array.
    module_renderer* m_pSndFile;    // Pointer to associated CSoundFile.

    static const bool deprecated_NoArrayDelete = false;
};

inline patternindex_t invalid_index_of_modtype(const MODTYPE type) {
    return type == MOD_TYPE_MPT ?  UINT16_MAX : 0xFF;
}

inline patternindex_t ignore_index_of_modtype(const MODTYPE type) {
    return type == MOD_TYPE_MPT ? UINT16_MAX - 1 : 0xFE;
}

class deprecated_modsequence_list_t : public modsequence_t {
    friend void WriteModSequenceOld(std::ostream& oStrm, const deprecated_modsequence_list_t& seq);
    friend void ReadModSequenceOld(std::istream& iStrm, deprecated_modsequence_list_t& seq, const size_t);
    friend void WriteModSequences(std::ostream& oStrm, const deprecated_modsequence_list_t& seq);
    friend void ReadModSequences(std::istream& iStrm, deprecated_modsequence_list_t& seq, const size_t);
    friend void ReadModSequence(std::istream& iStrm, modsequence_t& seq, const size_t);

public:
    deprecated_modsequence_list_t(module_renderer& sndFile);

    const modsequence_t& GetSequence() {return GetSequence(GetCurrentSequenceIndex());}
    const modsequence_t& GetSequence(sequenceindex_t nSeq) const;
    modsequence_t& GetSequence(sequenceindex_t nSeq);
    sequenceindex_t GetNumSequences() const {return static_cast<sequenceindex_t>(m_Sequences.size());}
    void SetSequence(sequenceindex_t);                        // Sets working sequence.
    sequenceindex_t AddSequence(bool bDuplicate = true);        // Adds new sequence. If bDuplicate is true, new sequence is a duplicate of the old one. Returns the ID of the new sequence.
    void RemoveSequence() {RemoveSequence(GetCurrentSequenceIndex());}
    void RemoveSequence(sequenceindex_t);                // Removes given sequence
    sequenceindex_t GetCurrentSequenceIndex() const {return m_nCurrentSeq;}

    void OnModTypeChanged(const MODTYPE oldtype);

    deprecated_modsequence_list_t& operator=(const modsequence_t& seq) {modsequence_t::operator=(seq); return *this;}

    // Merges multiple sequences into one and destroys all other sequences.
    // Returns false if there were no sequences to merge, true otherwise.
    bool MergeSequences();

    // If there are subsongs (separated by "---" or "+++" patterns) in the module,
    // asks user whether to convert these into multiple sequences (given that the
    // modformat supports multiple sequences).
    // Returns true if sequences were modified, false otherwise.
    bool ConvertSubsongsToMultipleSequences();

    static const uint32_t s_nCacheSize = 255;

private:
    void CopyCacheToStorage();
    void CopyStorageToCache();

    patternindex_t m_Cache[s_nCacheSize];                // Local cache array.
    std::vector<modsequence_t> m_Sequences;        // Array of sequences.
    sequenceindex_t m_nCurrentSeq;                        // Index of current sequence.
};

}
}


const char FileIdSequences[] = "mptSeqC";
const char FileIdSequence[] = "mptSeq";
