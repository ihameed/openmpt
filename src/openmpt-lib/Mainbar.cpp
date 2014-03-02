#include "stdafx.h"
#include "mainfrm.h"
#include "moddoc.h"

/////////////////////////////////////////////////////////////////////
// CToolBarEx: custom toolbar base class

BOOL CToolBarEx::SetHorizontal()
//------------------------------
{
    m_bVertical = FALSE;
    SetBarStyle(GetBarStyle() | CBRS_ALIGN_TOP);
    return TRUE;
}


BOOL CToolBarEx::SetVertical()
//----------------------------
{
    m_bVertical = TRUE;
    return TRUE;
}


CSize CToolBarEx::CalcDynamicLayout(int nLength, uint32_t dwMode)
//------------------------------------------------------------
{
    CSize sizeResult;
    // if we're committing set the buttons appropriately
    if (dwMode & LM_COMMIT)
    {
            if (dwMode & LM_VERTDOCK)
            {
                    if (!m_bVertical)
                            SetVertical();
            } else
            {
                    if (m_bVertical)
                            SetHorizontal();
            }
            sizeResult = CToolBar::CalcDynamicLayout(nLength, dwMode);
    } else
    {
            BOOL bOld = m_bVertical;
            BOOL bSwitch = (dwMode & LM_HORZ) ? bOld : !bOld;

            if (bSwitch)
            {
                    if (bOld)
                            SetHorizontal();
                    else
                            SetVertical();
            }

            sizeResult = CToolBar::CalcDynamicLayout(nLength, dwMode);

            if (bSwitch)
            {
                    if (bOld)
                            SetHorizontal();
                    else
                            SetVertical();
            }
    }
#if 0
    // Hack - fixed in VC++ 6.0
    if ((sizeResult.cy > 30) && (m_bFlatButtons))
    {
            if (dwMode & LM_HORZ)
                    sizeResult.cy += ((sizeResult.cy-30)/28) * 4;
            else
                    sizeResult.cy += 8;
    }
#endif
    return sizeResult;
}


BOOL CToolBarEx::EnableControl(CWnd &wnd, UINT nIndex, UINT nHeight)
//------------------------------------------------------------------
{
    if (wnd.m_hWnd != NULL)
    {
            CRect rect;
            GetItemRect(nIndex, rect);
            if (nHeight)
            {
                    int n = (rect.bottom + rect.top - nHeight) / 2;
                    if (n > rect.top) rect.top = n;
            }
            wnd.SetWindowPos(NULL, rect.left, rect.top, 0, 0, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOCOPYBITS);
            wnd.ShowWindow(SW_SHOW);
    }
    return TRUE;
}


void CToolBarEx::ChangeCtrlStyle(long lStyle, BOOL bSetStyle)
//-----------------------------------------------------------
{
    if (m_hWnd)
    {
            LONG lStyleOld = GetWindowLong(m_hWnd, GWL_STYLE);
            if (bSetStyle)
                    lStyleOld |= lStyle;
            else
                    lStyleOld &= ~lStyle;
            SetWindowLong(m_hWnd, GWL_STYLE, lStyleOld);
            SetWindowPos(NULL, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE);
            Invalidate();
    }
}


void CToolBarEx::EnableFlatButtons(BOOL bFlat)
//--------------------------------------------
{
    m_bFlatButtons = bFlat;
    ChangeCtrlStyle(TBSTYLE_FLAT, bFlat);
}


/////////////////////////////////////////////////////////////////////
// CMainToolBar

