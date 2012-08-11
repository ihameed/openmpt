#pragma once

class module_renderer;

// CPatternGotoDialog dialog

class CPatternGotoDialog : public CDialog
{
    DECLARE_DYNAMIC(CPatternGotoDialog)

public:
    CPatternGotoDialog(CWnd* pParent = NULL);   // standard constructor
    virtual ~CPatternGotoDialog();

    enum { IDD = IDD_EDIT_GOTO };
    DECLARE_MESSAGE_MAP()

public:
    UINT m_nRow, m_nChannel, m_nPattern, m_nOrder, m_nActiveOrder;
    void UpdatePos(UINT row, UINT chan, UINT pat, UINT ord, module_renderer* pSndFile);

protected:
    bool m_bControlLock;
    inline bool ControlsLocked() {return m_bControlLock;}
    inline void LockControls() {m_bControlLock=true;}
    inline void UnlockControls() {m_bControlLock=false;}

    module_renderer* m_pSndFile;
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual void OnOK();
    afx_msg void OnEnChangeGotoPat();


public:
    afx_msg void OnEnChangeGotoOrd();
};