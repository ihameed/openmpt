#ifndef _MODPLUGDLGS_
#define _MODPLUGDLGS_

#define NUMMIXRATE    16

class module_renderer;
class CMainFrame;



//=================================
class CEQSlider: public CSliderCtrl
//=================================
{
public:
    CWnd *m_pParent;
    UINT m_nSliderNo;
    short int m_x, m_y;
public:
    CEQSlider() {}
    VOID Init(UINT nID, UINT n, CWnd *parent);
    BOOL PreTranslateMessage(MSG *pMsg);
};


//=====================================
class CEQSetupDlg: public CPropertyPage
//=====================================
{
protected:
    CEQSlider m_Sliders[MAX_EQ_BANDS];
    PEQPRESET m_pEqPreset;
    UINT m_nSliderMenu;

public:
    CEQSetupDlg(PEQPRESET pEq):CPropertyPage(IDD_SETUP_EQ) { m_pEqPreset = pEq; }
    void UpdateDialog();
    void UpdateEQ(BOOL bReset);

public:
    static EQPRESET gUserPresets[4];
    static VOID LoadEQ(HKEY key, LPCSTR pszName, PEQPRESET pEqSettings);
    static VOID SaveEQ(HKEY key, LPCSTR pszName, PEQPRESET pEqSettings);

protected:
    virtual BOOL OnInitDialog();
    virtual BOOL OnSetActive();
    afx_msg void OnSettingsChanged() { SetModified(TRUE); }
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnEqFlat();
    afx_msg void OnEqJazz();
    afx_msg void OnEqPop();
    afx_msg void OnEqRock();
    afx_msg void OnEqConcert();
    afx_msg void OnEqClear();
    afx_msg void OnEqUser1();
    afx_msg void OnEqUser2();
    afx_msg void OnEqUser3();
    afx_msg void OnEqUser4();
    afx_msg void OnSavePreset();
    afx_msg void OnSliderMenu(UINT);
    afx_msg void OnSliderFreq(UINT);
    DECLARE_MESSAGE_MAP()
};


//=======================================
class CMidiSetupDlg: public CPropertyPage
//=======================================
{
public:
    uint32_t m_dwMidiSetup;
    LONG m_nMidiDevice;

protected:
    CSpinButtonCtrl m_SpinSpd, m_SpinPat;

public:
    CMidiSetupDlg(uint32_t d, LONG n):CPropertyPage(IDD_OPTIONS_MIDI)
    	{ m_dwMidiSetup = d; m_nMidiDevice = n; }

protected:
    virtual BOOL OnInitDialog();
    virtual void OnOK();
    virtual BOOL OnSetActive();
    virtual void DoDataExchange(CDataExchange* pDX);
    afx_msg void OnSettingsChanged() { SetModified(TRUE); }
    DECLARE_MESSAGE_MAP()
};

#endif