// Play Command
#define PLAYCMD_INDEX            10
#define TOOLBAR_IMAGE_PAUSE    8
#define TOOLBAR_IMAGE_PLAY    16
// Base octave
#define EDITOCTAVE_INDEX    13
#define EDITOCTAVE_WIDTH    55
#define EDITOCTAVE_HEIGHT    20
#define SPINOCTAVE_INDEX    (EDITOCTAVE_INDEX+1)
#define SPINOCTAVE_WIDTH    16
#define SPINOCTAVE_HEIGHT    (EDITOCTAVE_HEIGHT)
// Static "Tempo:"
#define TEMPOTEXT_INDEX            16
#define TEMPOTEXT_WIDTH            45
#define TEMPOTEXT_HEIGHT    20
// Edit Tempo
#define EDITTEMPO_INDEX            (TEMPOTEXT_INDEX+1)
#define EDITTEMPO_WIDTH            32
#define EDITTEMPO_HEIGHT    20
// Spin Tempo
#define SPINTEMPO_INDEX            (EDITTEMPO_INDEX+1)
#define SPINTEMPO_WIDTH            16
#define SPINTEMPO_HEIGHT    (EDITTEMPO_HEIGHT)
// Static "Speed:"
#define SPEEDTEXT_INDEX            20
#define SPEEDTEXT_WIDTH            57
#define SPEEDTEXT_HEIGHT    (TEMPOTEXT_HEIGHT)
// Edit Speed
#define EDITSPEED_INDEX            (SPEEDTEXT_INDEX+1)
#define EDITSPEED_WIDTH            28
#define EDITSPEED_HEIGHT    (EDITTEMPO_HEIGHT)
// Spin Speed
#define SPINSPEED_INDEX            (EDITSPEED_INDEX+1)
#define SPINSPEED_WIDTH            16
#define SPINSPEED_HEIGHT    (EDITSPEED_HEIGHT)
// Static "Rows/Beat:"
#define RPBTEXT_INDEX            24
#define RPBTEXT_WIDTH            63
#define RPBTEXT_HEIGHT            (TEMPOTEXT_HEIGHT)
// Edit Speed
#define EDITRPB_INDEX            (RPBTEXT_INDEX+1)
#define EDITRPB_WIDTH            28
#define EDITRPB_HEIGHT            (EDITTEMPO_HEIGHT)
// Spin Speed
#define SPINRPB_INDEX            (EDITRPB_INDEX+1)
#define SPINRPB_WIDTH            16
#define SPINRPB_HEIGHT            (EDITRPB_HEIGHT)

static UINT BASED_CODE MainButtons[] =
{
    // same order as in the bitmap 'mainbar.bmp'
    ID_FILE_NEW,
    ID_FILE_OPEN,
    ID_FILE_SAVE,
            ID_SEPARATOR,
    ID_EDIT_CUT,
    ID_EDIT_COPY,
    ID_EDIT_PASTE,
            ID_SEPARATOR,
    ID_MIDI_RECORD,
    ID_PLAYER_STOP,
    ID_PLAYER_PAUSE,
    ID_PLAYER_PLAYFROMSTART,
            ID_SEPARATOR,
            ID_SEPARATOR,
            ID_SEPARATOR,
            ID_SEPARATOR,
            ID_SEPARATOR,
            ID_SEPARATOR,
            ID_SEPARATOR,
            ID_SEPARATOR,
            ID_SEPARATOR,
            ID_SEPARATOR,
            ID_SEPARATOR,
            ID_SEPARATOR,

            ID_SEPARATOR,
            ID_SEPARATOR,
            ID_SEPARATOR,
            ID_SEPARATOR,
    ID_VIEW_OPTIONS,
    ID_APP_ABOUT,
//    ID_CONTEXT_HELP,
            ID_SEPARATOR,        //rewbs.reportBug
    ID_REPORT_BUG,                //rewbs.reportBug
            ID_SEPARATOR,
    ID_PANIC,
};


BEGIN_MESSAGE_MAP(CMainToolBar, CToolBarEx)
    ON_WM_VSCROLL()
END_MESSAGE_MAP()



