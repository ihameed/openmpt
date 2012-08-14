#ifndef MOD_SEQUENCE_H
#define MOD_SEQUENCE_H

#include <vector>

class module_renderer;
class ModSequenceSet;

class ModSequence
//===============
{
public:
    friend class ModSequenceSet;
    typedef modplug::tracker::patternindex_t* iterator;
    typedef const modplug::tracker::patternindex_t* const_iterator;

    friend void WriteModSequence(std::ostream& oStrm, const ModSequence& seq);
    friend void ReadModSequence(std::istream& iStrm, ModSequence& seq, const size_t);

    virtual ~ModSequence() {if (m_bDeletableArray) delete[] m_pArray;}
    ModSequence(const ModSequence&);
    ModSequence(module_renderer& rSf, modplug::tracker::orderindex_t nSize);
    ModSequence(module_renderer& rSf, modplug::tracker::patternindex_t* pArray, modplug::tracker::orderindex_t nSize, modplug::tracker::orderindex_t nCapacity, bool bDeletableArray);

    // Initialize default sized sequence.
    void Init();

    modplug::tracker::patternindex_t& operator[](const size_t i) {ASSERT(i < m_nSize); return m_pArray[i];}
    const modplug::tracker::patternindex_t& operator[](const size_t i) const {ASSERT(i < m_nSize); return m_pArray[i];}
    modplug::tracker::patternindex_t& At(const size_t i) {return (*this)[i];}
    const modplug::tracker::patternindex_t& At(const size_t i) const {return (*this)[i];}

    modplug::tracker::patternindex_t& Last() {ASSERT(m_nSize > 0); return m_pArray[m_nSize-1];}
    const modplug::tracker::patternindex_t& Last() const {ASSERT(m_nSize > 0); return m_pArray[m_nSize-1];}

    // Returns last accessible index, i.e. GetLength() - 1. Behaviour is undefined if length is zero.
    modplug::tracker::orderindex_t GetLastIndex() const {return m_nSize - 1;}

    void Append() {Append(GetInvalidPatIndex());}        // Appends InvalidPatIndex.
    void Append(modplug::tracker::patternindex_t nPat);                                        // Appends given patindex.

    // Inserts nCount orders starting from nPos using nFill as the pattern index for all inserted orders.
    // Sequence will automatically grow if needed and if it can't grow enough, some tail
    // orders will be discarded.
    // Return: Number of orders inserted.
    modplug::tracker::orderindex_t Insert(modplug::tracker::orderindex_t nPos, modplug::tracker::orderindex_t nCount) {return Insert(nPos, nCount, GetInvalidPatIndex());}
    modplug::tracker::orderindex_t Insert(modplug::tracker::orderindex_t nPos, modplug::tracker::orderindex_t nCount, modplug::tracker::patternindex_t nFill);

    // Removes orders from range [nPosBegin, nPosEnd].
    void Remove(modplug::tracker::orderindex_t nPosBegin, modplug::tracker::orderindex_t nPosEnd);

    void clear();
    void resize(modplug::tracker::orderindex_t nNewSize) {resize(nNewSize, GetInvalidPatIndex());}
    void resize(modplug::tracker::orderindex_t nNewSize, modplug::tracker::patternindex_t nFill);

    // Replaces all occurences of nOld with nNew.
    void Replace(modplug::tracker::patternindex_t nOld, modplug::tracker::patternindex_t nNew) {if (nOld != nNew) std::replace(begin(), end(), nOld, nNew);}

    void AdjustToNewModType(const MODTYPE oldtype);

    modplug::tracker::orderindex_t size() const {return GetLength();}
    modplug::tracker::orderindex_t GetLength() const {return m_nSize;}

    // Returns length of sequence without counting trailing '---' items.
    modplug::tracker::orderindex_t GetLengthTailTrimmed() const;

    // Returns length of sequence stopping counting on first '---' (or at the end of sequence).
    modplug::tracker::orderindex_t GetLengthFirstEmpty() const;

    modplug::tracker::patternindex_t GetInvalidPatIndex() const {return m_nInvalidIndex;} //To correspond 0xFF
    static modplug::tracker::patternindex_t GetInvalidPatIndex(const MODTYPE type);

    modplug::tracker::patternindex_t GetIgnoreIndex() const {return m_nIgnoreIndex;} //To correspond 0xFE
    static modplug::tracker::patternindex_t GetIgnoreIndex(const MODTYPE type);

    // Returns the previous/next order ignoring skip indeces(+++).
    // If no previous/next order exists, return first/last order, and zero
    // when orderlist is empty.
    modplug::tracker::orderindex_t GetPreviousOrderIgnoringSkips(const modplug::tracker::orderindex_t start) const;
    modplug::tracker::orderindex_t GetNextOrderIgnoringSkips(const modplug::tracker::orderindex_t start) const;

    ModSequence& operator=(const ModSequence& seq);

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
    modplug::tracker::patternindex_t* m_pArray;                        // Pointer to sequence array.
    modplug::tracker::orderindex_t m_nSize;                                // Sequence length.
    modplug::tracker::orderindex_t m_nCapacity;                        // Capacity in m_pArray.
    modplug::tracker::patternindex_t m_nInvalidIndex;        // Invalid pat index.
    modplug::tracker::patternindex_t m_nIgnoreIndex;        // Ignore pat index.
    bool m_bDeletableArray;                        // True if m_pArray points the deletable(with delete[]) array.
    module_renderer* m_pSndFile;                        // Pointer to associated CSoundFile.

    static const bool NoArrayDelete = false;
};


