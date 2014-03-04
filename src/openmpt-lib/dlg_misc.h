#ifndef _MPT_DLG_MISC_H_
#define _MPT_DLG_MISC_H_

class module_renderer;
class CModDoc;
class CDLSBank;

//===============================
class CModTypeDlg: public CDialog
//===============================
{
public:
    CComboBox m_TypeBox, m_ChannelsBox, m_TempoModeBox, m_PlugMixBox;
    CButton m_CheckBox1, m_CheckBox2, m_CheckBox3, m_CheckBox4, m_CheckBox5, m_CheckBoxPT1x;
    module_renderer *m_pSndFile;
    UINT m_nChannels;
    MODTYPE m_nType;
    uint32_t m_dwSongFlags;

// -> CODE#0023
// -> DESC="IT project files (.itp)"
    CButton m_CheckBox6;
// -! NEW_FEATURE#0023

public:
    CModTypeDlg(module_renderer *pSndFile, CWnd *parent):CDialog(IDD_MODDOC_MODTYPE, parent) { m_pSndFile = pSndFile; m_nType = MOD_TYPE_NONE; m_nChannels = 0; }
    bool VerifyData();
    void UpdateDialog();

private:
    void UpdateChannelCBox();

protected:
    //{{AFX_VIRTUAL(CModTypeDlg)
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual void OnOK();
    virtual void OnCancel();

    //}}AFX_VIRTUAL

    BOOL OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult);

    //{{AFX_MSG(CModTypeDlg)
    afx_msg void OnCheck1();
    afx_msg void OnCheck2();
    afx_msg void OnCheck3();
    afx_msg void OnCheck4();
    afx_msg void OnCheck5();
// -> CODE#0023
// -> DESC="IT project files (.itp)"
    afx_msg void OnCheck6();
// -! NEW_FEATURE#0023
    afx_msg void OnCheckPT1x();
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};


//===============================
class CShowLogDlg: public CDialog
//===============================
{
public:
    LPCSTR m_lpszLog, m_lpszTitle;
    CEdit m_EditLog;

public:
    CShowLogDlg(CWnd *parent=NULL):CDialog(IDD_SHOWLOG, parent) { m_lpszLog = NULL; m_lpszTitle = NULL; }
    UINT ShowLog(LPCSTR pszLog, LPCSTR lpszTitle=NULL);

protected:
    //{{AFX_VIRTUAL(CShowLogDlg)
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    //}}AFX_VIRTUAL
    DECLARE_MESSAGE_MAP()
};


//======================================
class CRemoveChannelsDlg: public CDialog
//======================================
{
public:
    module_renderer *m_pSndFile;
    vector<bool> m_bKeepMask;
    modplug::tracker::chnindex_t m_nChannels, m_nRemove;
    CListBox m_RemChansList;                //rewbs.removeChansDlgCleanup
    bool m_ShowCancel;

public:
    CRemoveChannelsDlg(module_renderer *pSndFile, modplug::tracker::chnindex_t nChns, bool showCancel = true, CWnd *parent=NULL):CDialog(IDD_REMOVECHANNELS, parent)
            { m_pSndFile = pSndFile;
              m_nChannels = m_pSndFile->GetNumChannels();
              m_nRemove = nChns;
              m_bKeepMask.assign(m_nChannels, true);
              m_ShowCancel = showCancel;
            }

protected:
    //{{AFX_VIRTUAL(CRemoveChannelsDlg)
    virtual void DoDataExchange(CDataExchange* pDX); //rewbs.removeChansDlgCleanup
    virtual BOOL OnInitDialog();
    virtual void OnOK();
    //}}AFX_VIRTUAL
    //{{AFX_MSG(CRemoveChannelsDlg)
    afx_msg void OnChannelChanged();
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP();
};


/////////////////////////////////////////////////////////////////////////
// Keyboard control

enum
{
    KBDNOTIFY_MOUSEMOVE=0,
    KBDNOTIFY_LBUTTONDOWN,
    KBDNOTIFY_LBUTTONUP,
};

//=================================
class CKeyboardControl: public CWnd
//=================================
{
public:
    enum
    {
            KEYFLAG_NORMAL=0,
            KEYFLAG_REDDOT,
    };
protected:
    HWND m_hParent;
    UINT m_nOctaves;
    int m_nSelection;
    BOOL m_bCapture, m_bCursorNotify;
    uint8_t KeyFlags[modplug::tracker::NoteMax]; // 10 octaves bad_max

public:
    CKeyboardControl() { m_hParent = NULL; m_nOctaves = 1; m_nSelection = -1; m_bCapture = FALSE; }

public:
    void Init(HWND parent, UINT nOctaves=1, BOOL bCursNotify=FALSE) { m_hParent = parent;
    m_nOctaves = nOctaves; m_bCursorNotify = bCursNotify; memset(KeyFlags, 0, sizeof(KeyFlags)); }
    void SetFlags(UINT key, UINT flags) { if (key < modplug::tracker::NoteMax) KeyFlags[key] = (uint8_t)flags; }
    UINT GetFlags(UINT key) const { return (key < modplug::tracker::NoteMax) ? KeyFlags[key] : 0; }
    afx_msg void OnPaint();
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    DECLARE_MESSAGE_MAP()
};



/////////////////////////////////////////////////////////////////////////
// Messagebox with 'don't show again'-option.

// Enums for message entries. See dlg_misc.cpp for the array of entries.
enum enMsgBoxHidableMessage
{
    ModCompatibilityExportTip                = 0,
    ItCompatibilityExportTip                = 1,
    ConfirmSignUnsignWhenPlaying        = 2,
    XMCompatibilityExportTip                = 3,
    enMsgBoxHidableMessage_count
};

void MsgBoxHidable(enMsgBoxHidableMessage enMsg);

#endif