BOOL CMainToolBar::Create(CWnd *parent)
//-------------------------------------
{
    CRect rect;
    uint32_t dwStyle = WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_SIZE_DYNAMIC | CBRS_TOOLTIPS | CBRS_FLYBY;

    if (!CToolBar::Create(parent, dwStyle)) return FALSE;
    if (!LoadBitmap(IDB_MAINBAR)) return FALSE;
    if (!SetButtons(MainButtons, CountOf(MainButtons))) return FALSE;

    nCurrentSpeed = 6;
    nCurrentTempo = 125;
    nCurrentRowsPerBeat = 4;
    nCurrentOctave = -1;

    // Octave Edit Box
    rect.SetRect(-EDITOCTAVE_WIDTH, -EDITOCTAVE_HEIGHT, 0, 0);
    if (!m_EditOctave.Create("", WS_CHILD|WS_BORDER|SS_LEFT|SS_CENTERIMAGE, rect, this, IDC_EDIT_BASEOCTAVE)) return FALSE;
    rect.SetRect(-SPINOCTAVE_WIDTH, -SPINOCTAVE_HEIGHT, 0, 0);
    m_SpinOctave.Create(WS_CHILD|UDS_ALIGNRIGHT, rect, this, IDC_SPIN_BASEOCTAVE);

    // Tempo Text
    rect.SetRect(-TEMPOTEXT_WIDTH, -TEMPOTEXT_HEIGHT, 0, 0);
    if (!m_StaticTempo.Create("Tempo:", WS_CHILD|SS_CENTER|SS_CENTERIMAGE, rect, this, IDC_TEXT_CURRENTTEMPO)) return FALSE;
    // Tempo EditBox
    rect.SetRect(-EDITTEMPO_WIDTH, -EDITTEMPO_HEIGHT, 0, 0);
    if (!m_EditTempo.Create("---", WS_CHILD|WS_BORDER|SS_LEFT|SS_CENTERIMAGE , rect, this, IDC_EDIT_CURRENTTEMPO)) return FALSE;
    // Tempo Spin
    rect.SetRect(-SPINTEMPO_WIDTH, -SPINTEMPO_HEIGHT, 0, 0);
    m_SpinTempo.Create(WS_CHILD|UDS_ALIGNRIGHT, rect, this, IDC_SPIN_CURRENTTEMPO);

    // Speed Text
    rect.SetRect(-SPEEDTEXT_WIDTH, -SPEEDTEXT_HEIGHT, 0, 0);
    if (!m_StaticSpeed.Create("Ticks/Row:", WS_CHILD|SS_CENTER|SS_CENTERIMAGE, rect, this, IDC_TEXT_CURRENTSPEED)) return FALSE;
    // Speed EditBox
    rect.SetRect(-EDITSPEED_WIDTH, -EDITSPEED_HEIGHT, 0, 0);
    if (!m_EditSpeed.Create("---", WS_CHILD|WS_BORDER|SS_LEFT|SS_CENTERIMAGE , rect, this, IDC_EDIT_CURRENTSPEED)) return FALSE;
    // Speed Spin
    rect.SetRect(-SPINSPEED_WIDTH, -SPINSPEED_HEIGHT, 0, 0);
    m_SpinSpeed.Create(WS_CHILD|UDS_ALIGNRIGHT, rect, this, IDC_SPIN_CURRENTSPEED);

    // Rows per Beat Text
    rect.SetRect(-RPBTEXT_WIDTH, -RPBTEXT_HEIGHT, 0, 0);
    if (!m_StaticRowsPerBeat.Create("Rows/Beat:", WS_CHILD|SS_CENTER|SS_CENTERIMAGE, rect, this, IDC_TEXT_RPB)) return FALSE;
    // Rows per Beat EditBox
    rect.SetRect(-EDITRPB_WIDTH, -EDITRPB_HEIGHT, 0, 0);
    if (!m_EditRowsPerBeat.Create("---", WS_CHILD|WS_BORDER|SS_LEFT|SS_CENTERIMAGE , rect, this, IDC_EDIT_RPB)) return FALSE;
    // Rows per Beat Spin
    rect.SetRect(-SPINRPB_WIDTH, -SPINRPB_HEIGHT, 0, 0);
    m_SpinRowsPerBeat.Create(WS_CHILD|UDS_ALIGNRIGHT, rect, this, IDC_SPIN_RPB);


    // Adjust control styles
    HFONT hFont = CMainFrame::GetGUIFont();
      m_EditOctave.SendMessage(WM_SETFONT, (WPARAM)hFont);
    m_EditOctave.ModifyStyleEx(0, WS_EX_STATICEDGE, SWP_NOACTIVATE);
    m_StaticTempo.SendMessage(WM_SETFONT, (WPARAM)hFont);
    m_EditTempo.SendMessage(WM_SETFONT, (WPARAM)hFont);
    m_EditTempo.ModifyStyleEx(0, WS_EX_STATICEDGE, SWP_NOACTIVATE);
    m_StaticSpeed.SendMessage(WM_SETFONT, (WPARAM)hFont);
    m_EditSpeed.SendMessage(WM_SETFONT, (WPARAM)hFont);
    m_EditSpeed.ModifyStyleEx(0, WS_EX_STATICEDGE, SWP_NOACTIVATE);
    m_StaticRowsPerBeat.SendMessage(WM_SETFONT, (WPARAM)hFont);
    m_EditRowsPerBeat.SendMessage(WM_SETFONT, (WPARAM)hFont);
    m_EditRowsPerBeat.ModifyStyleEx(0, WS_EX_STATICEDGE, SWP_NOACTIVATE);
    m_SpinOctave.SetRange(MIN_BASEOCTAVE, MAX_BASEOCTAVE);
    m_SpinOctave.SetPos(4);
    m_SpinTempo.SetRange(-1, 1);
    m_SpinTempo.SetPos(0);
    m_SpinSpeed.SetRange(-1, 1);
    m_SpinSpeed.SetPos(0);
    m_SpinRowsPerBeat.SetRange(-1, 1);
    m_SpinRowsPerBeat.SetPos(0);
    // Display everything
    SetWindowText(_T("Main"));
    SetBaseOctave(4);
    SetCurrentSong(NULL);
    EnableDocking(CBRS_ALIGN_ANY);
    return TRUE;
}


