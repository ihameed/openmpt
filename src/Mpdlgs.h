#ifndef _MODPLUGDLGS_
#define _MODPLUGDLGS_

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