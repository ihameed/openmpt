#ifndef _MAINBAR_H_
#define _MAINBAR_H_

#define MIN_BASEOCTAVE            0
#define MAX_BASEOCTAVE            8

class module_renderer;
class CModDoc;

//===============================
class CToolBarEx: public CToolBar
//===============================
{
protected:
    BOOL m_bVertical, m_bFlatButtons;

public:
    CToolBarEx() { m_bVertical = m_bFlatButtons = FALSE; }
    virtual ~CToolBarEx() {}

public:
    BOOL EnableControl(CWnd &wnd, UINT nIndex, UINT nHeight=0);
    void ChangeCtrlStyle(LONG lStyle, BOOL bSetStyle);
    void EnableFlatButtons(BOOL bFlat);

public:
    //{{AFX_VIRTUAL(CToolBarEx)
    virtual CSize CalcDynamicLayout(int nLength, uint32_t dwMode);
    virtual BOOL SetHorizontal();
    virtual BOOL SetVertical();
    //}}AFX_VIRTUAL
};


//===================================
class CMainToolBar: public CToolBarEx
//===================================
{
protected:
    CStatic m_EditTempo, m_EditSpeed, m_EditOctave, m_EditRowsPerBeat;
    CStatic m_StaticTempo, m_StaticSpeed, m_StaticRowsPerBeat;
    CSpinButtonCtrl m_SpinTempo, m_SpinSpeed, m_SpinOctave, m_SpinRowsPerBeat;
    int nCurrentSpeed, nCurrentTempo, nCurrentOctave, nCurrentRowsPerBeat;

public:
    CMainToolBar() {}
    virtual ~CMainToolBar() {}

protected:
    void SetRowsPerBeat(modplug::tracker::rowindex_t nNewRPB);

public:
    //{{AFX_VIRTUAL(CMainToolBar)
    virtual BOOL SetHorizontal();
    virtual BOOL SetVertical();
    //}}AFX_VIRTUAL

public:
    BOOL Create(CWnd *parent);
    void Init(CMainFrame *);
    UINT GetBaseOctave();
    BOOL SetBaseOctave(UINT nOctave);
    BOOL SetCurrentSong(module_renderer *pModDoc);

protected:
    //{{AFX_MSG(CMainToolBar)
    afx_msg void OnVScroll(UINT, UINT, CScrollBar *);
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};


#define MTB_VERTICAL    0x01
#define MTB_CAPTURE            0x02
#define MTB_DRAGGING    0x04
#define MTB_TRACKER            0x08


#endif