void CMainToolBar::Init(CMainFrame *pMainFrm)
//-------------------------------------------
{
    EnableFlatButtons(CMainFrame::m_dwPatternSetup & PATTERN_FLATBUTTONS);
    SetHorizontal();
    pMainFrm->DockControlBar(this);
}


BOOL CMainToolBar::SetHorizontal()
//--------------------------------
{
    CToolBarEx::SetHorizontal();
    SetButtonInfo(EDITOCTAVE_INDEX, IDC_EDIT_BASEOCTAVE, TBBS_SEPARATOR, EDITOCTAVE_WIDTH);
    SetButtonInfo(SPINOCTAVE_INDEX, IDC_SPIN_BASEOCTAVE, TBBS_SEPARATOR, SPINOCTAVE_WIDTH);
    SetButtonInfo(TEMPOTEXT_INDEX, IDC_TEXT_CURRENTTEMPO, TBBS_SEPARATOR, TEMPOTEXT_WIDTH);
    SetButtonInfo(EDITTEMPO_INDEX, IDC_EDIT_CURRENTTEMPO, TBBS_SEPARATOR, EDITTEMPO_WIDTH);
    SetButtonInfo(SPINTEMPO_INDEX, IDC_SPIN_CURRENTTEMPO, TBBS_SEPARATOR, SPINTEMPO_WIDTH);
    SetButtonInfo(SPEEDTEXT_INDEX, IDC_TEXT_CURRENTSPEED, TBBS_SEPARATOR, SPEEDTEXT_WIDTH);
    SetButtonInfo(EDITSPEED_INDEX, IDC_EDIT_CURRENTSPEED, TBBS_SEPARATOR, EDITSPEED_WIDTH);
    SetButtonInfo(SPINSPEED_INDEX, IDC_SPIN_CURRENTSPEED, TBBS_SEPARATOR, SPINSPEED_WIDTH);
    SetButtonInfo(RPBTEXT_INDEX, IDC_TEXT_RPB, TBBS_SEPARATOR, RPBTEXT_WIDTH);
    SetButtonInfo(EDITRPB_INDEX, IDC_EDIT_RPB, TBBS_SEPARATOR, EDITRPB_WIDTH);
    SetButtonInfo(SPINRPB_INDEX, IDC_SPIN_RPB, TBBS_SEPARATOR, SPINRPB_WIDTH);

    //SetButtonInfo(SPINSPEED_INDEX+1, IDC_TEXT_BPM, TBBS_SEPARATOR, SPEEDTEXT_WIDTH);
    // Octave Box
    EnableControl(m_EditOctave, EDITOCTAVE_INDEX);
    EnableControl(m_SpinOctave, SPINOCTAVE_INDEX);
    // Tempo
    EnableControl(m_StaticTempo, TEMPOTEXT_INDEX, TEMPOTEXT_HEIGHT);
    EnableControl(m_EditTempo, EDITTEMPO_INDEX, EDITTEMPO_HEIGHT);
    EnableControl(m_SpinTempo, SPINTEMPO_INDEX, SPINTEMPO_HEIGHT);
    // Speed
    EnableControl(m_StaticSpeed, SPEEDTEXT_INDEX, SPEEDTEXT_HEIGHT);
    EnableControl(m_EditSpeed, EDITSPEED_INDEX, EDITSPEED_HEIGHT);
    EnableControl(m_SpinSpeed, SPINSPEED_INDEX, SPINSPEED_HEIGHT);
    // Rows per Beat
    EnableControl(m_StaticRowsPerBeat, RPBTEXT_INDEX, RPBTEXT_HEIGHT);
    EnableControl(m_EditRowsPerBeat, EDITRPB_INDEX, EDITRPB_HEIGHT);
    EnableControl(m_SpinRowsPerBeat, SPINRPB_INDEX, SPINRPB_HEIGHT);

    return TRUE;
}


