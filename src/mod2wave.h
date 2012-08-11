#ifndef _MOD2WAVE_H_
#define _MOD2WAVE_H_
#include "tagging.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// Direct To Disk Recording

//================================
class CWaveConvert: public CDialog
//================================
{
public:
    WAVEFORMATEXTENSIBLE WaveFormat;
    ULONGLONG m_dwFileLimit;
    uint32_t m_dwSongLimit;
    bool m_bSelectPlay, m_bNormalize, m_bHighQuality, m_bGivePlugsIdleTime;
    ORDERINDEX m_nMinOrder, m_nMaxOrder;
    CComboBox m_CbnSampleRate, m_CbnSampleFormat;
    CEdit m_EditMinOrder, m_EditMaxOrder;

// -> CODE#0024
// -> DESC="wav export update"
    bool m_bChannelMode;		// Render by channel
// -! NEW_FEATURE#0024
     bool m_bInstrumentMode;	// Render by instrument

public:
    CWaveConvert(CWnd *parent, ORDERINDEX nMinOrder = ORDERINDEX_INVALID, ORDERINDEX nMaxOrder = ORDERINDEX_INVALID);

public:
    void UpdateDialog();
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange *pDX);
    virtual void OnOK();
    afx_msg void OnCheck1();
    afx_msg void OnCheck2();
    afx_msg void OnCheckChannelMode();
    afx_msg void OnCheckInstrMode();
    afx_msg void OnFormatChanged();
    afx_msg void OnPlayerOptions(); //rewbs.resamplerConf
    DECLARE_MESSAGE_MAP()
};


//==================================
class CDoWaveConvert: public CDialog
//==================================
{
public:
    PWAVEFORMATEX m_pWaveFormat;
    module_renderer *m_pSndFile;
    LPCSTR m_lpszFileName;
    uint32_t m_dwFileLimit, m_dwSongLimit;
    UINT m_nMaxPatterns;
    bool m_bAbort, m_bNormalize, m_bGivePlugsIdleTime;

public:
    CDoWaveConvert(module_renderer *sndfile, LPCSTR fname, PWAVEFORMATEX pwfx, bool bNorm, CWnd *parent = NULL):CDialog(IDD_PROGRESS, parent)
    	{ m_pSndFile = sndfile;
    	  m_lpszFileName = fname;
    	  m_pWaveFormat = pwfx;
    	  m_bAbort = false;
    	  m_bNormalize = bNorm;
    	  m_dwFileLimit = m_dwSongLimit = 0;
    	  m_nMaxPatterns = 0; }
    BOOL OnInitDialog();
    void OnCancel() { m_bAbort = true; }
    afx_msg void OnButton1();
    DECLARE_MESSAGE_MAP()
};

#endif // _MOD2WAVE_H_