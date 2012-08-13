#ifndef _DLS_BANK_H_
#define _DLS_BANK_H_

class module_renderer;

#pragma pack(1)

#define DLSMAXREGIONS            128
#define DLSMAXENVELOPES            2048

// Region Flags
#define DLSREGION_KEYGROUPMASK            0x0F
#define DLSREGION_OVERRIDEWSMP            0x10
#define DLSREGION_PINGPONGLOOP            0x20
#define DLSREGION_SAMPLELOOP            0x40
#define DLSREGION_SELFNONEXCLUSIVE    0x80
#define DLSREGION_SUSTAINLOOP            0x100


typedef struct DLSREGION
{
    uint32_t ulLoopStart;
    uint32_t ulLoopEnd;
    uint16_t nWaveLink;
    uint16_t uPercEnv;
    uint16_t usVolume;                // 0..256
    uint16_t fuOptions;        // flags + key group
    SHORT sFineTune;        // 1..100
    uint8_t uKeyMin;
    uint8_t uKeyMax;
    uint8_t uUnityNote;
} DLSREGION;

typedef struct DLSENVELOPE
{
    // Volume Envelope
    uint16_t wVolAttack;                // Attack Time: 0-1000, 1 = 20ms (1/50s) -> [0-20s]
    uint16_t wVolDecay;                        // Decay Time: 0-1000, 1 = 20ms (1/50s) -> [0-20s]
    uint16_t wVolRelease;                // Release Time: 0-1000, 1 = 20ms (1/50s) -> [0-20s]
    uint8_t nVolSustainLevel;        // Sustain Level: 0-128, 128=100%
    // Default Pan
    uint8_t nDefPan;
} DLSENVELOPE;

// Special Bank bits
#define F_INSTRUMENT_DRUMS            0x80000000

typedef struct DLSINSTRUMENT
{
    uint32_t ulBank, ulInstrument;
    UINT nRegions, nMelodicEnv;
    DLSREGION Regions[DLSMAXREGIONS];
    CHAR szName[32];
    // SF2 stuff (DO NOT USE! -> used internally by the SF2 loader)
    uint16_t wPresetBagNdx, wPresetBagNum;
} DLSINSTRUMENT;

typedef struct DLSSAMPLEEX
{
    CHAR szName[20];
    uint32_t dwLen;
    uint32_t dwStartloop;
    uint32_t dwEndloop;
    uint32_t dwSampleRate;
    uint8_t byOriginalPitch;
    CHAR chPitchCorrection;
} DLSSAMPLEEX;

#pragma pack()

#define SOUNDBANK_TYPE_INVALID    0
#define SOUNDBANK_TYPE_DLS            0x01
#define SOUNDBANK_TYPE_SF2            0x02

typedef struct SOUNDBANKINFO
{
    CHAR szBankName[256];
    CHAR szCopyRight[256];
    CHAR szComments[512];
    CHAR szEngineer[256];
    CHAR szSoftware[256];                // ISFT: Software
    CHAR szDescription[256];        // ISBJ: Subject
} SOUNDBANKINFO;


//============
class CDLSBank
//============
{
protected:
    SOUNDBANKINFO m_BankInfo;
    CHAR m_szFileName[_MAX_PATH];
    UINT m_nType;
    uint32_t m_dwWavePoolOffset;
    // DLS Information
    UINT m_nInstruments, m_nWaveForms, m_nEnvelopes, m_nSamplesEx, m_nMaxWaveLink;
    uint32_t *m_pWaveForms;
    DLSINSTRUMENT *m_pInstruments;
    DLSSAMPLEEX *m_pSamplesEx;
    DLSENVELOPE m_Envelopes[DLSMAXENVELOPES];

public:
    CDLSBank();
    virtual ~CDLSBank();
    void Destroy();
    static BOOL IsDLSBank(LPCSTR lpszFileName);
    static uint32_t MakeMelodicCode(UINT bank, UINT instr) { return ((bank << 16) | (instr));}
    static uint32_t MakeDrumCode(UINT rgn, UINT instr) { return (0x80000000 | (rgn << 16) | (instr));}

public:
    BOOL Open(LPCSTR lpszFileName);
    LPCSTR GetFileName() const { return m_szFileName; }
    UINT GetBankType() const { return m_nType; }
    UINT GetBankInfo(SOUNDBANKINFO *pBankInfo=NULL) const { if (pBankInfo) *pBankInfo = m_BankInfo; return m_nType; }

public:
    UINT GetNumInstruments() const { return m_nInstruments; }
    UINT GetNumSamples() const { return m_nWaveForms; }
    DLSINSTRUMENT *GetInstrument(UINT iIns) { return (m_pInstruments) ? &m_pInstruments[iIns] : NULL; }
    DLSINSTRUMENT *FindInstrument(BOOL bDrum, UINT nBank=0xFF, uint32_t dwProgram=0xFF, uint32_t dwKey=0xFF, UINT *pInsNo=NULL);
    UINT GetRegionFromKey(UINT nIns, UINT nKey);
    BOOL FreeWaveForm(LPBYTE p);
    BOOL ExtractWaveForm(UINT nIns, UINT nRgn, LPBYTE *ppWave, uint32_t *pLen);
    BOOL ExtractSample(module_renderer *pSndFile, UINT nSample, UINT nIns, UINT nRgn, int transpose=0);
    BOOL ExtractInstrument(module_renderer *pSndFile, UINT nInstr, UINT nIns, UINT nDrumRgn);

// Internal Loader Functions
protected:
    BOOL UpdateInstrumentDefinition(DLSINSTRUMENT *pDlsIns, LPVOID pchunk, uint32_t dwMaxLen);
    BOOL UpdateSF2PresetData(LPVOID psf2info, LPVOID pchunk, uint32_t dwMaxLen);
    BOOL ConvertSF2ToDLS(LPVOID psf2info);

public:
    // DLS Unit conversion
    static LONG __cdecl DLS32BitTimeCentsToMilliseconds(LONG lTimeCents);
    static LONG __cdecl DLS32BitRelativeGainToLinear(LONG lCentibels);        // 0dB = 0x10000
    static LONG __cdecl DLS32BitRelativeLinearToGain(LONG lGain);                // 0dB = 0x10000
    static LONG __cdecl DLSMidiVolumeToLinear(UINT nMidiVolume);                // [0-127] -> [0-0x10000]
};


#endif