BOOL CMainToolBar::SetVertical()
//------------------------------
{
    CToolBarEx::SetVertical();
    // Change Buttons
    SetButtonInfo(EDITOCTAVE_INDEX, ID_SEPARATOR, TBBS_SEPARATOR, 1);
    SetButtonInfo(SPINOCTAVE_INDEX, ID_SEPARATOR, TBBS_SEPARATOR, 1);
    SetButtonInfo(TEMPOTEXT_INDEX, ID_SEPARATOR, TBBS_SEPARATOR, 1);
    SetButtonInfo(EDITTEMPO_INDEX, ID_SEPARATOR, TBBS_SEPARATOR, 1);
    SetButtonInfo(SPINTEMPO_INDEX, ID_SEPARATOR, TBBS_SEPARATOR, 1);
    SetButtonInfo(SPEEDTEXT_INDEX, ID_SEPARATOR, TBBS_SEPARATOR, 1);
    SetButtonInfo(EDITSPEED_INDEX, ID_SEPARATOR, TBBS_SEPARATOR, 1);
    SetButtonInfo(SPINSPEED_INDEX, ID_SEPARATOR, TBBS_SEPARATOR, 1);
    SetButtonInfo(RPBTEXT_INDEX, ID_SEPARATOR, TBBS_SEPARATOR, 1);
    SetButtonInfo(EDITRPB_INDEX, ID_SEPARATOR, TBBS_SEPARATOR, 1);
    SetButtonInfo(SPINRPB_INDEX, ID_SEPARATOR, TBBS_SEPARATOR, 1);

    // Hide Controls
    if (m_EditOctave.m_hWnd != NULL) m_EditOctave.ShowWindow(SW_HIDE);
    if (m_SpinOctave.m_hWnd != NULL) m_SpinOctave.ShowWindow(SW_HIDE);
    if (m_StaticTempo.m_hWnd != NULL) m_StaticTempo.ShowWindow(SW_HIDE);
    if (m_EditTempo.m_hWnd != NULL) m_EditTempo.ShowWindow(SW_HIDE);
    if (m_SpinTempo.m_hWnd != NULL) m_SpinTempo.ShowWindow(SW_HIDE);
    if (m_StaticSpeed.m_hWnd != NULL) m_StaticSpeed.ShowWindow(SW_HIDE);
    if (m_EditSpeed.m_hWnd != NULL) m_EditSpeed.ShowWindow(SW_HIDE);
    if (m_SpinSpeed.m_hWnd != NULL) m_SpinSpeed.ShowWindow(SW_HIDE);
    if (m_StaticRowsPerBeat.m_hWnd != NULL) m_StaticRowsPerBeat.ShowWindow(SW_HIDE);
    if (m_EditRowsPerBeat.m_hWnd != NULL) m_EditRowsPerBeat.ShowWindow(SW_HIDE);
    if (m_SpinRowsPerBeat.m_hWnd != NULL) m_SpinRowsPerBeat.ShowWindow(SW_HIDE);
    //if (m_StaticBPM.m_hWnd != NULL) m_StaticBPM.ShowWindow(SW_HIDE);
    return TRUE;
}