inline modplug::tracker::patternindex_t ModSequence::GetInvalidPatIndex(const MODTYPE type) {return type == MOD_TYPE_MPT ?  UINT16_MAX : 0xFF;}
inline modplug::tracker::patternindex_t ModSequence::GetIgnoreIndex(const MODTYPE type) {return type == MOD_TYPE_MPT ? UINT16_MAX - 1 : 0xFE;}


//=======================================
class ModSequenceSet : public ModSequence
//=======================================
{
    friend void WriteModSequenceOld(std::ostream& oStrm, const ModSequenceSet& seq);
    friend void ReadModSequenceOld(std::istream& iStrm, ModSequenceSet& seq, const size_t);
    friend void WriteModSequences(std::ostream& oStrm, const ModSequenceSet& seq);
    friend void ReadModSequences(std::istream& iStrm, ModSequenceSet& seq, const size_t);
    friend void ReadModSequence(std::istream& iStrm, ModSequence& seq, const size_t);

public:
    ModSequenceSet(module_renderer& sndFile);

    const ModSequence& GetSequence() {return GetSequence(GetCurrentSequenceIndex());}
    const ModSequence& GetSequence(SEQUENCEINDEX nSeq) const;
    ModSequence& GetSequence(SEQUENCEINDEX nSeq);
    SEQUENCEINDEX GetNumSequences() const {return static_cast<SEQUENCEINDEX>(m_Sequences.size());}
    void SetSequence(SEQUENCEINDEX);                        // Sets working sequence.
    SEQUENCEINDEX AddSequence(bool bDuplicate = true);        // Adds new sequence. If bDuplicate is true, new sequence is a duplicate of the old one. Returns the ID of the new sequence.
    void RemoveSequence() {RemoveSequence(GetCurrentSequenceIndex());}
    void RemoveSequence(SEQUENCEINDEX);                // Removes given sequence
    SEQUENCEINDEX GetCurrentSequenceIndex() const {return m_nCurrentSeq;}

    void OnModTypeChanged(const MODTYPE oldtype);

    ModSequenceSet& operator=(const ModSequence& seq) {ModSequence::operator=(seq); return *this;}

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
    std::vector<ModSequence> m_Sequences;        // Array of sequences.
    SEQUENCEINDEX m_nCurrentSeq;                        // Index of current sequence.
};


const char FileIdSequences[] = "mptSeqC";
const char FileIdSequence[] = "mptSeq";

void WriteModSequences(std::ostream& oStrm, const ModSequenceSet& seq);
void ReadModSequences(std::istream& iStrm, ModSequenceSet& seq, const size_t nSize = 0);

void WriteModSequence(std::ostream& oStrm, const ModSequence& seq);
void ReadModSequence(std::istream& iStrm, ModSequence& seq, const size_t);

void WriteModSequenceOld(std::ostream& oStrm, const ModSequenceSet& seq);
void ReadModSequenceOld(std::istream& iStrm, ModSequenceSet& seq, const size_t);


#endif