UINT CMainToolBar::GetBaseOctave()
//--------------------------------
{
    if (nCurrentOctave >= MIN_BASEOCTAVE) return (UINT)nCurrentOctave;
    return 4;
}


BOOL CMainToolBar::SetBaseOctave(UINT nOctave)
//--------------------------------------------
{
    CHAR s[64];

    if ((nOctave < MIN_BASEOCTAVE) || (nOctave > MAX_BASEOCTAVE)) return FALSE;
    if (nOctave != (UINT)nCurrentOctave)
    {
            nCurrentOctave = nOctave;
            wsprintf(s, " Octave %d", nOctave);
            m_EditOctave.SetWindowText(s);
            m_SpinOctave.SetPos(nOctave);
    }
    return TRUE;
}


BOOL CMainToolBar::SetCurrentSong(module_renderer *pSndFile)
//-----------------------------------------------------
{
    // Update Info
    if (pSndFile)
    {
            CHAR s[256];
            // Update play/pause button
            if (nCurrentTempo == -1) SetButtonInfo(PLAYCMD_INDEX, ID_PLAYER_PAUSE, TBBS_BUTTON, TOOLBAR_IMAGE_PAUSE);
            // Update Speed
            int nSpeed = pSndFile->m_nMusicSpeed;
            if (nSpeed != nCurrentSpeed)
            {
                    //rewbs.envRowGrid
                    CModDoc *pModDoc = CMainFrame::GetMainFrame()->GetActiveDoc();
                    if (pModDoc) {
                            pModDoc->UpdateAllViews(NULL, HINT_SPEEDCHANGE);
                    }
                    //end rewbs.envRowGrid

                    if (nCurrentSpeed < 0) m_SpinSpeed.EnableWindow(TRUE);
                    nCurrentSpeed = nSpeed;
                    wsprintf(s, "%d", nCurrentSpeed);
                    m_EditSpeed.SetWindowText(s);
            }
            int nTempo = pSndFile->m_nMusicTempo;
            if (nTempo != nCurrentTempo)
            {
                    if (nCurrentTempo < 0) m_SpinTempo.EnableWindow(TRUE);
                    nCurrentTempo = nTempo;
                    wsprintf(s, "%d", nCurrentTempo);
                    m_EditTempo.SetWindowText(s);
            }
            int nRowsPerBeat = pSndFile->m_nCurrentRowsPerBeat;
            if (nRowsPerBeat != nCurrentRowsPerBeat)
            {
                    if (nCurrentRowsPerBeat < 0) m_SpinRowsPerBeat.EnableWindow(TRUE);
                    nCurrentRowsPerBeat = nRowsPerBeat;
                    wsprintf(s, "%d", nCurrentRowsPerBeat);
                    m_EditRowsPerBeat.SetWindowText(s);
            }
    } else
    {
            if (nCurrentTempo != -1)
            {
                    nCurrentTempo = -1;
                    m_EditTempo.SetWindowText("---");
                    m_SpinTempo.EnableWindow(FALSE);
                    SetButtonInfo(PLAYCMD_INDEX, ID_PLAYER_PLAY, TBBS_BUTTON, TOOLBAR_IMAGE_PLAY);
            }
            if (nCurrentSpeed != -1)
            {
                    nCurrentSpeed = -1;
                    m_EditSpeed.SetWindowText("---");
                    m_SpinSpeed.EnableWindow(FALSE);
            }
            if (nCurrentRowsPerBeat != -1)
            {
                    nCurrentRowsPerBeat = -1;
                    m_EditRowsPerBeat.SetWindowText("---");
                    m_SpinRowsPerBeat.EnableWindow(FALSE);
            }
    }
    return TRUE;
}


void CMainToolBar::OnVScroll(UINT nCode, UINT nPos, CScrollBar *pScrollBar)
//-------------------------------------------------------------------------
{
    CMainFrame *pMainFrm;

    CToolBarEx::OnVScroll(nCode, nPos, pScrollBar);
    short int oct = (short int)m_SpinOctave.GetPos();
    if ((oct >= MIN_BASEOCTAVE) && ((int)oct != nCurrentOctave))
    {
            SetBaseOctave(oct);
    }
    if ((nCurrentSpeed < 0) || (nCurrentTempo < 0)) return;
    if ((pMainFrm = CMainFrame::GetMainFrame()) != NULL)
    {
            module_renderer *pSndFile = pMainFrm->GetSoundFilePlaying();
            if (pSndFile)
            {
                    short int n;
                    if ((n = (short int)m_SpinTempo.GetPos()) != 0)
                    {
                            if (n < 0)
                                    pSndFile->SetTempo(bad_max(nCurrentTempo - 1, pSndFile->GetModSpecifications().tempoMin), true);
                            else
                                    pSndFile->SetTempo(bad_min(nCurrentTempo + 1, pSndFile->GetModSpecifications().tempoMax), true);

                            m_SpinTempo.SetPos(0);
                    }
                    if ((n = (short int)m_SpinSpeed.GetPos()) != 0)
                    {
                            if (n < 0)
                            {
                                    pSndFile->m_nMusicSpeed = bad_max(nCurrentSpeed - 1, pSndFile->GetModSpecifications().speedMin);
                            } else
                            {
                                    pSndFile->m_nMusicSpeed = bad_min(nCurrentSpeed + 1, pSndFile->GetModSpecifications().speedMax);
                            }
                            m_SpinSpeed.SetPos(0);
                    }
                    if ((n = (short int)m_SpinRowsPerBeat.GetPos()) != 0)
                    {
                            if (n < 0)
                            {
                                    if (nCurrentRowsPerBeat > 1)
                                    {
                                            SetRowsPerBeat(nCurrentRowsPerBeat - 1);
                                    }
                            } else
                            {
                                    if (nCurrentRowsPerBeat < pSndFile->m_nCurrentRowsPerMeasure)
                                    {
                                            SetRowsPerBeat(nCurrentRowsPerBeat + 1);
                                    }
                            }
                            m_SpinRowsPerBeat.SetPos(0);

                            //update pattern editor

                            CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
                            if (pMainFrm)
                            {
                                    pMainFrm->PostMessage(WM_MOD_INVALIDATEPATTERNS, HINT_MPTOPTIONS);
                            }
                    }

                    SetCurrentSong(pSndFile);
            }
    }
}


void CMainToolBar::SetRowsPerBeat(modplug::tracker::rowindex_t nNewRPB)
//-------------------------------------------------
{
    CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
    if(pMainFrm == nullptr)
            return;
    CModDoc *pModDoc = pMainFrm->GetModPlaying();
    module_renderer *pSndFile = pMainFrm->GetSoundFilePlaying();
    if(pModDoc == nullptr || pSndFile == nullptr)
            return;

    pSndFile->m_nCurrentRowsPerBeat = nNewRPB;
    modplug::tracker::patternindex_t nPat = pSndFile->GetCurrentPattern();
    if(pSndFile->Patterns[nPat].GetOverrideSignature())
    {
            if(nNewRPB <= pSndFile->Patterns[nPat].GetRowsPerMeasure())
            {
                    pSndFile->Patterns[nPat].SetSignature(nNewRPB, pSndFile->Patterns[nPat].GetRowsPerMeasure());
                    pModDoc->SetModified();
            }
    } else
    {
            if(nNewRPB <= pSndFile->m_nDefaultRowsPerMeasure)
            {
                    pSndFile->m_nDefaultRowsPerBeat = nNewRPB;
                    pModDoc->SetModified();
            }
    }
}
