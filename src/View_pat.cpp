#include "stdafx.h"
#include "mainfrm.h"
#include "childfrm.h"
#include "moddoc.h"
#include "PatternEditorDialogs.h"
#include "SampleEditorDialogs.h" // For amplification dialog (which is re-used from sample editor)
#include "globals.h"
#include "view_pat.h"
#include "ctrl_pat.h"
#include "vstplug.h"    // for writing plug params to pattern

#include "view_pat.h"
#include "View_gen.h"
#include "misc_util.h"
#include "legacy_soundlib/midi.h"
#include <cmath>

#include "gui/qt5/pattern_editor.hpp"

using namespace modplug::tracker;
using namespace modplug::pervasives;

#define    PLUGNAME_HEIGHT        16        //rewbs.patPlugName

FindReplaceStruct CViewPattern::m_findReplace = {
    { note_t(0), 0, VolCmdNone, CmdNone, 0, 0 },
    { note_t(0), 0, VolCmdNone, CmdNone, 0, 0 },
    PATSEARCH_FULLSEARCH,
    PATSEARCH_REPLACEALL,
    0,
    0,
    0,
    0,
    0
};

modevent_t CViewPattern::m_cmdOld = { note_t(0), 0, VolCmdNone, CmdNone, 0, 0 };

IMPLEMENT_SERIAL(CViewPattern, CModScrollView, 0)

BEGIN_MESSAGE_MAP(CViewPattern, CModScrollView)
    //{{AFX_MSG_MAP(CViewPattern)
    ON_WM_ERASEBKGND()
    ON_WM_VSCROLL()
    ON_WM_SIZE()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_LBUTTONUP()
    ON_WM_RBUTTONDOWN()
    ON_WM_SETFOCUS()
    ON_WM_KILLFOCUS()
    ON_WM_SYSKEYDOWN()
    ON_WM_DESTROY()
    ON_MESSAGE(WM_MOD_MIDIMSG,                  OnMidiMsg)
    ON_MESSAGE(WM_MOD_RECORDPARAM,              OnRecordPlugParamChange)
    ON_COMMAND(ID_EDIT_CUT,                     OnEditCut)
    ON_COMMAND(ID_EDIT_COPY,                    OnEditCopy)
    ON_COMMAND(ID_EDIT_PASTE,                   OnEditPaste)
    ON_COMMAND(ID_EDIT_MIXPASTE,                OnEditMixPaste)
    ON_COMMAND(ID_EDIT_MIXPASTE_ITSTYLE,        OnEditMixPasteITStyle)
    ON_COMMAND(ID_EDIT_PASTEFLOOD,              OnEditPasteFlood)
    ON_COMMAND(ID_EDIT_PUSHFORWARDPASTE,        OnEditPushForwardPaste)
    ON_COMMAND(ID_EDIT_SELECT_ALL,              OnEditSelectAll)
    ON_COMMAND(ID_EDIT_SELECTCOLUMN,            OnEditSelectColumn)
    ON_COMMAND(ID_EDIT_SELECTCOLUMN2,           OnSelectCurrentColumn)
    ON_COMMAND(ID_EDIT_FIND,                    OnEditFind)
    ON_COMMAND(ID_EDIT_GOTO_MENU,               OnEditGoto)
    ON_COMMAND(ID_EDIT_FINDNEXT,                OnEditFindNext)
    ON_COMMAND(ID_EDIT_RECSELECT,               OnRecordSelect)
    // -> CODE#0012
    // -> DESC="midi keyboard split"
    ON_COMMAND(ID_EDIT_SPLITRECSELECT,          OnSplitRecordSelect)
    ON_COMMAND(ID_EDIT_SPLITKEYBOARDSETTINGS,   SetSplitKeyboardSettings)
    // -! NEW_FEATURE#0012
    ON_COMMAND(ID_EDIT_UNDO,                    OnEditUndo)
    ON_COMMAND(ID_PATTERN_CHNRESET,             OnChannelReset)
    ON_COMMAND(ID_PATTERN_MUTE,                 OnMuteFromClick) //rewbs.customKeys
    ON_COMMAND(ID_PATTERN_SOLO,                 OnSoloFromClick) //rewbs.customKeys
    ON_COMMAND(ID_PATTERN_TRANSITIONMUTE,       OnTogglePendingMuteFromClick)
    ON_COMMAND(ID_PATTERN_TRANSITIONSOLO,       OnPendingSoloChnFromClick)
    ON_COMMAND(ID_PATTERN_TRANSITION_UNMUTEALL, OnPendingUnmuteAllChnFromClick)
    ON_COMMAND(ID_PATTERN_UNMUTEALL,            OnUnmuteAll)
    ON_COMMAND(ID_PATTERN_DELETEROW,            OnDeleteRows)
    ON_COMMAND(ID_PATTERN_DELETEALLROW,         OnDeleteRowsEx)
    ON_COMMAND(ID_PATTERN_INSERTROW,            OnInsertRows)
    ON_COMMAND(ID_NEXTINSTRUMENT,               OnNextInstrument)
    ON_COMMAND(ID_PREVINSTRUMENT,               OnPrevInstrument)
    ON_COMMAND(ID_PATTERN_PLAYROW,              OnPatternStep)
    ON_COMMAND(ID_CONTROLENTER,                 OnPatternStep)
    ON_COMMAND(ID_CONTROLTAB,                   OnSwitchToOrderList)
    ON_COMMAND(ID_PREVORDER,                    OnPrevOrder)
    ON_COMMAND(ID_NEXTORDER,                    OnNextOrder)
    ON_COMMAND(IDC_PATTERN_RECORD,              OnPatternRecord)
    ON_COMMAND(ID_RUN_SCRIPT,                   OnRunScript)
    ON_COMMAND(ID_TRANSPOSE_UP,                 OnTransposeUp)
    ON_COMMAND(ID_TRANSPOSE_DOWN,               OnTransposeDown)
    ON_COMMAND(ID_TRANSPOSE_OCTUP,              OnTransposeOctUp)
    ON_COMMAND(ID_TRANSPOSE_OCTDOWN,            OnTransposeOctDown)
    ON_COMMAND(ID_PATTERN_PROPERTIES,           OnPatternProperties)
    ON_COMMAND(ID_PATTERN_INTERPOLATE_VOLUME,   OnInterpolateVolume)
    ON_COMMAND(ID_PATTERN_INTERPOLATE_EFFECT,   OnInterpolateEffect)
    ON_COMMAND(ID_PATTERN_INTERPOLATE_NOTE,     OnInterpolateNote)
    ON_COMMAND(ID_PATTERN_VISUALIZE_EFFECT,     OnVisualizeEffect) //rewbs.fxvis
    ON_COMMAND(ID_GROW_SELECTION,               OnGrowSelection)
    ON_COMMAND(ID_SHRINK_SELECTION,             OnShrinkSelection)
    ON_COMMAND(ID_PATTERN_SETINSTRUMENT,        OnSetSelInstrument)
    ON_COMMAND(ID_PATTERN_ADDCHANNEL_FRONT,     OnAddChannelFront)
    ON_COMMAND(ID_PATTERN_ADDCHANNEL_AFTER,     OnAddChannelAfter)
    ON_COMMAND(ID_PATTERN_DUPLICATECHANNEL,     OnDuplicateChannel)
    ON_COMMAND(ID_PATTERN_REMOVECHANNEL,        OnRemoveChannel)
    ON_COMMAND(ID_PATTERN_REMOVECHANNELDIALOG,  OnRemoveChannelDialog)
    ON_COMMAND(ID_CURSORCOPY,                   OnCursorCopy)
    ON_COMMAND(ID_CURSORPASTE,                  OnCursorPaste)
    ON_COMMAND(ID_PATTERN_AMPLIFY,              OnPatternAmplify)
    ON_COMMAND(ID_CLEAR_SELECTION,              OnClearSelectionFromMenu)
    ON_COMMAND(ID_SHOWTIMEATROW,                OnShowTimeAtRow)
    ON_COMMAND(ID_CHANNEL_RENAME,               OnRenameChannel)
    ON_COMMAND(ID_PATTERN_EDIT_PCNOTE_PLUGIN,   OnTogglePCNotePluginEditor)
    ON_COMMAND_RANGE(ID_CHANGE_INSTRUMENT,      ID_CHANGE_INSTRUMENT+MAX_INSTRUMENTS, OnSelectInstrument)
    ON_COMMAND_RANGE(ID_CHANGE_PCNOTE_PARAM,    ID_CHANGE_PCNOTE_PARAM + modplug::tracker::modevent_t::MaxColumnValue, OnSelectPCNoteParam)
    ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO,          OnUpdateUndo)
    ON_COMMAND_RANGE(ID_PLUGSELECT,             ID_PLUGSELECT+MAX_MIXPLUGINS, OnSelectPlugin) //rewbs.patPlugName

    //}}AFX_MSG_MAP
    ON_WM_INITMENU()
    ON_WM_RBUTTONDBLCLK()
    ON_WM_RBUTTONUP()
END_MESSAGE_MAP()

static_assert(modplug::tracker::modevent_t::MaxColumnValue <= 999, "Command range for ID_CHANGE_PCNOTE_PARAM is designed for 999");

CViewPattern::CViewPattern()
//--------------------------
{
    m_pEffectVis = NULL; //rewbs.fxvis
    m_bLastNoteEntryBlocked=false;

    m_nMenuOnChan = 0;
    m_nPattern = 0;
    m_nDetailLevel = 4;
    m_pEditWnd = NULL;
    m_pGotoWnd = NULL;
    m_Dib.Init(CMainFrame::bmpNotes);
    UpdateColors();
}

void CViewPattern::OnInitialUpdate()
//----------------------------------
{
    memset(ChnVUMeters, 0, sizeof(ChnVUMeters));
    memset(OldVUMeters, 0, sizeof(OldVUMeters));
// -> CODE#0012
// -> DESC="midi keyboard split"
    memset(splitActiveNoteChannel, 0xFF, sizeof(splitActiveNoteChannel));
    memset(activeNoteChannel, 0xFF, sizeof(activeNoteChannel));
    oldrow = -1;
    oldchn = -1;
    oldsplitchn = -1;
    m_nPlayPat = 0xFFFF;
    m_nPlayRow = 0;
    m_nMidRow = 0;
    m_nDragItem = 0;
    m_bDragging = false;
    m_bInItemRect = false;
    m_bContinueSearch = false;
    m_dwBeginSel = m_dwEndSel = m_dwCursor = m_dwStartSel = m_dwDragPos = 0;
    //m_dwStatus = 0;
    m_dwStatus = PATSTATUS_PLUGNAMESINHEADERS;
    m_nXScroll = m_nYScroll = 0;
    m_nPattern = m_nRow = 0;
    m_nSpacing = 0;
    m_nAccelChar = 0;
// -> CODE#0018
// -> DESC="route PC keyboard inputs to midi in mechanism"
    ignorekey = 0;
// -! BEHAVIOUR_CHANGE#0018
    CScrollView::OnInitialUpdate();
    UpdateSizes();
    UpdateScrollSize();
    SetCurrentPattern(0);
    m_nFoundInstrument = 0;
    m_nLastPlayedRow = 0;
    m_nLastPlayedOrder = 0;
}


BOOL CViewPattern::SetCurrentPattern(UINT npat, int nrow)
//-------------------------------------------------------
{
    module_renderer *pSndFile;
    CModDoc *pModDoc = GetDocument();
    bool bUpdateScroll = false;
    pSndFile = pModDoc ? pModDoc->GetSoundFile() : NULL;

    if ( (!pModDoc) || (!pSndFile) || (npat >= pSndFile->Patterns.Size()) ) return FALSE;
    if ((m_pEditWnd) && (m_pEditWnd->IsWindowVisible())) m_pEditWnd->ShowWindow(SW_HIDE);

    if ((npat + 1 < pSndFile->Patterns.Size()) && (!pSndFile->Patterns[npat])) npat = 0;
    while ((npat > 0) && (!pSndFile->Patterns[npat])) npat--;
    if (!pSndFile->Patterns[npat])
    {
        // Changed behaviour here. Previously, an empty pattern was inserted and the user most likely didn't notice that. Now, we just return an error.
        //pSndFile->Patterns.Insert(npat, 64);
        return FALSE;
    }

    m_nPattern = npat;
    if ((nrow >= 0) && (nrow != (int)m_nRow) && (nrow < (int)pSndFile->Patterns[m_nPattern].GetNumRows()))
    {
        m_nRow = nrow;
        bUpdateScroll = true;
    }
    if (m_nRow >= pSndFile->Patterns[m_nPattern].GetNumRows()) m_nRow = 0;
    int sel = CreateCursor(m_nRow) | m_dwCursor;
    SetCurSel(sel, sel);
    UpdateSizes();
    UpdateScrollSize();
    UpdateIndicator();

    if (m_bWholePatternFitsOnScreen)             //rewbs.scrollFix
        SetScrollPos(SB_VERT, 0);
    else if (bUpdateScroll) //rewbs.fix3147
        SetScrollPos(SB_VERT, m_nRow * GetColumnHeight());

    UpdateScrollPos();
    InvalidatePattern(TRUE);
    SendCtrlMessage(CTRLMSG_PATTERNCHANGED, m_nPattern);

    return TRUE;
}


// This should be used instead of consecutive calls to SetCurrentRow() then SetCurrentColumn()
BOOL CViewPattern::SetCursorPosition(UINT nrow, UINT ncol, BOOL bWrap)
//--------------------------------------------------------------------------
{
    // Set row, but do not update scroll position yet
    // as there is another position update on the way:
    SetCurrentRow(nrow, bWrap, false);
    // Now set column and update scroll position:
    SetCurrentColumn(ncol);
    return TRUE;
}


BOOL CViewPattern::SetCurrentRow(UINT row, BOOL bWrap, BOOL bUpdateHorizontalScrollbar)
//-------------------------------------------------------------------------------------
{
    module_renderer *pSndFile;
    CModDoc *pModDoc = GetDocument();
    if (!pModDoc) return FALSE;
    pSndFile = pModDoc->GetSoundFile();

    if ((bWrap) && (pSndFile->Patterns[m_nPattern].GetNumRows()))
    {
        if ((int)row < (int)0)
        {
            if (m_dwStatus & (PATSTATUS_KEYDRAGSEL|PATSTATUS_MOUSEDRAGSEL))
            {
                row = 0;
            } else
            if (CMainFrame::m_dwPatternSetup & PATTERN_CONTSCROLL)
            {
                UINT nCurOrder = SendCtrlMessage(CTRLMSG_GETCURRENTORDER);
                if ((nCurOrder > 0) && (nCurOrder < unwrap(pSndFile->Order.size())) && (m_nPattern == pSndFile->Order[nCurOrder]))
                {
                    const modplug::tracker::orderindex_t prevOrd = pSndFile->Order.GetPreviousOrderIgnoringSkips(nCurOrder);
                    const modplug::tracker::patternindex_t nPrevPat = pSndFile->Order[prevOrd];
                    if ((nPrevPat < pSndFile->Patterns.Size()) && (pSndFile->Patterns[nPrevPat].GetNumRows()))
                    {
                        SendCtrlMessage(CTRLMSG_SETCURRENTORDER, prevOrd);
                        if (SetCurrentPattern(nPrevPat))
                            return SetCurrentRow(pSndFile->Patterns[nPrevPat].GetNumRows() + (int)row);
                    }
                }
                row = 0;
            } else
            if (CMainFrame::m_dwPatternSetup & PATTERN_WRAP)
            {
                if ((int)row < (int)0) row += pSndFile->Patterns[m_nPattern].GetNumRows();
                row %= pSndFile->Patterns[m_nPattern].GetNumRows();
            }
        } else //row >= 0
        if (row >= pSndFile->Patterns[m_nPattern].GetNumRows())
        {
            if (m_dwStatus & (PATSTATUS_KEYDRAGSEL|PATSTATUS_MOUSEDRAGSEL))
            {
                row = pSndFile->Patterns[m_nPattern].GetNumRows()-1;
            } else
            if (CMainFrame::m_dwPatternSetup & PATTERN_CONTSCROLL)
            {
                UINT nCurOrder = SendCtrlMessage(CTRLMSG_GETCURRENTORDER);
                if ((nCurOrder+1 < pSndFile->Order.size()) && (m_nPattern == pSndFile->Order[nCurOrder]))
                {
                    const modplug::tracker::orderindex_t nextOrder = pSndFile->Order.GetNextOrderIgnoringSkips(nCurOrder);
                    const modplug::tracker::patternindex_t nextPat = pSndFile->Order[nextOrder];
                    if ((nextPat < pSndFile->Patterns.Size()) && (pSndFile->Patterns[nextPat].GetNumRows()))
                    {
                        const modplug::tracker::rowindex_t newRow = row - pSndFile->Patterns[m_nPattern].GetNumRows();
                        SendCtrlMessage(CTRLMSG_SETCURRENTORDER, nextOrder);
                        if (SetCurrentPattern(nextPat))
                            return SetCurrentRow(newRow);
                    }
                }
                row = pSndFile->Patterns[m_nPattern].GetNumRows()-1;
            } else
            if (CMainFrame::m_dwPatternSetup & PATTERN_WRAP)
            {
                row %= pSndFile->Patterns[m_nPattern].GetNumRows();
            }
        }
    }

    //rewbs.fix3168
    if ( (static_cast<int>(row)<0) && !(CMainFrame::m_dwPatternSetup & PATTERN_CONTSCROLL))
        row = 0;
    if (row >= pSndFile->Patterns[m_nPattern].GetNumRows() && !(CMainFrame::m_dwPatternSetup & PATTERN_CONTSCROLL))
        row = pSndFile->Patterns[m_nPattern].GetNumRows()-1;
    //end rewbs.fix3168

    if ((row >= pSndFile->Patterns[m_nPattern].GetNumRows()) || (!m_szCell.cy)) return FALSE;
    // Fix: If cursor isn't on screen move both scrollbars to make it visible
    InvalidateRow();
    m_nRow = row;
    // Fix: Horizontal scrollbar pos screwed when selecting with mouse
    UpdateScrollbarPositions(bUpdateHorizontalScrollbar);
    InvalidateRow();
    int sel = CreateCursor(m_nRow) | m_dwCursor;
    int sel0 = sel;
    if ((m_dwStatus & (PATSTATUS_KEYDRAGSEL|PATSTATUS_MOUSEDRAGSEL))
     && (!(m_dwStatus & PATSTATUS_DRAGNDROPEDIT))) sel0 = m_dwStartSel;
    SetCurSel(sel0, sel);
    UpdateIndicator();

    //XXXih: turkish
    /*
    Log("Row: %d; Chan: %d; ColType: %d; cursor&0xFFFF: %x; cursor>>16: %x;\n",
        GetRowFromCursor(sel0),
        GetChanFromCursor(sel0),
        GetColTypeFromCursor(sel0),
        (int)(sel0&0xFFFF),
        (int)(sel0>>16));
        */


    return TRUE;
}


BOOL CViewPattern::SetCurrentColumn(UINT ncol)
//--------------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    module_renderer *pSndFile;
    if (!pModDoc) return FALSE;
    pSndFile = pModDoc->GetSoundFile();
    if ((ncol & 0x07) == 7)
    {
        ncol = (ncol & ~0x07) + m_nDetailLevel;
    } else
    if ((ncol & 0x07) > m_nDetailLevel)
    {
        ncol += (((ncol & 0x07) - m_nDetailLevel) << 3) - (m_nDetailLevel+1);
    }
    if ((ncol >> 3) >= pSndFile->GetNumChannels()) return FALSE;
    m_dwCursor = ncol;
    int sel = CreateCursor(m_nRow) | m_dwCursor;
    int sel0 = sel;
    if ((m_dwStatus & (PATSTATUS_KEYDRAGSEL|PATSTATUS_MOUSEDRAGSEL))
     && (!(m_dwStatus & PATSTATUS_DRAGNDROPEDIT))) sel0 = m_dwStartSel;
    SetCurSel(sel0, sel);
    // Fix: If cursor isn't on screen move both scrollbars to make it visible
    UpdateScrollbarPositions();
    UpdateIndicator();
    return TRUE;
}

// Fix: If cursor isn't on screen move scrollbars to make it visible
// Fix: save pattern scrollbar position when switching to other tab
// Assume that m_nRow and m_dwCursor are valid
// When we switching to other tab the CViewPattern object is deleted
// and when switching back new one is created
BOOL CViewPattern::UpdateScrollbarPositions( BOOL UpdateHorizontalScrollbar )
//---------------------------------------------------------------------------
{
// HACK - after new CViewPattern object created SetCurrentRow() and SetCurrentColumn() are called -
// just skip first two calls of UpdateScrollbarPositions() if pModDoc->GetOldPatternScrollbarsPos() is valid
    CModDoc *pModDoc = GetDocument();
    if (pModDoc)
    {
        CSize scroll = pModDoc->GetOldPatternScrollbarsPos();
        if( scroll.cx >= 0 )
        {
            OnScrollBy( scroll );
            scroll.cx = -1;
            pModDoc->SetOldPatternScrollbarsPos( scroll );
            return TRUE;
        } else
        if( scroll.cx >= -1 )
        {
            scroll.cx = -2;
            pModDoc->SetOldPatternScrollbarsPos( scroll );
            return TRUE;
        }
    }
    CSize scroll(0,0);
    UINT row = GetCurrentRow();
    UINT yofs = GetYScrollPos();
    CRect rect;
    GetClientRect(&rect);
    rect.top += m_szHeader.cy;
    int numrows = (rect.bottom - rect.top - 1) / m_szCell.cy;
    if (numrows < 1) numrows = 1;
    if (m_nMidRow)
    {
        if (row != yofs)
        {
            scroll.cy = (int)(row - yofs) * m_szCell.cy;
        }
    } else
    {
        if (row < yofs)
        {
            scroll.cy = (int)(row - yofs) * m_szCell.cy;
        } else
        if (row > yofs + (UINT)numrows - 1)
        {
            scroll.cy = (int)(row - yofs - numrows + 1) * m_szCell.cy;
        }
    }

    if( UpdateHorizontalScrollbar )
    {
        UINT ncol = GetCurrentColumn();
        UINT xofs = GetXScrollPos();
        int nchn = ncol >> 3;
        if (nchn < xofs)
        {
            scroll.cx = (int)(nchn - xofs) * m_szCell.cx;
        } else
        if (nchn > xofs)
        {
            int maxcol = (rect.right - m_szHeader.cx) / m_szCell.cx;
            if ((nchn >= (xofs+maxcol)) && (maxcol >= 0))
            {
                scroll.cx = (int)(nchn - xofs - maxcol + 1) * m_szCell.cx;
            }
        }
    }
    if( scroll.cx != 0 || scroll.cy != 0 )
    {
        OnScrollBy(scroll, TRUE);
    }
    return TRUE;
}

uint32_t CViewPattern::GetDragItem(CPoint point, LPRECT lpRect)
//----------------------------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    module_renderer *pSndFile;
    CRect rcClient, rect, plugRect;     //rewbs.patPlugNames
    UINT n;
    int xofs, yofs;

    if (!pModDoc) return 0;
    GetClientRect(&rcClient);
    xofs = GetXScrollPos();
    yofs = GetYScrollPos();
    rect.SetRect(m_szHeader.cx, 0, m_szHeader.cx + GetColumnWidth() /*- 2*/, m_szHeader.cy);
    plugRect.SetRect(m_szHeader.cx, m_szHeader.cy-PLUGNAME_HEIGHT, m_szHeader.cx + GetColumnWidth() - 2, m_szHeader.cy);    //rewbs.patPlugNames
    pSndFile = pModDoc->GetSoundFile();
    const UINT nmax = pSndFile->GetNumChannels();
    // Checking channel headers
    //rewbs.patPlugNames
    if (m_dwStatus & PATSTATUS_PLUGNAMESINHEADERS)
    {
        n = xofs;
        for (UINT ihdr=0; n<nmax; ihdr++, n++)
        {
            if (plugRect.PtInRect(point))
            {
                if (lpRect) *lpRect = plugRect;
                return (DRAGITEM_PLUGNAME | n);
            }
            plugRect.OffsetRect(GetColumnWidth(), 0);
        }
    }
    //end rewbs.patPlugNames
    n = xofs;
    for (UINT ihdr=0; n<nmax; ihdr++, n++)
    {
        if (rect.PtInRect(point))
        {
            if (lpRect) *lpRect = rect;
            return (DRAGITEM_CHNHEADER | n);
        }
        rect.OffsetRect(GetColumnWidth(), 0);
    }
    if ((pSndFile->Patterns[m_nPattern]) && (pSndFile->m_nType & (MOD_TYPE_XM|MOD_TYPE_IT|MOD_TYPE_MPT)))
    {
        rect.SetRect(0, 0, m_szHeader.cx, m_szHeader.cy);
        if (rect.PtInRect(point))
        {
            if (lpRect) *lpRect = rect;
            return DRAGITEM_PATTERNHEADER;
        }
    }
    return 0;
}


// Drag a selection to position dwPos.
// If bScroll is true, the point dwPos is scrolled into the view if needed.
// If bNoMode if specified, the original selection points are not altered.
// Note that scrolling will only be executed if dwPos contains legal coordinates.
// This can be useful when selecting a whole row and specifying 0xFFFF as the end channel.
BOOL CViewPattern::DragToSel(uint32_t dwPos, BOOL bScroll, BOOL bNoMove)
//-------------------------------------------------------------------
{
    CRect rect;
    module_renderer *pSndFile;
    CModDoc *pModDoc = GetDocument();
    int yofs = GetYScrollPos(), xofs = GetXScrollPos();
    int row, col;

    if ((!pModDoc) || (!m_szCell.cy)) return FALSE;
    GetClientRect(&rect);
    pSndFile = pModDoc->GetSoundFile();
    if (!bNoMove) SetCurSel(m_dwStartSel, dwPos);
    if (!bScroll) return TRUE;
    // Scroll to row
    row = GetRowFromCursor(dwPos);
    if (row < (int)pSndFile->Patterns[m_nPattern].GetNumRows())
    {
        row += m_nMidRow;
        rect.top += m_szHeader.cy;
        int numrows = (rect.bottom - rect.top - 1) / m_szCell.cy;
        if (numrows < 1) numrows = 1;
        if (row < yofs)
        {
            CSize sz;
            sz.cx = 0;
            sz.cy = (int)(row - yofs) * m_szCell.cy;
            OnScrollBy(sz, TRUE);
        } else
        if (row > yofs + numrows - 1)
        {
            CSize sz;
            sz.cx = 0;
            sz.cy = (int)(row - yofs - numrows + 1) * m_szCell.cy;
            OnScrollBy(sz, TRUE);
        }
    }
    // Scroll to column
    col = GetChanFromCursor(dwPos);
    if (col < (int)pSndFile->m_nChannels)
    {
        int maxcol = (rect.right - m_szHeader.cx) - 4;
        maxcol -= GetColumnOffset(GetColTypeFromCursor(dwPos));
        maxcol /= GetColumnWidth();
        if (col < xofs)
        {
            CSize size(-m_szCell.cx,0);
            if (!bNoMove) size.cx = (col - xofs) * (int)m_szCell.cx;
            OnScrollBy(size, TRUE);
        } else
        if ((col > xofs+maxcol) && (maxcol > 0))
        {
            CSize size(m_szCell.cx,0);
            if (!bNoMove) size.cx = (col - maxcol + 1) * (int)m_szCell.cx;
            OnScrollBy(size, TRUE);
        }
    }
    UpdateWindow();
    return TRUE;
}


BOOL CViewPattern::SetPlayCursor(UINT nPat, UINT nRow)
//----------------------------------------------------
{
    UINT nOldPat = m_nPlayPat, nOldRow = m_nPlayRow;
    if ((nPat == m_nPlayPat) && (nRow == m_nPlayRow)) return TRUE;
    m_nPlayPat = nPat;
    m_nPlayRow = nRow;
    if (nOldPat == m_nPattern) InvalidateRow(nOldRow);
    if (m_nPlayPat == m_nPattern) InvalidateRow(m_nPlayRow);
    return TRUE;
}


UINT CViewPattern::GetCurrentInstrument() const
//---------------------------------------------
{
    return SendCtrlMessage(CTRLMSG_GETCURRENTINSTRUMENT);
}

BOOL CViewPattern::ShowEditWindow()
//---------------------------------
{
    if (!m_pEditWnd)
    {
        m_pEditWnd = new CEditCommand;
        if (m_pEditWnd) m_pEditWnd->SetParent(this, GetDocument());
    }
    if (m_pEditWnd)
    {
        m_pEditWnd->ShowEditWindow(m_nPattern, CreateCursor(m_nRow) | m_dwCursor);
        return TRUE;
    }
    return FALSE;
}


BOOL CViewPattern::PrepareUndo(uint32_t dwBegin, uint32_t dwEnd)
//--------------------------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    UINT nChnBeg, nRowBeg, nChnEnd, nRowEnd;

    nChnBeg = GetChanFromCursor(dwBegin);
    nRowBeg = GetRowFromCursor(dwBegin);
    nChnEnd = GetChanFromCursor(dwEnd);
    nRowEnd = GetRowFromCursor(dwEnd);
    if( (nChnEnd < nChnBeg) || (nRowEnd < nRowBeg) ) return FALSE;
    //if (pModDoc) return pModDoc->GetPatternUndo()->PrepareUndo(m_nPattern, nChnBeg, nRowBeg, nChnEnd-nChnBeg+1, nRowEnd-nRowBeg+1);
    return FALSE;
}


////////////////////////////////////////////////////////////////////////
// CViewPattern message handlers

void CViewPattern::OnDestroy()
//----------------------------
{
// Fix: save pattern scrollbar position when switching to other tab
// When we switching to other tab the CViewPattern object is deleted
    CModDoc *pModDoc = GetDocument();
    if (pModDoc)
    {
        pModDoc->SetOldPatternScrollbarsPos(CSize(m_nXScroll*m_szCell.cx, m_nYScroll*m_szCell.cy));
    };

    if (m_pEditWnd)    {
        m_pEditWnd->DestroyWindow();
        delete m_pEditWnd;
        m_pEditWnd = NULL;
    }

    CModScrollView::OnDestroy();
}


void CViewPattern::OnSetFocus(CWnd *pOldWnd)
//------------------------------------------
{
    CScrollView::OnSetFocus(pOldWnd);
    m_dwStatus |= PATSTATUS_FOCUS;
    InvalidateRow();
    CModDoc *pModDoc = GetDocument();
    if (pModDoc)
    {
        pModDoc->SetFollowWnd(m_hWnd, MPTNOTIFY_POSITION|MPTNOTIFY_VUMETERS);
        UpdateIndicator();
    }
}


void CViewPattern::OnKillFocus(CWnd *pNewWnd)
//-------------------------------------------
{
    CScrollView::OnKillFocus(pNewWnd);

    //rewbs.customKeys
    //Unset all selection
    m_dwStatus &= ~PATSTATUS_KEYDRAGSEL;
    m_dwStatus &= ~PATSTATUS_CTRLDRAGSEL;
//    CMainFrame::GetMainFrame()->GetInputHandler()->SetModifierMask(0);
    //end rewbs.customKeys

    m_dwStatus &= ~PATSTATUS_FOCUS;
    InvalidateRow();
}

//rewbs.customKeys
void CViewPattern::OnGrowSelection()
//-----------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if (!pModDoc) return;
    module_renderer *pSndFile = pModDoc->GetSoundFile();
    modplug::tracker::modevent_t *p = pSndFile->Patterns[m_nPattern];
    if (!p) return;
    BeginWaitCursor();

    uint32_t startSel = ((m_dwBeginSel>>16)<(m_dwEndSel>>16)) ? m_dwBeginSel : m_dwEndSel;
    uint32_t endSel   = ((m_dwBeginSel>>16)<(m_dwEndSel>>16)) ? m_dwEndSel : m_dwBeginSel;
    //pModDoc->GetPatternUndo()->PrepareUndo(m_nPattern, 0, 0, pSndFile->m_nChannels, pSndFile->Patterns[m_nPattern].GetNumRows());

    int finalDest = GetRowFromCursor(startSel) + (GetRowFromCursor(endSel) - GetRowFromCursor(startSel))*2;
    for (int row = finalDest; row > (int)GetRowFromCursor(startSel); row -= 2)
    {
        int offset = row - GetRowFromCursor(startSel);
        for (UINT i=(startSel & 0xFFFF); i<=(endSel & 0xFFFF); i++) if (GetColTypeFromCursor(i) <= LAST_COLUMN)
        {
            UINT chn = GetChanFromCursor(i);
            if ((chn >= pSndFile->GetNumChannels()) || (row >= pSndFile->Patterns[m_nPattern].GetNumRows())) continue;
            modplug::tracker::modevent_t *dest = &p[row * pSndFile->GetNumChannels() + chn];
            modplug::tracker::modevent_t *src  = &p[(row-offset/2) * pSndFile->GetNumChannels() + chn];
            modplug::tracker::modevent_t *blank= &p[(row-1) * pSndFile->GetNumChannels() + chn];
            //memcpy(dest/*+(i%5)*/, src/*+(i%5)*/, /*sizeof(modplug::tracker::modcommand_t) - (i-chn)*/ sizeof(uint8_t));
            //Log("dst: %d; src: %d; blk: %d\n", row, (row-offset/2), (row-1));
            switch(GetColTypeFromCursor(i))
            {
                case NOTE_COLUMN:    dest->note    = src->note;    blank->note = NoteNone;                break;
                case INST_COLUMN:    dest->instr   = src->instr;   blank->instr = 0;                                break;
                case VOL_COLUMN:    dest->vol     = src->vol;     blank->vol = 0;
                                    dest->volcmd  = src->volcmd;  blank->volcmd = VolCmdNone;    break;
                case EFFECT_COLUMN:    dest->command = src->command; blank->command = CmdNone;                        break;
                case PARAM_COLUMN:    dest->param   = src->param;   blank->param = CmdNone;                break;
            }
        }
    }

    m_dwBeginSel = startSel;
    m_dwEndSel   = (bad_min(finalDest,pSndFile->Patterns[m_nPattern].GetNumRows()-1)<<16) | (endSel&0xFFFF);

    InvalidatePattern(FALSE);
    pModDoc->SetModified();
    pModDoc->UpdateAllViews(this, HINT_PATTERNDATA | (m_nPattern << HINT_SHIFT_PAT), NULL);
    EndWaitCursor();
    SetFocus();
}


void CViewPattern::OnShrinkSelection()
//-----------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if (!pModDoc) return;
    module_renderer *pSndFile = pModDoc->GetSoundFile();
    modplug::tracker::modevent_t *p = pSndFile->Patterns[m_nPattern];
    if (!p) return;
    BeginWaitCursor();

    uint32_t startSel = ((m_dwBeginSel>>16)<(m_dwEndSel>>16)) ? m_dwBeginSel : m_dwEndSel;
    uint32_t endSel   = ((m_dwBeginSel>>16)<(m_dwEndSel>>16)) ? m_dwEndSel : m_dwBeginSel;
    //pModDoc->GetPatternUndo()->PrepareUndo(m_nPattern, 0, 0, pSndFile->m_nChannels, pSndFile->Patterns[m_nPattern].GetNumRows());

    int finalDest = GetRowFromCursor(startSel) + (GetRowFromCursor(endSel) - GetRowFromCursor(startSel))/2;

    for (int row = GetRowFromCursor(startSel); row <= finalDest; row++)
    {
        int offset = row - GetRowFromCursor(startSel);
        int srcRow = GetRowFromCursor(startSel) + (offset * 2);

        for (UINT i = (startSel & 0xFFFF); i <= (endSel & 0xFFFF); i++) if (GetColTypeFromCursor(i) <= LAST_COLUMN)
        {
            const modplug::tracker::chnindex_t chn = GetChanFromCursor(i);
            if ((chn >= pSndFile->GetNumChannels()) || (srcRow >= pSndFile->Patterns[m_nPattern].GetNumRows())
                                               || (row    >= pSndFile->Patterns[m_nPattern].GetNumRows())) continue;
            modplug::tracker::modevent_t *dest = pSndFile->Patterns[m_nPattern].GetpModCommand(row, chn);
            modplug::tracker::modevent_t *src = pSndFile->Patterns[m_nPattern].GetpModCommand(srcRow, chn);
            // if source command is empty, try next source row.
            if(srcRow < pSndFile->Patterns[m_nPattern].GetNumRows() - 1)
            {
                const modplug::tracker::modevent_t *srcNext = pSndFile->Patterns[m_nPattern].GetpModCommand(srcRow + 1, chn);
                if(src->note == NoteNone) src->note = srcNext->note;
                if(src->instr == 0) src->instr = srcNext->instr;
                if(src->volcmd == VolCmdNone)
                {
                    src->volcmd = srcNext->volcmd;
                    src->vol = srcNext->vol;
                }
                if(src->command == CmdNone)
                {
                    src->command = srcNext->command;
                    src->param = srcNext->param;
                }
            }

            //memcpy(dest/*+(i%5)*/, src/*+(i%5)*/, /*sizeof(modplug::tracker::modcommand_t) - (i-chn)*/ sizeof(uint8_t));
            Log("dst: %d; src: %d\n", row, srcRow);
            switch(GetColTypeFromCursor(i))
            {
                case NOTE_COLUMN:    dest->note    = src->note;    break;
                case INST_COLUMN:    dest->instr   = src->instr;   break;
                case VOL_COLUMN:    dest->vol     = src->vol;
                                    dest->volcmd  = src->volcmd;  break;
                case EFFECT_COLUMN:    dest->command = src->command; break;
                case PARAM_COLUMN:    dest->param   = src->param;   break;
            }
        }
    }
    for (int row = finalDest + 1; row <= GetRowFromCursor(endSel); row++)
    {
        for (UINT i = (startSel & 0xFFFF); i <= (endSel & 0xFFFF); i++) if (GetColTypeFromCursor(i) <= LAST_COLUMN)
        {
            UINT chn = GetChanFromCursor(i);
            modplug::tracker::modevent_t *dest = pSndFile->Patterns[m_nPattern].GetpModCommand(row, chn);
            switch(GetColTypeFromCursor(i))
            {
                case NOTE_COLUMN:    dest->note    = NoteNone;                break;
                case INST_COLUMN:    dest->instr   = 0;                                break;
                case VOL_COLUMN:    dest->vol     = 0;
                                    dest->volcmd  = VolCmdNone;    break;
                case EFFECT_COLUMN:    dest->command = CmdNone;                break;
                case PARAM_COLUMN:    dest->param   = 0;                                break;
            }
        }
    }
    m_dwBeginSel = startSel;
    m_dwEndSel   = (bad_min(finalDest,pSndFile->Patterns[m_nPattern].GetNumRows()-1)<<16) | (endSel& 0xFFFF);

    InvalidatePattern(FALSE);
    pModDoc->SetModified();
    pModDoc->UpdateAllViews(this, HINT_PATTERNDATA | (m_nPattern << HINT_SHIFT_PAT), NULL);
    EndWaitCursor();
    SetFocus();
}
//rewbs.customKeys

void CViewPattern::OnClearSelectionFromMenu()
//-------------------------------------------
{
    OnClearSelection();
}

void CViewPattern::OnClearSelection(bool ITStyle, RowMask rm) //Default RowMask: all elements enabled
//-----------------------------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if (!pModDoc || !(IsEditingEnabled_bmsg())) return;
    module_renderer *pSndFile = pModDoc->GetSoundFile();
    modplug::tracker::modevent_t *p = pSndFile->Patterns[m_nPattern];
    if (!p) return;

    BeginWaitCursor();

    if(ITStyle && GetColTypeFromCursor(m_dwEndSel) == NOTE_COLUMN) m_dwEndSel += 1;
    //If selection ends to a note column, in ITStyle extending it to instrument column since the instrument data is
    //removed with note data.

    PrepareUndo(m_dwBeginSel, m_dwEndSel);
    uint32_t tmp = m_dwEndSel;
    bool invalidateAllCols = false;

    for (UINT row = GetRowFromCursor(m_dwBeginSel); row <= GetRowFromCursor(m_dwEndSel); row++) { // for all selected rows
        for (UINT i=(m_dwBeginSel & 0xFFFF); i<=(m_dwEndSel & 0xFFFF); i++) if (GetColTypeFromCursor(i) <= LAST_COLUMN) { // for all selected cols

            UINT chn = GetChanFromCursor(i);
            if ((chn >= pSndFile->m_nChannels) || (row >= pSndFile->Patterns[m_nPattern].GetNumRows())) continue;
            modplug::tracker::modevent_t *m = &p[row * pSndFile->m_nChannels + chn];
            switch(GetColTypeFromCursor(i))
            {
            case NOTE_COLUMN:    // Clear note
                if (rm.note)
                {
                    if(m->IsPcNote())
                    {  // Clear whole cell if clearing PC note
                        m->Clear();
                        invalidateAllCols = true;
                    }
                    else
                    {
                        m->note = NoteNone;
                        if (ITStyle) m->instr = 0;
                    }
                }
                break;
            case INST_COLUMN: // Clear instrument
                if (rm.instrument)
                    m->instr = 0;
                break;
            case VOL_COLUMN:    // Clear volume
                if (rm.volume)
                    m->volcmd = VolCmdNone;
                    m->vol = 0;
                break;
            case EFFECT_COLUMN: // Clear Command
                if (rm.command)
                {
                    m->command = CmdNone;
                    if(m->IsPcNote())
                    {
                        m->SetValueEffectCol(0);
                        invalidateAllCols = true;
                    }
                }
                break;
            case PARAM_COLUMN:    // Clear Command Param
                if (rm.parameter)
                {
                    m->param = 0;
                    if(m->IsPcNote())
                    {
                        m->SetValueEffectCol(0);
                        // first column => update effect column char
                        if(i == (m_dwBeginSel & 0xFFFF))
                        {
                            m_dwBeginSel--;
                        }
                    }
                }
                break;
            } //end switch
        } //end selected columns
    } //end selected rows

    // If selection ends on effect command column, extent invalidation area to
    // effect param column as well.
    if (GetColTypeFromCursor(tmp) == EFFECT_COLUMN)
        tmp++;

    // If invalidation on all columns is wanted, extent invalidation area.
    if(invalidateAllCols)
        tmp += LAST_COLUMN - GetColTypeFromCursor(tmp);

    InvalidateArea(m_dwBeginSel, tmp);
    pModDoc->SetModified();
    pModDoc->UpdateAllViews(this, HINT_PATTERNDATA | (m_nPattern << HINT_SHIFT_PAT), NULL);
    EndWaitCursor();
    SetFocus();
}


void CViewPattern::OnEditCut()
//----------------------------
{
    OnEditCopy();
    OnClearSelection(false);
}


void CViewPattern::OnEditCopy()
//-----------------------------
{
    CModDoc *pModDoc = GetDocument();

    if (pModDoc)
    {
        pModDoc->CopyPattern(m_nPattern, m_dwBeginSel, m_dwEndSel);
        SetFocus();
    }
}


void CViewPattern::OnLButtonDown(UINT nFlags, CPoint point)
//---------------------------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if (/*(m_bDragging) ||*/ (pModDoc == nullptr) || (pModDoc->GetSoundFile() == nullptr)) return;
    SetFocus();
    m_nDropItem = m_nDragItem = GetDragItem(point, &m_rcDragItem);
    m_bDragging = true;
    m_bInItemRect = true;
    m_bShiftDragging = false;

    SetCapture();
    if ((point.x >= m_szHeader.cx) && (point.y <= m_szHeader.cy))
    {
        if (nFlags & MK_CONTROL)
        {
            TogglePendingMute(GetChanFromCursor(GetPositionFromPoint(point)));
        }
    }
    else if ((point.x >= m_szHeader.cx) && (point.y > m_szHeader.cy))
    {
        // Set first selection point
        m_dwStartSel = GetPositionFromPoint(point);
        if (GetChanFromCursor(m_dwStartSel) < pModDoc->GetNumChannels())
        {
            m_dwStatus |= PATSTATUS_MOUSEDRAGSEL;

            if (m_dwStatus & PATSTATUS_CTRLDRAGSEL)
            {
                SetCurSel(m_dwStartSel, m_dwStartSel);
            }
            if ((CMainFrame::m_dwPatternSetup & PATTERN_DRAGNDROPEDIT)
            && ((m_dwBeginSel != m_dwEndSel) || (m_dwStatus & PATSTATUS_CTRLDRAGSEL))
            && (GetRowFromCursor(m_dwStartSel) >= GetRowFromCursor(m_dwBeginSel))
            && (GetRowFromCursor(m_dwStartSel) <= GetRowFromCursor(m_dwEndSel))
            && ((m_dwStartSel & 0xFFFF) >= (m_dwBeginSel & 0xFFFF))
            && ((m_dwStartSel & 0xFFFF) <= (m_dwEndSel & 0xFFFF)))
            {
                m_dwStatus |= PATSTATUS_DRAGNDROPEDIT;
            } else
            if (CMainFrame::m_dwPatternSetup & PATTERN_CENTERROW)
            {
                SetCurSel(m_dwStartSel, m_dwStartSel);
            } else
            {
                // Fix: Horizontal scrollbar pos screwed when selecting with mouse
                SetCursorPosition( GetRowFromCursor(m_dwStartSel), m_dwStartSel & 0xFFFF );
            }
        }
    } else if ((point.x < m_szHeader.cx) && (point.y > m_szHeader.cy))
    {
        // Mark row number => mark whole row (start)
        InvalidateSelection();
        uint32_t dwPoint = GetPositionFromPoint(point);
        if(GetRowFromCursor(dwPoint) < pModDoc->GetSoundFile()->Patterns[m_nPattern].GetNumRows())
        {
            m_dwBeginSel = m_dwStartSel = dwPoint;
            m_dwEndSel = m_dwBeginSel | 0xFFFF;
            SetCurSel(m_dwStartSel, m_dwEndSel);
            m_dwStatus |= PATSTATUS_SELECTROW;
        }
    }

    if (m_nDragItem)
    {
        InvalidateRect(&m_rcDragItem, FALSE);
        UpdateWindow();
    }
}


void CViewPattern::OnLButtonDblClk(UINT uFlags, CPoint point)
//-----------------------------------------------------------
{
    uint32_t dwPos = GetPositionFromPoint(point);
    if ((dwPos == (CreateCursor(m_nRow) | m_dwCursor)) && (point.y >= m_szHeader.cy))
    {
        if (ShowEditWindow()) return;
    }
    OnLButtonDown(uFlags, point);
}


void CViewPattern::OnLButtonUp(UINT nFlags, CPoint point)
//-------------------------------------------------------
{
    CModDoc *pModDoc = GetDocument();

    const bool bItemSelected = m_bInItemRect || ((m_nDragItem & DRAGITEM_MASK) == DRAGITEM_CHNHEADER);
    if (/*(!m_bDragging) ||*/ (!pModDoc)) return;

    m_bDragging = false;
    m_bInItemRect = false;
    ReleaseCapture();
    m_dwStatus &= ~(PATSTATUS_MOUSEDRAGSEL|PATSTATUS_SELECTROW);
    // Drag & Drop Editing
    if (m_dwStatus & PATSTATUS_DRAGNDROPEDIT)
    {
        if (m_dwStatus & PATSTATUS_DRAGNDROPPING)
        {
            OnDrawDragSel();
            m_dwStatus &= ~PATSTATUS_DRAGNDROPPING;
            OnDropSelection();
        }
        uint32_t dwPos = GetPositionFromPoint(point);
        if (dwPos == m_dwStartSel)
        {
            SetCurSel(dwPos, dwPos);
        }
        SetCursor(CMainFrame::curArrow);
        m_dwStatus &= ~PATSTATUS_DRAGNDROPEDIT;
    }
    if (((m_nDragItem & DRAGITEM_MASK) != DRAGITEM_CHNHEADER)
     && ((m_nDragItem & DRAGITEM_MASK) != DRAGITEM_PATTERNHEADER)
     && ((m_nDragItem & DRAGITEM_MASK) != DRAGITEM_PLUGNAME))
    {
        if ((m_nMidRow) && (m_dwBeginSel == m_dwEndSel))
        {
            uint32_t dwPos = m_dwBeginSel;
            // Fix: Horizontal scrollbar pos screwed when selecting with mouse
            SetCursorPosition( GetRowFromCursor(dwPos), dwPos & 0xFFFF );
            //UpdateIndicator();
        }
    }
    if ((!bItemSelected) || (!m_nDragItem)) return;
    InvalidateRect(&m_rcDragItem, FALSE);
    const modplug::tracker::chnindex_t nItemNo = (m_nDragItem & DRAGITEM_VALUEMASK);
    const modplug::tracker::chnindex_t nTargetNo = (m_nDropItem != 0) ? (m_nDropItem & DRAGITEM_VALUEMASK) : ChannelIndexInvalid;

    switch(m_nDragItem & DRAGITEM_MASK)
    {
    case DRAGITEM_CHNHEADER:
        if(nItemNo == nTargetNo)
        {
            // Just clicked a channel header...

            if (nFlags & MK_SHIFT)
            {
                if (nItemNo < MAX_BASECHANNELS)
                {
                    pModDoc->Record1Channel(nItemNo);
                    InvalidateChannelsHeaders();
                }
            }
            else if (!(nFlags & MK_CONTROL))
            {
                pModDoc->MuteChannel(nItemNo, !pModDoc->IsChannelMuted(nItemNo));
                pModDoc->UpdateAllViews(this, HINT_MODCHANNELS | ((nItemNo / CHANNELS_IN_TAB) << HINT_SHIFT_CHNTAB));
            }
        } else if(nTargetNo < pModDoc->GetNumChannels() && (m_nDropItem & DRAGITEM_MASK) == DRAGITEM_CHNHEADER)
        {
            // Dragged to other channel header => move or copy channel

            InvalidateRect(&m_rcDropItem, FALSE);

            const bool duplicate = (nFlags & MK_SHIFT) ? true : false;
            const modplug::tracker::chnindex_t newChannels = pModDoc->GetNumChannels() + (duplicate ? 1 : 0);
            vector<modplug::tracker::chnindex_t> channels(newChannels, 0);
            modplug::tracker::chnindex_t i = 0;
            bool modified = duplicate;

            for(modplug::tracker::chnindex_t nChn = 0; nChn < newChannels; nChn++)
            {
                if(nChn == nTargetNo)
                {
                    channels[nChn] = nItemNo;
                }
                else
                {
                    if(i == nItemNo && !duplicate)    // Don't want that source channel twice if we're just moving
                    {
                        i++;
                    }
                    channels[nChn] = i++;
                }
                if(channels[nChn] != nChn)
                {
                    modified = true;
                }
            }
            if(modified && pModDoc->ReArrangeChannels(channels) != ChannelIndexInvalid)
            {
                if(duplicate)
                {
                    pModDoc->UpdateAllViews(this, HINT_MODCHANNELS);
                    pModDoc->UpdateAllViews(this, HINT_MODTYPE);
                    SetCurrentPattern(m_nPattern);
                }
                InvalidatePattern();
                pModDoc->SetModified();
            }
        }
        break;
    case DRAGITEM_PATTERNHEADER:
        OnPatternProperties();
        break;
    case DRAGITEM_PLUGNAME:                    //rewbs.patPlugNames
        if (nItemNo < MAX_BASECHANNELS)
            TogglePluginEditor(nItemNo);
        break;
    }

    m_nDropItem = 0;
}


void CViewPattern::OnPatternProperties()
//--------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if (pModDoc && pModDoc->GetSoundFile() && pModDoc->GetSoundFile()->Patterns.IsValidPat(m_nPattern))
    {
        CPatternPropertiesDlg dlg(pModDoc, m_nPattern, this);
        if (dlg.DoModal())
        {
            UpdateScrollSize();
            InvalidatePattern(TRUE);
        }
    }
}


void CViewPattern::OnRButtonDown(UINT, CPoint pt)
//-----------------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    module_renderer *pSndFile;
    HMENU hMenu;

    // Too far left to get a ctx menu:
    if ((!pModDoc) || (pt.x < m_szHeader.cx)) {
        return;
    }

    // Handle drag n drop
    if (m_dwStatus & PATSTATUS_DRAGNDROPEDIT)    {
        if (m_dwStatus & PATSTATUS_DRAGNDROPPING)
        {
            OnDrawDragSel();
            m_dwStatus &= ~PATSTATUS_DRAGNDROPPING;
        }
        m_dwStatus &= ~(PATSTATUS_DRAGNDROPEDIT|PATSTATUS_MOUSEDRAGSEL);
        if (m_bDragging)
        {
            m_bDragging = false;
            m_bInItemRect = false;
            ReleaseCapture();
        }
        SetCursor(CMainFrame::curArrow);
        return;
    }

    if ((hMenu = ::CreatePopupMenu()) == NULL)
    {
        return;
    }

    pSndFile = pModDoc->GetSoundFile();
    m_nMenuParam = GetPositionFromPoint(pt);

    // Right-click outside selection? Reposition cursor to the new location
    if ((GetRowFromCursor(m_nMenuParam) < GetRowFromCursor(m_dwBeginSel))
     || (GetRowFromCursor(m_nMenuParam) > GetRowFromCursor(m_dwEndSel))
     || ((m_nMenuParam & 0xFFFF) < (m_dwBeginSel & 0xFFFF))
     || ((m_nMenuParam & 0xFFFF) > (m_dwEndSel & 0xFFFF)))
    {
        if (pt.y > m_szHeader.cy) { //ensure we're not clicking header
            // Fix: Horizontal scrollbar pos screwed when selecting with mouse
            SetCursorPosition( GetRowFromCursor(m_nMenuParam), m_nMenuParam & 0xFFFF );
        }
    }
    const modplug::tracker::chnindex_t nChn = GetChanFromCursor(m_nMenuParam);
    if ((nChn < pSndFile->GetNumChannels()) && (pSndFile->Patterns[m_nPattern]))
    {
        CString MenuText;

        //------ Plugin Header Menu --------- :
        if ((m_dwStatus & PATSTATUS_PLUGNAMESINHEADERS) &&
            (pt.y > m_szHeader.cy-PLUGNAME_HEIGHT) && (pt.y < m_szHeader.cy)) {
            BuildPluginCtxMenu(hMenu, nChn, pSndFile);
        }

        //------ Channel Header Menu ---------- :
        else if (pt.y < m_szHeader.cy){
            if (BuildSoloMuteCtxMenu(hMenu, nChn, pSndFile))
                AppendMenu(hMenu, MF_SEPARATOR, 0, "");
            BuildRecordCtxMenu(hMenu, nChn, pModDoc);
            BuildChannelControlCtxMenu(hMenu);
            BuildChannelMiscCtxMenu(hMenu, pSndFile);
        }

        //------ Standard Menu ---------- :
        else if ((pt.x >= m_szHeader.cx) && (pt.y >= m_szHeader.cy))    {
            /*if (BuildSoloMuteCtxMenu(hMenu, ih, nChn, pSndFile))
                AppendMenu(hMenu, MF_SEPARATOR, 0, "");*/
            if (BuildSelectionCtxMenu(hMenu))
                AppendMenu(hMenu, MF_SEPARATOR, 0, "");
            if (BuildEditCtxMenu(hMenu, pModDoc))
                AppendMenu(hMenu, MF_SEPARATOR, 0, "");
            if (BuildNoteInterpolationCtxMenu(hMenu, pSndFile) |    //Use bitwise ORs to avoid shortcuts
                BuildVolColInterpolationCtxMenu(hMenu, pSndFile) |
                BuildEffectInterpolationCtxMenu(hMenu, pSndFile) )
                AppendMenu(hMenu, MF_SEPARATOR, 0, "");
            if (BuildTransposeCtxMenu(hMenu))
                AppendMenu(hMenu, MF_SEPARATOR, 0, "");
            if (BuildVisFXCtxMenu(hMenu)   |     //Use bitwise ORs to avoid shortcuts
                BuildAmplifyCtxMenu(hMenu) |
                BuildSetInstCtxMenu(hMenu, pSndFile) )
                AppendMenu(hMenu, MF_SEPARATOR, 0, "");
            if (BuildPCNoteCtxMenu(hMenu, pSndFile))
                AppendMenu(hMenu, MF_SEPARATOR, 0, "");
            if (BuildGrowShrinkCtxMenu(hMenu))
                AppendMenu(hMenu, MF_SEPARATOR, 0, "");
            if(BuildMiscCtxMenu(hMenu))
                AppendMenu(hMenu, MF_SEPARATOR, 0, "");
            BuildRowInsDelCtxMenu(hMenu);

        }

        ClientToScreen(&pt);
        ::TrackPopupMenu(hMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hWnd, NULL);
    }
    ::DestroyMenu(hMenu);
}

void CViewPattern::OnRButtonUp(UINT nFlags, CPoint point)
//-------------------------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    //BOOL bItemSelected = m_bInItemRect;
    if (!pModDoc) return;

    //CSoundFile *pSndFile = pModDoc->GetSoundFile();
    m_nDragItem = GetDragItem(point, &m_rcDragItem);
    uint32_t nItemNo = m_nDragItem & DRAGITEM_VALUEMASK;
    switch(m_nDragItem & DRAGITEM_MASK)
    {
    case DRAGITEM_CHNHEADER:
        if (nFlags & MK_SHIFT)
        {
            if (nItemNo < MAX_BASECHANNELS)
            {
                pModDoc->Record2Channel(nItemNo);
                InvalidateChannelsHeaders();
            }
        }
        break;
    }

    CModScrollView::OnRButtonUp(nFlags, point);
}


void CViewPattern::OnMouseMove(UINT nFlags, CPoint point)
//-------------------------------------------------------
{
    if (!m_bDragging) return;

    if (m_nDragItem)
    {
        const CRect oldDropRect = m_rcDropItem;
        const uint32_t oldDropItem = m_nDropItem;

        m_bShiftDragging = (nFlags & MK_SHIFT) ? true : false;
        m_nDropItem = GetDragItem(point, &m_rcDropItem);

        const bool b = (m_nDropItem == m_nDragItem) ? true : false;
        const bool dragChannel = (m_nDragItem & DRAGITEM_MASK) == DRAGITEM_CHNHEADER;

        if (b != m_bInItemRect || (m_nDropItem != oldDropItem && dragChannel))
        {
            m_bInItemRect = b;
            InvalidateRect(&m_rcDragItem, FALSE);

            // Dragging around channel headers? Update move indicator...
            if((m_nDropItem & DRAGITEM_MASK) == DRAGITEM_CHNHEADER) InvalidateRect(&m_rcDropItem, FALSE);
            if((oldDropItem & DRAGITEM_MASK) == DRAGITEM_CHNHEADER) InvalidateRect(&oldDropRect, FALSE);

            UpdateWindow();
        }
    }

    if ((m_dwStatus & PATSTATUS_SELECTROW) /*&& (point.x < m_szHeader.cx)*/ && (point.y > m_szHeader.cy))
    {
        // Mark row number => mark whole row (continue)
        InvalidateSelection();
        m_dwEndSel = GetPositionFromPoint(point) | 0xFFFF;
        DragToSel(m_dwEndSel, TRUE, FALSE);
    } else if (m_dwStatus & PATSTATUS_MOUSEDRAGSEL)
    {
        CModDoc *pModDoc = GetDocument();
        uint32_t dwPos = GetPositionFromPoint(point);
        if ((pModDoc) && (m_nPattern < pModDoc->GetSoundFile()->Patterns.Size()))
        {
            UINT row = GetRowFromCursor(dwPos);
            UINT bad_max = pModDoc->GetSoundFile()->Patterns[m_nPattern].GetNumRows();
            if ((row) && (row >= bad_max)) row = bad_max-1;
            dwPos &= 0xFFFF;
            dwPos |= CreateCursor(row);
        }
        // Drag & Drop editing
        if (m_dwStatus & PATSTATUS_DRAGNDROPEDIT)
        {
            if (!(m_dwStatus & PATSTATUS_DRAGNDROPPING)) SetCursor(CMainFrame::curDragging);
            if ((!(m_dwStatus & PATSTATUS_DRAGNDROPPING)) || ((m_dwDragPos & ~7) != (dwPos & ~7)))
            {
                if (m_dwStatus & PATSTATUS_DRAGNDROPPING) OnDrawDragSel();
                m_dwStatus &= ~PATSTATUS_DRAGNDROPPING;
                DragToSel(dwPos, TRUE, TRUE);
                m_dwDragPos = dwPos;
                m_dwStatus |= PATSTATUS_DRAGNDROPPING;
                OnDrawDragSel();
                pModDoc->SetModified();
            }
        } else
        // Default: selection
        if (CMainFrame::m_dwPatternSetup & PATTERN_CENTERROW)
        {
            DragToSel(dwPos, TRUE);
        } else
        {
            // Fix: Horizontal scrollbar pos screwed when selecting with mouse
            SetCursorPosition( GetRowFromCursor(dwPos), dwPos & 0xFFFF );
        }
    }
}


void CViewPattern::OnEditSelectAll()
//----------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if (pModDoc)
    {
        module_renderer *pSndFile = pModDoc->GetSoundFile();
        SetCurSel(CreateCursor(0), CreateCursor(pSndFile->Patterns[m_nPattern].GetNumRows() - 1, pSndFile->GetNumChannels() - 1, LAST_COLUMN));
    }
}


void CViewPattern::OnEditSelectColumn()
//-------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if (pModDoc)
    {
        module_renderer *pSndFile = pModDoc->GetSoundFile();
        SetCurSel(CreateCursor(0, GetChanFromCursor(m_nMenuParam), 0), CreateCursor(pSndFile->Patterns[m_nPattern].GetNumRows() - 1, GetChanFromCursor(m_nMenuParam), LAST_COLUMN));
    }
}


void CViewPattern::OnSelectCurrentColumn()
//----------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if (pModDoc)
    {
        module_renderer *pSndFile = pModDoc->GetSoundFile();
        uint32_t dwBeginSel = CreateCursor(0, GetChanFromCursor(m_dwCursor), 0);
        uint32_t dwEndSel = CreateCursor(pSndFile->Patterns[m_nPattern].GetNumRows() - 1, GetChanFromCursor(m_dwCursor), LAST_COLUMN);
        // If column is already selected, select the current pattern
        if ((dwBeginSel == m_dwBeginSel) && (dwEndSel == m_dwEndSel))
        {
            dwBeginSel = CreateCursor(0);
            dwEndSel = CreateCursor(pSndFile->Patterns[m_nPattern].GetNumRows() - 1, pSndFile->GetNumChannels() - 1, LAST_COLUMN);
        }
        SetCurSel(dwBeginSel, dwEndSel);
    }
}

void CViewPattern::OnChannelReset()
//---------------------------------
{
    const modplug::tracker::chnindex_t nChn = GetChanFromCursor(m_nMenuParam);
    CModDoc *pModDoc = GetDocument();
    module_renderer* pSndFile;
    if (pModDoc == 0 || (pSndFile = pModDoc->GetSoundFile()) == 0) return;

    const bool bIsMuted = pModDoc->IsChannelMuted(nChn);
    if(!bIsMuted) pModDoc->MuteChannel(nChn, true);
    pSndFile->ResetChannelState(nChn, CHNRESET_TOTAL);
    if(!bIsMuted) pModDoc->MuteChannel(nChn, false);
}


void CViewPattern::OnMuteFromClick()
//----------------------------------
{
    OnMuteChannel(false);
}

void CViewPattern::OnMuteChannel(bool current)
//--------------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if (pModDoc)
    {
        const modplug::tracker::chnindex_t nChn = GetChanFromCursor(current ? m_dwCursor : m_nMenuParam);
        pModDoc->SoloChannel(nChn, false); //rewbs.merge: recover old solo/mute behaviour
        pModDoc->MuteChannel(nChn, !pModDoc->IsChannelMuted(nChn));

        //If we just unmuted a channel, make sure none are still considered "solo".
        if(!pModDoc->IsChannelMuted(nChn))
        {
            for(modplug::tracker::chnindex_t i = 0; i < pModDoc->GetNumChannels(); i++)
            {
                pModDoc->SoloChannel(i, false);
            }
        }

        InvalidateChannelsHeaders();
    }
}

void CViewPattern::OnSoloFromClick()
{
    OnSoloChannel(false);
}


// When trying to solo a channel that is already the only unmuted channel,
// this will result in unmuting all channels, in order to satisfy user habits.
// In all other cases, soloing a channel unsoloes all and mutes all except this channel
void CViewPattern::OnSoloChannel(bool current)
//--------------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if (pModDoc == nullptr) return;

    const modplug::tracker::chnindex_t nChn = GetChanFromCursor(current ? m_dwCursor : m_nMenuParam);
    if (nChn >= pModDoc->GetNumChannels())
    {
        return;
    }

    if (pModDoc->IsChannelSolo(nChn))
    {
        bool nChnIsOnlyUnMutedChan=true;
        for (modplug::tracker::chnindex_t i = 0; i < pModDoc->GetNumChannels(); i++)    //check status of all other chans
        {
            if (i != nChn && !pModDoc->IsChannelMuted(i))
            {
                nChnIsOnlyUnMutedChan=false;    //found a channel that isn't muted!
                break;
            }
        }
        if (nChnIsOnlyUnMutedChan)    // this is the only playable channel and it is already soloed ->  uunMute all
        {
            OnUnmuteAll();
            return;
        }
    }
    for(modplug::tracker::chnindex_t i = 0; i < pModDoc->GetNumChannels(); i++)
    {
        pModDoc->MuteChannel(i, !(i == nChn)); //mute all chans except nChn, unmute nChn
        pModDoc->SoloChannel(i, (i == nChn));  //unsolo all chans except nChn, solo nChn
    }
    InvalidateChannelsHeaders();
}


void CViewPattern::OnRecordSelect()
//---------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if (pModDoc)
    {
        UINT nNumChn = pModDoc->GetNumChannels();
        UINT nChn = GetChanFromCursor(m_nMenuParam);
        if (nChn < nNumChn)
        {
//                    MultiRecordMask[nChn>>3] ^= (1 << (nChn & 7));
// -> CODE#0012
// -> DESC="midi keyboard split"
            pModDoc->Record1Channel(nChn);
// -! NEW_FEATURE#0012
            InvalidateChannelsHeaders();
        }
    }
}

// -> CODE#0012
// -> DESC="midi keyboard split"
void CViewPattern::OnSplitRecordSelect()
{
    CModDoc *pModDoc = GetDocument();
    if (pModDoc){
        UINT nNumChn = pModDoc->GetNumChannels();
        UINT nChn = GetChanFromCursor(m_nMenuParam);
        if (nChn < nNumChn){
            pModDoc->Record2Channel(nChn);
            InvalidateChannelsHeaders();
        }
    }
}
// -! NEW_FEATURE#0012


void CViewPattern::OnUnmuteAll()
//------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if (pModDoc)
    {
        const modplug::tracker::chnindex_t nChns = pModDoc->GetNumChannels();
        for(modplug::tracker::chnindex_t i = 0; i < nChns; i++)
        {
            pModDoc->MuteChannel(i, false);
            pModDoc->SoloChannel(i, false); //rewbs.merge: binary solo/mute behaviour
        }
        InvalidateChannelsHeaders();
    }
}


void CViewPattern::DeleteRows(UINT colmin, UINT colmax, UINT nrows)
//-----------------------------------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    module_renderer *pSndFile;
    UINT row, maxrow;

    if (!pModDoc || !( IsEditingEnabled_bmsg() )) return;
    pSndFile = pModDoc->GetSoundFile();
    if (!pSndFile->Patterns[m_nPattern]) return;
    row = GetRowFromCursor(m_dwBeginSel);
    maxrow = pSndFile->Patterns[m_nPattern].GetNumRows();
    if (colmax >= pSndFile->m_nChannels) colmax = pSndFile->m_nChannels-1;
    if (colmin > colmax) return;
    //pModDoc->GetPatternUndo()->PrepareUndo(m_nPattern, 0,0, pSndFile->m_nChannels, maxrow);
    for (UINT r=row; r<maxrow; r++)
    {
        modplug::tracker::modevent_t *m = pSndFile->Patterns[m_nPattern] + r * pSndFile->m_nChannels + colmin;
        for (UINT c=colmin; c<=colmax; c++, m++)
        {
            if (r + nrows >= maxrow)
            {
                m->Clear();
            } else
            {
                *m = *(m + pSndFile->m_nChannels * nrows);
            }
        }
    }
    //rewbs.customKeys
    uint32_t finalPos = CreateCursor(GetSelectionStartRow()) | (m_dwEndSel & 0xFFFF);
    SetCurSel(finalPos, finalPos);
    // Fix: Horizontal scrollbar pos screwed when selecting with mouse
    SetCursorPosition( GetRowFromCursor(finalPos), finalPos & 0xFFFF );
    //end rewbs.customKeys

    pModDoc->SetModified();
    pModDoc->UpdateAllViews(this, HINT_PATTERNDATA | (m_nPattern << HINT_SHIFT_PAT), NULL);
    InvalidatePattern(FALSE);
}


void CViewPattern::OnDeleteRows()
//-------------------------------
{
    UINT colmin = GetChanFromCursor(m_dwBeginSel);
    UINT colmax = GetChanFromCursor(m_dwEndSel);
    UINT nrows = GetRowFromCursor(m_dwEndSel) - GetRowFromCursor(m_dwBeginSel) + 1;
    DeleteRows(colmin, colmax, nrows);
    //m_dwEndSel = (m_dwEndSel & 0x0000FFFF) | (m_dwBeginSel & 0xFFFF0000);
}


void CViewPattern::OnDeleteRowsEx()
//---------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if (!pModDoc) return;
    module_renderer *pSndFile = pModDoc->GetSoundFile();
    UINT nrows = GetRowFromCursor(m_dwEndSel) - GetRowFromCursor(m_dwBeginSel) + 1;
    DeleteRows(0, pSndFile->GetNumChannels() - 1, nrows);
    m_dwEndSel = (m_dwEndSel & 0x0000FFFF) | (m_dwBeginSel & 0xFFFF0000);
}

void CViewPattern::InsertRows(UINT colmin, UINT colmax) //rewbs.customKeys: added args
//-----------------------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    module_renderer *pSndFile;
    UINT row, maxrow;

    if (!pModDoc || !(IsEditingEnabled_bmsg())) return;
    pSndFile = pModDoc->GetSoundFile();
    if (!pSndFile->Patterns[m_nPattern]) return;
    row = GetRowFromCursor(m_dwBeginSel);
    maxrow = pSndFile->Patterns[m_nPattern].GetNumRows();
    if (colmax >= pSndFile->GetNumChannels()) colmax = pSndFile->GetNumChannels() - 1;
    if (colmin > colmax) return;
    //pModDoc->GetPatternUndo()->PrepareUndo(m_nPattern, 0,0, pSndFile->GetNumChannels(), maxrow);

    for (UINT r=maxrow; r>row; )
    {
        r--;
        modplug::tracker::modevent_t *m = pSndFile->Patterns[m_nPattern] + r * pSndFile->GetNumChannels() + colmin;
        for (UINT c=colmin; c<=colmax; c++, m++)
        {
            if (r <= row)
            {
                m->Clear();
            } else
            {
                *m = *(m - pSndFile->GetNumChannels());
            }
        }
    }
    InvalidatePattern(FALSE);
    pModDoc->SetModified();
    pModDoc->UpdateAllViews(this, HINT_PATTERNDATA | (m_nPattern << HINT_SHIFT_PAT), NULL);
}


//rewbs.customKeys
void CViewPattern::OnInsertRows()
//-------------------------------
{
    CModDoc *pModDoc = GetDocument();
    module_renderer *pSndFile;
    UINT colmin, colmax, row, maxrow;

    if (!pModDoc) return;
    pSndFile = pModDoc->GetSoundFile();
    if (!pSndFile->Patterns[m_nPattern]) return;
    row = m_nRow;
    maxrow = pSndFile->Patterns[m_nPattern].GetNumRows();
    colmin = GetChanFromCursor(m_dwBeginSel);
    colmax = GetChanFromCursor(m_dwEndSel);

    InsertRows(colmin, colmax);
}
//end rewbs.customKeys

void CViewPattern::OnEditFind() { }

void CViewPattern::OnEditGoto() { }

void CViewPattern::OnEditFindNext()
//---------------------------------
{
    CHAR s[512], szFind[64];
    UINT nFound = 0, nPatStart, nPatEnd;
    CModDoc *pModDoc = GetDocument();
    module_renderer *pSndFile;
    BOOL bEffectEx;

    if (!pModDoc) return;
    if (!(m_findReplace.dwFindFlags & ~PATSEARCH_FULLSEARCH))
    {
        PostMessage(WM_COMMAND, ID_EDIT_FIND);
        return;
    }
    BeginWaitCursor();
    pSndFile = pModDoc->GetSoundFile();
    nPatStart = m_nPattern;
    nPatEnd = m_nPattern+1;
    if (m_findReplace.dwFindFlags & PATSEARCH_FULLSEARCH)
    {
        nPatStart = 0;
        nPatEnd = pSndFile->Patterns.Size();
    } else if(m_findReplace.dwFindFlags & PATSEARCH_PATSELECTION)
    {
        nPatStart = m_nPattern;
        nPatEnd = nPatStart + 1;
    }
    if (m_bContinueSearch)
    {
        nPatStart = m_nPattern;
    }
    bEffectEx = FALSE;
    if (m_findReplace.dwFindFlags & PATSEARCH_COMMAND)
    {
        UINT fxndx = pModDoc->GetIndexFromEffect(m_findReplace.cmdFind.command, m_findReplace.cmdFind.param);
        bEffectEx = pModDoc->IsExtendedEffect(fxndx);
    }
    for (UINT nPat=nPatStart; nPat<nPatEnd; nPat++)
    {
        modplug::tracker::modevent_t *m = pSndFile->Patterns[nPat];
        uint32_t len = pSndFile->m_nChannels * pSndFile->Patterns[nPat].GetNumRows();
        if ((!m) || (!len)) continue;
        UINT n = 0;
        if ((m_bContinueSearch) && (nPat == nPatStart) && (nPat == m_nPattern))
        {
            n = GetCurrentRow() * pSndFile->m_nChannels + GetCurrentChannel() + 1;
            m += n;
        }
        for (; n<len; n++, m++)
        {
            bool bFound = true, bReplace = true;

            if (m_findReplace.dwFindFlags & PATSEARCH_CHANNEL)
            {
                // limit to given channels
                const modplug::tracker::chnindex_t ch = n % pSndFile->m_nChannels;
                if ((ch < m_findReplace.nFindMinChn) || (ch > m_findReplace.nFindMaxChn)) bFound = false;
            }
            if (m_findReplace.dwFindFlags & PATSEARCH_PATSELECTION)
            {
                // limit to pattern selection
                const modplug::tracker::chnindex_t ch = n % pSndFile->m_nChannels;
                const modplug::tracker::rowindex_t row = n / pSndFile->m_nChannels;

                if ((ch < GetChanFromCursor(m_findReplace.dwBeginSel)) || (ch > GetChanFromCursor(m_findReplace.dwEndSel))) bFound = false;
                if ((row < GetRowFromCursor(m_findReplace.dwBeginSel)) || (row > GetRowFromCursor(m_findReplace.dwEndSel))) bFound = false;
            }
            if (((m_findReplace.dwFindFlags & PATSEARCH_NOTE) && ((m->note != m_findReplace.cmdFind.note) && ((m_findReplace.cmdFind.note != CFindReplaceTab::findAny) || (!m->note) || (m->note & 0x80))))
             || ((m_findReplace.dwFindFlags & PATSEARCH_INSTR) && (m->instr != m_findReplace.cmdFind.instr))
             || ((m_findReplace.dwFindFlags & PATSEARCH_VOLCMD) && (m->volcmd != m_findReplace.cmdFind.volcmd))
             || ((m_findReplace.dwFindFlags & PATSEARCH_VOLUME) && (m->vol != m_findReplace.cmdFind.vol))
             || ((m_findReplace.dwFindFlags & PATSEARCH_COMMAND) && (m->command != m_findReplace.cmdFind.command))
             || ((m_findReplace.dwFindFlags & PATSEARCH_PARAM) && (m->param != m_findReplace.cmdFind.param)))
            {
                bFound = false;
            }
            else
            {
                if (((m_findReplace.dwFindFlags & (PATSEARCH_COMMAND|PATSEARCH_PARAM)) == PATSEARCH_COMMAND) && (bEffectEx))
                {
                    if ((m->param & 0xF0) != (m_findReplace.cmdFind.param & 0xF0)) bFound = false;
                }

                // Ignore modcommands with PC/PCS notes when searching from volume or effect column.
                if( (m->IsPcNote())
                    &&
                    m_findReplace.dwFindFlags & (PATSEARCH_VOLCMD|PATSEARCH_VOLUME|PATSEARCH_COMMAND|PATSEARCH_PARAM))
                {
                    bFound = false;
                }
            }
            // Found!
            if (bFound)
            {
                bool bUpdPos = true;
                if ((m_findReplace.dwReplaceFlags & (PATSEARCH_REPLACEALL|PATSEARCH_REPLACE)) == (PATSEARCH_REPLACEALL|PATSEARCH_REPLACE)) bUpdPos = false;
                nFound++;

                if (bUpdPos)
                {
                    // turn off "follow song"
                    m_dwStatus &= ~PATSTATUS_FOLLOWSONG;
                    SendCtrlMessage(CTRLMSG_PAT_FOLLOWSONG, 0);
                    // go to place of finding
                    SetCurrentPattern(nPat);
                    SetCurrentRow(n / pSndFile->m_nChannels);
                }

                UINT ncurs = (n % pSndFile->m_nChannels) << 3;
                if (!(m_findReplace.dwFindFlags & PATSEARCH_NOTE))
                {
                    ncurs++;
                    if (!(m_findReplace.dwFindFlags & PATSEARCH_INSTR))
                    {
                        ncurs++;
                        if (!(m_findReplace.dwFindFlags & (PATSEARCH_VOLCMD|PATSEARCH_VOLUME)))
                        {
                            ncurs++;
                            if (!(m_findReplace.dwFindFlags & PATSEARCH_COMMAND)) ncurs++;
                        }
                    }
                }
                if (bUpdPos)
                {
                    SetCurrentColumn(ncurs);
                }
                if (!(m_findReplace.dwReplaceFlags & PATSEARCH_REPLACE)) goto EndSearch;
                if (!(m_findReplace.dwReplaceFlags & PATSEARCH_REPLACEALL))
                {
                    UINT ans = MessageBox("Replace this occurrence?", "Replace", MB_YESNOCANCEL);
                    if (ans == IDYES) bReplace = true; else
                    if (ans == IDNO) bReplace = false; else goto EndSearch;
                }
                if (bReplace)
                {
                    // Just create one logic undo step when auto-replacing all occurences.
                    const bool linkUndoBuffer = (nFound > 1) && (m_findReplace.dwReplaceFlags & PATSEARCH_REPLACEALL) != 0;
                    //pModDoc->GetPatternUndo()->PrepareUndo(nPat, n % pSndFile->m_nChannels, n / pSndFile->m_nChannels, 1, 1, linkUndoBuffer);

                    if ((m_findReplace.dwReplaceFlags & PATSEARCH_NOTE))
                    {
                        // -1 octave
                        if (m_findReplace.cmdReplace.note == CFindReplaceTab::replaceNoteMinusOctave)
                        {
                            if (m->note > 12) m->note -= 12;
                        } else
                        // +1 octave
                        if (m_findReplace.cmdReplace.note == CFindReplaceTab::replaceNotePlusOctave)
                        {
                            if (m->note <= NoteMax - 12) m->note += 12;
                        } else
                        // Note--
                        if (m_findReplace.cmdReplace.note == CFindReplaceTab::replaceNoteMinusOne)
                        {
                            if (m->note > 1) m->note--;
                        } else
                        // Note++
                        if (m_findReplace.cmdReplace.note == CFindReplaceTab::replaceNotePlusOne)
                        {
                            if (m->note < NoteMax) m->note++;
                        } else
                        // Replace with another note
                        {
                            // If we're going to remove a PC Note or replace a normal note by a PC note, wipe out the complete column.
                            if(m->IsPcNote() != modplug::tracker::modevent_t::IsPcNote(m_findReplace.cmdReplace.note))
                            {
                                m->Clear();
                            }
                            m->note = m_findReplace.cmdReplace.note;
                        }
                    }
                    if ((m_findReplace.dwReplaceFlags & PATSEARCH_INSTR))
                    {
                        // Instr--
                        if (m_findReplace.cInstrRelChange == -1 && m->instr > 1)
                            m->instr--;
                        // Instr++
                        else if (m_findReplace.cInstrRelChange == 1 && m->instr > 0 && m->instr < (MAX_INSTRUMENTS - 1))
                            m->instr++;
                        else m->instr = m_findReplace.cmdReplace.instr;
                    }
                    if ((m_findReplace.dwReplaceFlags & PATSEARCH_VOLCMD))
                    {
                        m->volcmd = m_findReplace.cmdReplace.volcmd;
                    }
                    if ((m_findReplace.dwReplaceFlags & PATSEARCH_VOLUME))
                    {
                        m->vol = m_findReplace.cmdReplace.vol;
                    }
                    if ((m_findReplace.dwReplaceFlags & PATSEARCH_COMMAND))
                    {
                        m->command = m_findReplace.cmdReplace.command;
                    }
                    if ((m_findReplace.dwReplaceFlags & PATSEARCH_PARAM))
                    {
                        if ((bEffectEx) && (!(m_findReplace.dwReplaceFlags & PATSEARCH_COMMAND)))
                            m->param = (uint8_t)((m->param & 0xF0) | (m_findReplace.cmdReplace.param & 0x0F));
                        else
                            m->param = m_findReplace.cmdReplace.param;
                    }
                    pModDoc->SetModified();
                    if (bUpdPos) InvalidateRow();
                }
            }
        }
    }
EndSearch:
    if( m_findReplace.dwFindFlags & PATSEARCH_PATSELECTION)
    {
        // Restore original selection
        m_dwBeginSel = m_findReplace.dwBeginSel;
        m_dwEndSel = m_findReplace.dwEndSel;
        InvalidatePattern();
    } else if (m_findReplace.dwReplaceFlags & PATSEARCH_REPLACEALL)
    {
        InvalidatePattern();
    }
    m_bContinueSearch = true;
    EndWaitCursor();
    // Display search results
    //m_findReplace.dwReplaceFlags &= ~PATSEARCH_REPLACEALL;
    if (!nFound)
    {
        if (m_findReplace.dwFindFlags & PATSEARCH_NOTE)
        {
            wsprintf(szFind, "%s", GetNoteStr(m_findReplace.cmdFind.note));
        } else strcpy(szFind, "???");
        strcat(szFind, " ");
        if (m_findReplace.dwFindFlags & PATSEARCH_INSTR)
        {
            if (m_findReplace.cmdFind.instr)
                wsprintf(&szFind[strlen(szFind)], "%03d", m_findReplace.cmdFind.instr);
            else
                strcat(szFind, " ..");
        } else strcat(szFind, " ??");
        strcat(szFind, " ");
        if (m_findReplace.dwFindFlags & PATSEARCH_VOLCMD)
        {
            if (m_findReplace.cmdFind.volcmd)
                wsprintf(&szFind[strlen(szFind)], "%c", vol_command_glyphs[m_findReplace.cmdFind.volcmd]);
            else
                strcat(szFind, ".");
        } else strcat(szFind, "?");
        if (m_findReplace.dwFindFlags & PATSEARCH_VOLUME)
        {
            wsprintf(&szFind[strlen(szFind)], "%02d", m_findReplace.cmdFind.vol);
        } else strcat(szFind, "??");
        strcat(szFind, " ");
        if (m_findReplace.dwFindFlags & PATSEARCH_COMMAND)
        {
            if (m_findReplace.cmdFind.command)
            {
                if (pSndFile->m_nType & (MOD_TYPE_S3M|MOD_TYPE_IT|MOD_TYPE_MPT))
                    wsprintf(&szFind[strlen(szFind)], "%c", s3m_command_glyphs[m_findReplace.cmdFind.command]);
                else
                    wsprintf(&szFind[strlen(szFind)], "%c", mod_command_glyphs[m_findReplace.cmdFind.command]);
            } else
                strcat(szFind, ".");
        } else strcat(szFind, "?");
        if (m_findReplace.dwFindFlags & PATSEARCH_PARAM)
        {
            wsprintf(&szFind[strlen(szFind)], "%02X", m_findReplace.cmdFind.param);
        } else strcat(szFind, "??");
        wsprintf(s, "Cannot find \"%s\"", szFind);
        MessageBox(s, "Find/Replace", MB_OK | MB_ICONINFORMATION);
    }
}


void CViewPattern::OnPatternStep()
//--------------------------------
{
    PatternStep(true);
}


void CViewPattern::PatternStep(bool autoStep)
//-------------------------------------------
{
    CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
    CModDoc *pModDoc = GetDocument();

    if ((pMainFrm) && (pModDoc))
    {
        module_renderer *pSndFile = pModDoc->GetSoundFile();
        if ((!pSndFile->Patterns.IsValidPat(m_nPattern)) || (!pSndFile->Patterns[m_nPattern].GetNumRows())) return;
        BEGIN_CRITICAL();
        // Cut instruments/samples in virtual channels
        for (modplug::tracker::chnindex_t i = pSndFile->GetNumChannels(); i < MAX_VIRTUAL_CHANNELS; i++)
        {
            pSndFile->Chn[i].flags |= CHN_NOTEFADE | CHN_KEYOFF;
        }
        pSndFile->LoopPattern(m_nPattern);
        pSndFile->m_nNextRow = GetCurrentRow();
        pSndFile->m_dwSongFlags &= ~SONG_PAUSED;
        pSndFile->m_dwSongFlags |= SONG_STEP;
        END_CRITICAL();
        if (pMainFrm->GetModPlaying() != pModDoc)
        {
            pMainFrm->PlayMod(pModDoc, m_hWnd, MPTNOTIFY_POSITION|MPTNOTIFY_VUMETERS);
        }
        if(autoStep)
        {
            if (CMainFrame::m_dwPatternSetup & PATTERN_CONTSCROLL)
                SetCurrentRow(GetCurrentRow() + 1, TRUE);
            else
                SetCurrentRow((GetCurrentRow() + 1) % pSndFile->Patterns[m_nPattern].GetNumRows(), FALSE);
        }
        SetFocus();
    }
}


void CViewPattern::OnCursorCopy()
//-------------------------------
{
    CModDoc *pModDoc = GetDocument();

    if (pModDoc)
    {
        module_renderer *pSndFile = pModDoc->GetSoundFile();
        if ((!pSndFile->Patterns[m_nPattern].GetNumRows()) || (!pSndFile->Patterns[m_nPattern])) return;
        modplug::tracker::modevent_t *m = pSndFile->Patterns[m_nPattern] + m_nRow * pSndFile->GetNumChannels() + GetChanFromCursor(m_dwCursor);
        switch(GetColTypeFromCursor(m_dwCursor))
        {
        case NOTE_COLUMN:
        case INST_COLUMN:
            m_cmdOld.note = m->note;
            m_cmdOld.instr = m->instr;
            SendCtrlMessage(CTRLMSG_SETCURRENTINSTRUMENT, m_cmdOld.instr);
            break;
        case VOL_COLUMN:
            m_cmdOld.volcmd = m->volcmd;
            m_cmdOld.vol = m->vol;
            break;
        case EFFECT_COLUMN:
        case PARAM_COLUMN:
            m_cmdOld.command = m->command;
            m_cmdOld.param = m->param;
            break;
        }
    }
}

void CViewPattern::OnCursorPaste()
//--------------------------------
{
    //rewbs.customKeys
    CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
    CModDoc *pModDoc = GetDocument();

    if ((pModDoc) && (pMainFrm) && (IsEditingEnabled_bmsg()))
    {
        PrepareUndo(m_dwBeginSel, m_dwEndSel);
        UINT nChn = GetChanFromCursor(m_dwCursor);
        UINT nCursor = GetColTypeFromCursor(m_dwCursor);
        module_renderer *pSndFile = pModDoc->GetSoundFile();
        modplug::tracker::modevent_t *p = pSndFile->Patterns[m_nPattern] +  m_nRow*pSndFile->m_nChannels + nChn;

        switch(nCursor)
        {
            case NOTE_COLUMN:    p->note = m_cmdOld.note;
            case INST_COLUMN:    p->instr = m_cmdOld.instr; break;
            case VOL_COLUMN:    p->vol = m_cmdOld.vol; p->volcmd = m_cmdOld.volcmd; break;
            case EFFECT_COLUMN:
            case PARAM_COLUMN:    p->command = m_cmdOld.command; p->param = m_cmdOld.param; break;
        }
        pModDoc->SetModified();

        if (((pMainFrm->GetFollowSong(pModDoc) != m_hWnd) || (pSndFile->IsPaused()) || (!(m_dwStatus & PATSTATUS_FOLLOWSONG))))
        {
            uint32_t sel = CreateCursor(m_nRow, nChn, 0);
            InvalidateArea(sel, sel + LAST_COLUMN);
            SetCurrentRow(m_nRow + m_nSpacing);
            sel = CreateCursor(m_nRow) | m_dwCursor;
            SetCurSel(sel, sel);
        }
    }
    //end rewbs.customKeys
}


void CViewPattern::OnInterpolateVolume()
//--------------------------------------
{
    Interpolate(VOL_COLUMN);
}

//begin rewbs.fxvis
void CViewPattern::OnVisualizeEffect()
//--------------------------------------
{
}
//end rewbs.fxvis


void CViewPattern::OnInterpolateEffect()
//--------------------------------------
{
    Interpolate(EFFECT_COLUMN);
}

void CViewPattern::OnInterpolateNote()
//--------------------------------------
{
    Interpolate(NOTE_COLUMN);
}


//static void CArrayUtils<UINT>::Merge(CArray<UINT,UINT>& Dest, CArray<UINT,UINT>& Src);

void CViewPattern::Interpolate(PatternColumns type)
//-------------------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if (!pModDoc)
        return;
    module_renderer *pSndFile = pModDoc->GetSoundFile();

    bool changed = false;
    CArray<UINT,UINT> validChans;

    if (type==EFFECT_COLUMN || type==PARAM_COLUMN)
    {
        CArray<UINT,UINT> moreValidChans;
        ListChansWhereColSelected(EFFECT_COLUMN, validChans);
        ListChansWhereColSelected(PARAM_COLUMN, moreValidChans);
        //CArrayUtils<UINT>::Merge(validChans, moreValidChans); //Causes unresolved external, not sure why yet. //XXXih: ugh
        validChans.Append(moreValidChans);                                            //for now we'll just interpolate the same data several times. :)
    } else
    {
        ListChansWhereColSelected(type, validChans);
    }

    int nValidChans = validChans.GetCount();

    //for all channels where type is selected
    for (int chnIdx=0; chnIdx<nValidChans; chnIdx++)
    {
        UINT nchn = validChans[chnIdx];
        UINT row0 = GetSelectionStartRow();
        UINT row1 = GetSelectionEndRow();

        if (!IsInterpolationPossible(row0, row1, nchn, type, pSndFile))
            continue; //skip chans where interpolation isn't possible

        if (!changed) //ensure we save undo buffer only before any channels are interpolated
            PrepareUndo(m_dwBeginSel, m_dwEndSel);

        bool doPCinterpolation = false;

        int vsrc, vdest, vcmd = 0, verr = 0, distance;
        distance = row1 - row0;

        const modplug::tracker::modevent_t srcCmd = *pSndFile->Patterns[m_nPattern].GetpModCommand(row0, nchn);
        const modplug::tracker::modevent_t destCmd = *pSndFile->Patterns[m_nPattern].GetpModCommand(row1, nchn);

        modplug::tracker::note_t PCnote = 0;
        uint16_t PCinst = 0, PCparam = 0;

        switch(type)
        {
            case NOTE_COLUMN:
                vsrc = srcCmd.note;
                vdest = destCmd.note;
                vcmd = srcCmd.instr;
                verr = (distance * 59) / NoteMax;
                break;
            case VOL_COLUMN:
                vsrc = srcCmd.vol;
                vdest = destCmd.vol;
                vcmd = srcCmd.volcmd;
                verr = (distance * 63) / 128;
                if(srcCmd.volcmd == VolCmdNone)
                {
                    vsrc = vdest;
                    vcmd = destCmd.volcmd;
                } else if(destCmd.volcmd == VolCmdNone)
                {
                    vdest = vsrc;
                }
                break;
            case PARAM_COLUMN:
            case EFFECT_COLUMN:
                if(srcCmd.IsPcNote() || destCmd.IsPcNote())
                {
                    doPCinterpolation = true;
                    PCnote = (srcCmd.IsPcNote()) ? srcCmd.note : destCmd.note;
                    vsrc = srcCmd.GetValueEffectCol();
                    vdest = destCmd.GetValueEffectCol();
                    PCparam = srcCmd.GetValueVolCol();
                    if(PCparam == 0) PCparam = destCmd.GetValueVolCol();
                    PCinst = srcCmd.instr;
                    if(PCinst == 0)
                        PCinst = destCmd.instr;
                }
                else
                {
                    vsrc = srcCmd.param;
                    vdest = destCmd.param;
                    vcmd = srcCmd.command;
                    if(srcCmd.command == CmdNone)
                    {
                        vsrc = vdest;
                        vcmd = destCmd.command;
                    } else if(destCmd.command == CmdNone)
                    {
                        vdest = vsrc;
                    }
                }
                verr = (distance * 63) / 128;
                break;
            default:
                ASSERT(false);
                return;
        }

        if (vdest < vsrc) verr = -verr;

        modplug::tracker::modevent_t* pcmd = pSndFile->Patterns[m_nPattern].GetpModCommand(row0, nchn);

        for (UINT i=0; i<=distance; i++, pcmd += pSndFile->m_nChannels)    {

            switch(type) {
                case NOTE_COLUMN:
                    if ((!pcmd->note) || (pcmd->instr == vcmd))    {
                        int note = vsrc + ((vdest - vsrc) * (int)i + verr) / distance;
                        pcmd->note = (uint8_t)note;
                        pcmd->instr = vcmd;
                    }
                    break;
                case VOL_COLUMN:
                    if ((!pcmd->volcmd) || (pcmd->volcmd == vcmd))    {
                        int vol = vsrc + ((vdest - vsrc) * (int)i + verr) / distance;
                        pcmd->vol = (uint8_t)vol;
                        //XXXih: gross
                        pcmd->volcmd = (modplug::tracker::volcmd_t) vcmd;
                    }
                    break;
                case EFFECT_COLUMN:
                    if(doPCinterpolation)
                    {    // With PC/PCs notes, copy PCs note and plug index to all rows where
                        // effect interpolation is done if no PC note with non-zero instrument is there.
                        const uint16_t val = static_cast<uint16_t>(vsrc + ((vdest - vsrc) * (int)i + verr) / distance);
                        if (pcmd->IsPcNote() == false || pcmd->instr == 0)
                        {
                            pcmd->note = PCnote;
                            pcmd->instr = PCinst;
                        }
                        pcmd->SetValueVolCol(PCparam);
                        pcmd->SetValueEffectCol(val);
                    }
                    else
                    {
                        if ((!pcmd->command) || (pcmd->command == vcmd))
                        {
                            int val = vsrc + ((vdest - vsrc) * (int)i + verr) / distance;
                            pcmd->param = (uint8_t)val;
                            //XXXih: gross
                            pcmd->command = (modplug::tracker::cmd_t) vcmd;
                        }
                    }
                    break;
                default:
                    ASSERT(false);
            }
        }

        changed=true;

    } //end for all channels where type is selected

    if (changed) {
        pModDoc->SetModified();
        InvalidatePattern(FALSE);
    }
}


BOOL CViewPattern::TransposeSelection(int transp)
//-----------------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if (pModDoc)
    {
        const UINT row0 = GetRowFromCursor(m_dwBeginSel), row1 = GetRowFromCursor(m_dwEndSel);
        const UINT col0 = ((m_dwBeginSel & 0xFFFF)+7) >> 3, col1 = GetChanFromCursor(m_dwEndSel);
        module_renderer *pSndFile = pModDoc->GetSoundFile();
        modplug::tracker::modevent_t *pcmd = pSndFile->Patterns[m_nPattern];
        const modplug::tracker::note_t noteMin = pSndFile->GetModSpecifications().noteMin;
        const modplug::tracker::note_t noteMax = pSndFile->GetModSpecifications().noteMax;

        if ((!pcmd) || (col0 > col1) || (col1 >= pSndFile->m_nChannels)
         || (row0 > row1) || (row1 >= pSndFile->Patterns[m_nPattern].GetNumRows())) return FALSE;
        PrepareUndo(m_dwBeginSel, m_dwEndSel);
        for (UINT row=row0; row <= row1; row++)
        {
            modplug::tracker::modevent_t *m = &pcmd[row * pSndFile->m_nChannels];
            for (UINT col=col0; col<=col1; col++)
            {
                int note = m[col].note;
                if ((note >= NoteMin) && (note <= NoteMax))
                {
                    note += transp;
                    if (note < noteMin) note = noteMin;
                    if (note > noteMax) note = noteMax;
                    m[col].note = (uint8_t)note;
                }
            }
        }
        pModDoc->SetModified();
        InvalidateSelection();
        return TRUE;
    }
    return FALSE;
}


void CViewPattern::OnDropSelection()
//----------------------------------
{
    modplug::tracker::modevent_t *pOldPattern, *pNewPattern;
    CModDoc *pModDoc;
    module_renderer *pSndFile;
    int x1, y1, x2, y2, c1, c2, xc1, xc2, xmc1, xmc2, ym1, ym2;
    int dx, dy, nChannels, nRows;

    if ((pModDoc = GetDocument()) == NULL || !(IsEditingEnabled_bmsg())) return;
    pSndFile = pModDoc->GetSoundFile();
    nChannels = pSndFile->GetNumChannels();
    nRows = pSndFile->Patterns[m_nPattern].GetNumRows();
    pOldPattern = pSndFile->Patterns[m_nPattern];
    if ((nChannels < 1) || (nRows < 1) || (!pOldPattern)) return;
    dx = (int)GetChanFromCursor(m_dwDragPos) - (int)GetChanFromCursor(m_dwStartSel);
    dy = (int)GetRowFromCursor(m_dwDragPos) - (int)GetRowFromCursor(m_dwStartSel);
    if ((!dx) && (!dy)) return;
    //pModDoc->GetPatternUndo()->PrepareUndo(m_nPattern, 0,0, nChannels, nRows);
    pNewPattern = CPattern::AllocatePattern(nRows, nChannels);
    if (!pNewPattern) return;
    x1 = GetChanFromCursor(m_dwBeginSel);
    y1 = GetRowFromCursor(m_dwBeginSel);
    x2 = GetChanFromCursor(m_dwEndSel);
    y2 = GetRowFromCursor(m_dwEndSel);
    c1 = GetColTypeFromCursor(m_dwBeginSel);
    c2 = GetColTypeFromCursor(m_dwEndSel);
    if (c1 > EFFECT_COLUMN) c1 = EFFECT_COLUMN;
    if (c2 > EFFECT_COLUMN) c2 = EFFECT_COLUMN;
    xc1 = x1*4+c1;
    xc2 = x2*4+c2;
    xmc1 = xc1+dx*4;
    xmc2 = xc2+dx*4;
    ym1 = y1+dy;
    ym2 = y2+dy;
    BeginWaitCursor();
    modplug::tracker::modevent_t *p = pNewPattern;
    for (int y=0; y<nRows; y++)
    {
        for (int x=0; x<nChannels; x++)
        {
            for (int c=0; c<4; c++)
            {
                int xsrc=x, ysrc=y, xc=x*4+c;

                // Destination is from destination selection
                if ((xc >= xmc1) && (xc <= xmc2) && (y >= ym1) && (y <= ym2))
                {
                    xsrc -= dx;
                    ysrc -= dy;
                } else
                // Destination is from source rectangle (clear)
                if ((xc >= xc1) && (xc <= xc2) && (y >= y1) && (y <= y2))
                {
                    if (!(m_dwStatus & (PATSTATUS_KEYDRAGSEL|PATSTATUS_CTRLDRAGSEL))) xsrc = -1;
                }
                // Copy the data
                if ((xsrc >= 0) && (xsrc < nChannels) && (ysrc >= 0) && (ysrc < nRows))
                {
                    modplug::tracker::modevent_t *psrc = &pOldPattern[ysrc*nChannels+xsrc];
                    switch(c)
                    {
                    case 0:    p->note = psrc->note; break;
                    case 1: p->instr = psrc->instr; break;
                    case 2: p->vol = psrc->vol; p->volcmd = psrc->volcmd; break;
                    default: p->command = psrc->command; p->param = psrc->param; break;
                    }
                }
            }
            p++;
        }
    }

    BEGIN_CRITICAL();
    pSndFile->Patterns[m_nPattern] = pNewPattern;
    END_CRITICAL();


    x1 += dx;
    x2 += dx;
    y1 += dy;
    y2 += dy;
    if (x1<0) { x1=0; c1=0; }
    if (x1>=nChannels) x1=nChannels-1;
    if (x2<0) x2 = 0;
    if (x2>=nChannels) { x2=nChannels-1; c2=4; }
    if (y1<0) y1=0;
    if (y1>=nRows) y1=nRows-1;
    if (y2<0) y2=0;
    if (y2>=nRows) y2=nRows-1;
    if (c2 >= 3) c2 = 4;
    // Fix: Horizontal scrollbar pos screwed when selecting with mouse
    SetCursorPosition( y1, (x1<<3)|c1 );
    SetCurSel(CreateCursor(y1, x1, c1), CreateCursor(y2, x2, c2));
    InvalidatePattern();
    CPattern::FreePattern(pOldPattern);
    pModDoc->SetModified();
    EndWaitCursor();
}


void CViewPattern::OnSetSelInstrument()
//-------------------------------------
{
    SetSelectionInstrument(GetCurrentInstrument());
}


void CViewPattern::OnRemoveChannelDialog()
//----------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if(pModDoc == nullptr) return;
    pModDoc->ChangeNumChannels(0);
    SetCurrentPattern(m_nPattern); //Updating the screen.
}


void CViewPattern::OnRemoveChannel()
//----------------------------------
{
    CModDoc *pModDoc = GetDocument();
    module_renderer* pSndFile;
    if (pModDoc == nullptr || (pSndFile = pModDoc->GetSoundFile()) == nullptr) return;

    if(pSndFile->m_nChannels <= pSndFile->GetModSpecifications().channelsMin)
    {
        CMainFrame::GetMainFrame()->MessageBox("No channel removed - channel number already at minimum.", "Remove channel", MB_OK | MB_ICONINFORMATION);
        return;
    }

    modplug::tracker::chnindex_t nChn = GetChanFromCursor(m_nMenuParam);
    const bool isEmpty = pModDoc->IsChannelUnused(nChn);

    CString str;
    str.Format("Remove channel %d? This channel still contains note data!\nNote: Operation affects all patterns and has no undo", nChn + 1);
    if(isEmpty || CMainFrame::GetMainFrame()->MessageBox(str , "Remove channel", MB_YESNO | MB_ICONQUESTION) == IDYES)
    {
        vector<bool> keepMask(pModDoc->GetNumChannels(), true);
        keepMask[nChn] = false;
        pModDoc->RemoveChannels(keepMask);
        SetCurrentPattern(m_nPattern); //Updating the screen.
    }
}


void CViewPattern::AddChannelBefore(modplug::tracker::chnindex_t nBefore)
//-------------------------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if(pModDoc == nullptr) return;

    BeginWaitCursor();
    // Create new channel order, with channel nBefore being an invalid (and thus empty) channel.
    vector<modplug::tracker::chnindex_t> channels(pModDoc->GetNumChannels() + 1, ChannelIndexInvalid);
    modplug::tracker::chnindex_t i = 0;
    for(modplug::tracker::chnindex_t nChn = 0; nChn < pModDoc->GetNumChannels() + 1; nChn++)
    {
        if(nChn != nBefore)
        {
            channels[nChn] = i++;
        }
    }

    if (pModDoc->ReArrangeChannels(channels) != ChannelIndexInvalid)
    {
        pModDoc->SetModified();
        pModDoc->UpdateAllViews(NULL, HINT_MODCHANNELS); //refresh channel headers
        pModDoc->UpdateAllViews(NULL, HINT_MODTYPE); //updates(?) the channel number to general tab display
        SetCurrentPattern(m_nPattern);
    }
    EndWaitCursor();
}


void CViewPattern::OnDuplicateChannel()
//------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if (pModDoc == nullptr) return;

    if(AfxMessageBox(GetStrI18N(_TEXT("This affects all patterns, proceed?")), MB_YESNO) != IDYES)
        return;

    const modplug::tracker::chnindex_t nDupChn = GetChanFromCursor(m_nMenuParam);
    if(nDupChn >= pModDoc->GetNumChannels())
        return;

    BeginWaitCursor();
    // Create new channel order, with channel nDupChn duplicated.
    vector<modplug::tracker::chnindex_t> channels(pModDoc->GetNumChannels() + 1, 0);
    modplug::tracker::chnindex_t i = 0;
    for(modplug::tracker::chnindex_t nChn = 0; nChn < pModDoc->GetNumChannels() + 1; nChn++)
    {
        channels[nChn] = i;
        if(nChn != nDupChn)
        {
            i++;
        }
    }

    // Check that duplication happened and in that case update.
    if(pModDoc->ReArrangeChannels(channels) != ChannelIndexInvalid)
    {
        pModDoc->SetModified();
        pModDoc->UpdateAllViews(NULL, HINT_MODCHANNELS);
        pModDoc->UpdateAllViews(NULL, HINT_MODTYPE);
        SetCurrentPattern(m_nPattern);
    }
    EndWaitCursor();
}

void CViewPattern::OnRunScript()
//------------------------------
{
    ;
}


void CViewPattern::OnTransposeUp()
//--------------------------------
{
    TransposeSelection(1);
}


void CViewPattern::OnTransposeDown()
//----------------------------------
{
    TransposeSelection(-1);
}


void CViewPattern::OnTransposeOctUp()
//-----------------------------------
{
    TransposeSelection(12);
}


void CViewPattern::OnTransposeOctDown()
//-------------------------------------
{
    TransposeSelection(-12);
}


void CViewPattern::OnSwitchToOrderList()
//--------------------------------------
{
    PostCtrlMessage(CTRLMSG_SETFOCUS);
}


void CViewPattern::OnPrevOrder()
//------------------------------
{
    PostCtrlMessage(CTRLMSG_PREVORDER);
}


void CViewPattern::OnNextOrder()
//------------------------------
{
    PostCtrlMessage(CTRLMSG_NEXTORDER);
}


void CViewPattern::OnUpdateUndo(CCmdUI *pCmdUI)
//---------------------------------------------
{
}


void CViewPattern::OnEditUndo()
//-----------------------------
{
}


void CViewPattern::OnPatternAmplify()
//-----------------------------------
{
    static UINT snOldAmp = 100;
    CAmpDlg dlg(this, snOldAmp, 0);
    CModDoc *pModDoc = GetDocument();

    if ((pModDoc) && (dlg.DoModal() == IDOK))
    {
        module_renderer *pSndFile = pModDoc->GetSoundFile();
        const bool useVolCol = (pSndFile->GetType() != MOD_TYPE_MOD);

        BeginWaitCursor();
        PrepareUndo(m_dwBeginSel, m_dwEndSel);
        snOldAmp = static_cast<UINT>(dlg.m_nFactor);

        if (pSndFile->Patterns[m_nPattern])
        {
            modplug::tracker::modevent_t *p = pSndFile->Patterns[m_nPattern];

            modplug::tracker::chnindex_t firstChannel = GetSelectionStartChan(), lastChannel = GetSelectionEndChan();
            modplug::tracker::rowindex_t firstRow = GetSelectionStartRow(), lastRow = GetSelectionEndRow();
            firstChannel = CLAMP(firstChannel, 0, pSndFile->m_nChannels - 1);
            lastChannel = CLAMP(lastChannel, firstChannel, pSndFile->m_nChannels - 1);
            firstRow = CLAMP(firstRow, 0, pSndFile->Patterns[m_nPattern].GetNumRows() - 1);
            lastRow = CLAMP(lastRow, firstRow, pSndFile->Patterns[m_nPattern].GetNumRows() - 1);

            // adjust bad_min/bad_max channel if they're only partly selected (i.e. volume column or effect column (when using .MOD) is not covered)
            if(((firstChannel << 3) | (useVolCol ? 2 : 3)) < (m_dwBeginSel & 0xFFFF))
            {
                if(firstChannel >= lastChannel)
                {
                    // Selection too small!
                    EndWaitCursor();
                    return;
                }
                firstChannel++;
            }
            if(((lastChannel << 3) | (useVolCol ? 2 : 3)) > (m_dwEndSel & 0xFFFF))
            {
                if(lastChannel == 0)
                {
                    // Selection too small!
                    EndWaitCursor();
                    return;
                }
                lastChannel--;
            }

            // Volume memory for each channel.
            vector<uint8_t> chvol(lastChannel + 1, 64);

            for (modplug::tracker::rowindex_t nRow = firstRow; nRow <= lastRow; nRow++)
            {
                modplug::tracker::modevent_t *m = p + nRow * pSndFile->m_nChannels + firstChannel;
                for (modplug::tracker::chnindex_t nChn = firstChannel; nChn <= lastChannel; nChn++, m++)
                {
                    if ((m->command == CmdVol) && (m->param <= 64))
                    {
                        chvol[nChn] = m->param;
                        break;
                    }
                    if (m->volcmd == VolCmdVol)
                    {
                        chvol[nChn] = m->vol;
                        break;
                    }
                    if ((m->note) && (m->note <= NoteMax) && (m->instr))
                    {
                        UINT nSmp = m->instr;
                        if (pSndFile->m_nInstruments)
                        {
                            if ((nSmp <= pSndFile->m_nInstruments) && (pSndFile->Instruments[nSmp]))
                            {
                                nSmp = pSndFile->Instruments[nSmp]->Keyboard[m->note];
                                if(!nSmp) chvol[nChn] = 64;    // hack for instruments without samples
                            } else
                            {
                                nSmp = 0;
                            }
                        }
                        if ((nSmp) && (nSmp <= pSndFile->m_nSamples))
                        {
                            chvol[nChn] = (uint8_t)(pSndFile->Samples[nSmp].default_volume >> 2);
                            break;
                        }
                        else
                        {    //nonexistant sample and no volume present in patten? assume volume=64.
                            if(useVolCol)
                            {
                                m->volcmd = VolCmdVol;
                                m->vol = 64;
                            } else
                            {
                                m->command = CmdVol;
                                m->param = 64;
                            }
                            chvol[nChn] = 64;
                            break;
                        }
                    }
                }
            }

            for (modplug::tracker::rowindex_t nRow = firstRow; nRow <= lastRow; nRow++)
            {
                modplug::tracker::modevent_t *m = p + nRow * pSndFile->m_nChannels + firstChannel;
                const int cy = lastRow - firstRow + 1; // total rows (for fading)
                for (modplug::tracker::chnindex_t nChn = firstChannel; nChn <= lastChannel; nChn++, m++)
                {
                    if ((!m->volcmd) && (m->command != CmdVol)
                     && (m->note) && (m->note <= NoteMax) && (m->instr))
                    {
                        UINT nSmp = m->instr;
                        bool overrideSampleVol = false;
                        if (pSndFile->m_nInstruments)
                        {
                            if ((nSmp <= pSndFile->m_nInstruments) && (pSndFile->Instruments[nSmp]))
                            {
                                nSmp = pSndFile->Instruments[nSmp]->Keyboard[m->note];
                                // hack for instruments without samples
                                if(!nSmp)
                                {
                                    nSmp = 1;
                                    overrideSampleVol = true;
                                }
                            } else
                            {
                                nSmp = 0;
                            }
                        }
                        if ((nSmp) && (nSmp <= pSndFile->m_nSamples))
                        {
                            if(useVolCol)
                            {
                                m->volcmd = VolCmdVol;
                                m->vol = (overrideSampleVol) ? 64 : pSndFile->Samples[nSmp].default_volume >> 2;
                            } else
                            {
                                m->command = CmdVol;
                                m->param = (overrideSampleVol) ? 64 : pSndFile->Samples[nSmp].default_volume >> 2;
                            }
                        }
                    }
                    if (m->volcmd == VolCmdVol) chvol[nChn] = (uint8_t)m->vol;
                    if (((dlg.m_bFadeIn) || (dlg.m_bFadeOut)) && (m->command != CmdVol) && (!m->volcmd))
                    {
                        if(useVolCol)
                        {
                            m->volcmd = VolCmdVol;
                            m->vol = chvol[nChn];
                        } else
                        {
                            m->command = CmdVol;
                            m->param = chvol[nChn];
                        }
                    }
                    if (m->volcmd == VolCmdVol)
                    {
                        int vol = m->vol * dlg.m_nFactor;
                        if (dlg.m_bFadeIn) vol = (vol * (nRow+1-firstRow)) / cy;
                        if (dlg.m_bFadeOut) vol = (vol * (cy+firstRow-nRow)) / cy;
                        vol = (vol + 50) / 100;
                        if (vol > 64) vol = 64;
                        m->vol = (uint8_t)vol;
                    }
                    if ((((nChn << 3) | 3) >= (m_dwBeginSel & 0xFFFF)) && (((nChn << 3) | 3) <= (m_dwEndSel & 0xFFFF)))
                    {
                        if ((m->command == CmdVol) && (m->param <= 64))
                        {
                            int vol = m->param * dlg.m_nFactor;
                            if (dlg.m_bFadeIn) vol = (vol * (nRow + 1 - firstRow)) / cy;
                            if (dlg.m_bFadeOut) vol = (vol * (cy + firstRow - nRow)) / cy;
                            vol = (vol + 50) / 100;
                            if (vol > 64) vol = 64;
                            m->param = (uint8_t)vol;
                        }
                    }
                }
            }
        }
        pModDoc->SetModified();
        InvalidateSelection();
        EndWaitCursor();
    }
}


LRESULT CViewPattern::OnPlayerNotify(MPTNOTIFICATION *pnotify)
//------------------------------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if ((!pModDoc) || (!pnotify)) return 0;
    if (pnotify->dwType & MPTNOTIFY_POSITION)
    {
        UINT nOrd = pnotify->nOrder;
        UINT nRow = pnotify->nRow;
        //UINT nPat = 0xFFFF;
        UINT nPat = pnotify->nPattern; //get player pattern
        module_renderer *pSndFile = pModDoc->GetSoundFile();
        bool updateOrderList = false;

        if(m_nLastPlayedOrder != nOrd)
        {
            updateOrderList = true;
            m_nLastPlayedOrder = nOrd;
        }

        if (nRow < m_nLastPlayedRow) {
            InvalidateChannelsHeaders();
        }
        m_nLastPlayedRow = nRow;

        if (pSndFile->m_dwSongFlags & (SONG_PAUSED|SONG_STEP)) return 0;

        //rewbs.toCheck: is it safe to remove this? What if a pattern has no order?
        /*
        if (pSndFile->m_dwSongFlags & SONG_PATTERNLOOP)
        {
            nPat = pSndFile->m_nPattern;
            nOrd = 0xFFFF;
        } else
        */
        /*
        if (nOrd < MAX_ORDERS) {
            nPat = pSndFile->Order[nOrd];
        }
        */

        if (nOrd >= pSndFile->Order.GetLength() || pSndFile->Order[nOrd] != nPat) {
            //order doesn't correlate with pattern, so mark it as invalid
            nOrd = 0xFFFF;
        }

        if ((m_dwStatus & (PATSTATUS_FOLLOWSONG|PATSTATUS_DRAGVSCROLL|PATSTATUS_DRAGHSCROLL|PATSTATUS_MOUSEDRAGSEL|PATSTATUS_SELECTROW)) == PATSTATUS_FOLLOWSONG)
        {
            if (nPat < pSndFile->Patterns.Size())
            {
                if (nPat != m_nPattern || updateOrderList)
                {
                    if(nPat != m_nPattern) SetCurrentPattern(nPat, nRow);
                    if (nOrd < pSndFile->Order.size()) SendCtrlMessage(CTRLMSG_SETCURRENTORDER, nOrd);
                    updateOrderList = false;
                }
                if (nRow != m_nRow)    SetCurrentRow((nRow < pSndFile->Patterns[nPat].GetNumRows()) ? nRow : 0, FALSE, FALSE);
            }
            SetPlayCursor(0xFFFF, 0);
        } else
        {
            if(updateOrderList)
            {
                SendCtrlMessage(CTRLMSG_FORCEREFRESH); //force orderlist refresh
                updateOrderList = false;
            }
            SetPlayCursor(nPat, nRow);
        }



    } //Ends condition "if(pnotify->dwType & MPTNOTIFY_POSITION)"

    UpdateIndicator();

    return 0;


}

// record plugin parameter changes into current pattern
LRESULT CViewPattern::OnRecordPlugParamChange(WPARAM plugSlot, LPARAM paramIndex)
//-------------------------------------------------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    //if (!m_bRecord || !pModDoc) {
    if (!IsEditingEnabled() || !pModDoc)
    {
        return 0;
    }
    module_renderer *pSndFile = pModDoc->GetSoundFile();
    if (!pSndFile)
    {
        return 0;
    }

    //Work out where to put the new data
    const UINT nChn = GetChanFromCursor(m_dwCursor);
    const bool bUsePlaybackPosition = IsLiveRecord(*pModDoc, *pSndFile);
    modplug::tracker::rowindex_t nRow = m_nRow;
    modplug::tracker::patternindex_t nPattern = m_nPattern;
    if(bUsePlaybackPosition == true)
        SetEditPos(*pSndFile, nRow, nPattern, pSndFile->m_nRow, pSndFile->m_nPattern);

    modplug::tracker::modevent_t *pRow = pSndFile->Patterns[nPattern].GetpModCommand(nRow, nChn);

    // TODO: Is the right plugin active? Move to a chan with the right plug
    // Probably won't do this - finish fluctuator implementation instead.

    CVstPlugin *pPlug = (CVstPlugin*)pSndFile->m_MixPlugins[plugSlot].pMixPlugin;
    if (pPlug == nullptr) return 0;

    if(pSndFile->GetType() == MOD_TYPE_MPT)
    {
        // MPTM: Use PC Notes

        // only overwrite existing PC Notes
        if(pRow->IsEmpty() || pRow->IsPcNote())
        {
            //pModDoc->GetPatternUndo()->PrepareUndo(nPattern, nChn, nRow, 1, 1);

            pRow->Set(NotePcSmooth, plugSlot + 1, paramIndex, static_cast<uint16_t>(pPlug->GetParameter(paramIndex) * modplug::tracker::modevent_t::MaxColumnValue));
            InvalidateRow(nRow);
        }
    } else if(pSndFile->GetModSpecifications().HasCommand(CmdSmoothMidi))
    {
        // Other formats: Use MIDI macros

        //Figure out which plug param (if any) is controllable using the active macro on this channel.
        long activePlugParam  = -1;
        uint8_t activeMacro      = pSndFile->Chn[nChn].nActiveMacro;
        CString activeMacroString = pSndFile->m_MidiCfg.szMidiSFXExt[activeMacro];
        if (pModDoc->GetMacroType(activeMacroString) == sfx_plug)
        {
            activePlugParam = pModDoc->MacroToPlugParam(activeMacroString);
        }
        //If the wrong macro is active, see if we can find the right one.
        //If we can, activate it for this chan by writing appropriate SFx command it.
        if (activePlugParam != paramIndex)
        {
            int foundMacro = pModDoc->FindMacroForParam(paramIndex);
            if (foundMacro >= 0)
            {
                pSndFile->Chn[nChn].nActiveMacro = foundMacro;
                if (pRow->command == CmdNone || pRow->command == CmdSmoothMidi || pRow->command == CmdMidi) //we overwrite existing Zxx and \xx only.
                {
                    //pModDoc->GetPatternUndo()->PrepareUndo(nPattern, nChn, nRow, 1, 1);

                    pRow->command = (pSndFile->m_nType & (MOD_TYPE_IT | MOD_TYPE_MPT)) ? CmdS3mCmdEx : CmdModCmdEx;;
                    pRow->param = 0xF0 + (foundMacro&0x0F);
                    InvalidateRow(nRow);
                }

            }
        }

        //Write the data, but we only overwrite if the command is a macro anyway.
        if (pRow->command == CmdNone || pRow->command == CmdSmoothMidi || pRow->command == CmdMidi)
        {
            //pModDoc->GetPatternUndo()->PrepareUndo(nPattern, nChn, nRow, 1, 1);

            pRow->command = CmdSmoothMidi;
            pRow->param = pPlug->GetZxxParameter(paramIndex);
            InvalidateRow(nRow);
        }

    }

    return 0;

}


ModCommandPos CViewPattern::GetEditPos(module_renderer& rSf, const bool bLiveRecord) const
//-----------------------------------------------------------------------------------
{
    ModCommandPos editpos;
    if(bLiveRecord)
        SetEditPos(rSf, editpos.nRow, editpos.nPat, rSf.m_nRow, rSf.m_nPattern);
    else
        {editpos.nPat = m_nPattern; editpos.nRow = m_nRow;}
    editpos.nChn = GetChanFromCursor(m_dwCursor);
    return editpos;
}


modplug::tracker::modevent_t* CViewPattern::GetModCommand(module_renderer& rSf, const ModCommandPos& pos)
//--------------------------------------------------------------------------------
{
    static modplug::tracker::modevent_t m;
    if (rSf.Patterns.IsValidPat(pos.nPat) && pos.nRow < rSf.Patterns[pos.nPat].GetNumRows() && pos.nChn < rSf.GetNumChannels())
        return rSf.Patterns[pos.nPat].GetpModCommand(pos.nRow, pos.nChn);
    else
        return &m;
}


LRESULT CViewPattern::OnMidiMsg(WPARAM dwMidiDataParam, LPARAM)
//--------------------------------------------------------
{
    const uint32_t dwMidiData = dwMidiDataParam;
    static uint8_t midivolume = 127;

    CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
    CModDoc *pModDoc = GetDocument();

    if ((!pModDoc) || (!pMainFrm)) return 0;

    module_renderer* pSndFile = pModDoc->GetSoundFile();
    if(!pSndFile) return 0;

//Midi message from our perspective:
//     +---------------------------+---------------------------+-------------+-------------+
//bit: | 24.23.22.21 | 20.19.18.17 | 16.15.14.13 | 12.11.10.09 | 08.07.06.05 | 04.03.02.01 |
//     +---------------------------+---------------------------+-------------+-------------+
//     |     Velocity (0-127)      |  Note (middle C is 60)    |   Event     |   Channel   |
//     +---------------------------+---------------------------+-------------+-------------+
//(http://www.borg.com/~jglatt/tech/midispec.htm)

    //Notes:
    //. Initial midi data handling is done in MidiInCallBack().
    //. If no event is received, previous event is assumed.
    //. A note-on (event=9) with velocity 0 is equivalent to a note off.
    //. Basing the event solely on the velocity as follows is incorrect,
    //  since a note-off can have a velocity too:
    //  uint8_t event  = (dwMidiData>>16) & 0x64;
    //. Sample- and instrumentview handle midi mesages in their own methods.

    const uint8_t nByte1 = GetFromMIDIMsg_DataByte1(dwMidiData);
    const uint8_t nByte2 = GetFromMIDIMsg_DataByte2(dwMidiData);

    const uint8_t nNote = nByte1 + 1;                            // +1 is for MPT, where middle C is 61
    int nVol = nByte2;                                                    // At this stage nVol is a non linear value in [0;127]
                                                // Need to convert to linear in [0;64] - see below
    uint8_t event = GetFromMIDIMsg_Event(dwMidiData);

    if ((event == MIDIEVENT_NOTEON) && !nVol) event = MIDIEVENT_NOTEOFF;    //Convert event to note-off if req'd


    // Handle MIDI mapping.
    uint8_t mappedIndex = UINT8_MAX, paramValue = UINT8_MAX;
    uint32_t paramIndex = 0;
    const bool bCaptured = pSndFile->GetMIDIMapper().OnMIDImsg(dwMidiData, mappedIndex, paramIndex, paramValue);

    // Write parameter control commands if needed.
    if (paramValue != UINT8_MAX && IsEditingEnabled() && pSndFile->GetType() == MOD_TYPE_MPT)
    {
        // Note: There's no undo for these modifications.
        const bool bLiveRecord = IsLiveRecord(*pModDoc, *pSndFile);
        ModCommandPos editpos = GetEditPos(*pSndFile, bLiveRecord);
        modplug::tracker::modevent_t* p = GetModCommand(*pSndFile, editpos);
        //pModDoc->GetPatternUndo()->PrepareUndo(editpos.nPat, editpos.nChn, editpos.nRow, editpos.nChn, editpos.nRow);
        p->Set(NotePcSmooth, mappedIndex, static_cast<uint16_t>(paramIndex), static_cast<uint16_t>((paramValue * modplug::tracker::modevent_t::MaxColumnValue)/127));
        if(bLiveRecord == false)
            InvalidateRow(editpos.nRow);
        pMainFrm->ThreadSafeSetModified(pModDoc);
    }

    if (bCaptured)
        return 0;

    switch(event)
    {
        case MIDIEVENT_NOTEOFF: // Note Off
            // The following method takes care of:
            // . Silencing specific active notes (just setting nNote to 255 as was done before is not acceptible)
            // . Entering a note off in pattern if required
            TempStopNote(nNote, ((CMainFrame::m_dwMidiSetup & MIDISETUP_RECORDNOTEOFF) != 0));
        break;

        case MIDIEVENT_NOTEON: // Note On
            nVol = ApplyVolumeRelatedMidiSettings(dwMidiData, midivolume);
            if(nVol < 0) nVol = -1;
            else nVol = (nVol + 3) / 4; //Value from [0,256] to [0,64]
            TempEnterNote(nNote, true, nVol);

            // continue playing as soon as MIDI notes are being received (http://forum.openmpt.org/index.php?topic=2813.0)
            if(pSndFile->IsPaused() && (CMainFrame::m_dwMidiSetup & MIDISETUP_PLAYPATTERNONMIDIIN))
                pModDoc->OnPatternPlayNoLoop();

        break;

        case MIDIEVENT_CONTROLLERCHANGE: //Controller change
            switch(nByte1)
            {
                case MIDICC_Volume_Coarse: //Volume
                    midivolume = nByte2;
                break;
            }

            // Checking whether to record MIDI controller change as MIDI macro change.
            // Don't write this if command was already written by MIDI mapping.
            if((paramValue == UINT8_MAX || pSndFile->GetType() != MOD_TYPE_MPT) && IsEditingEnabled() && (CMainFrame::m_dwMidiSetup & MIDISETUP_MIDIMACROCONTROL))
            {
                // Note: There's no undo for these modifications.
                const bool bLiveRecord = IsLiveRecord(*pModDoc, *pSndFile);
                ModCommandPos editpos = GetEditPos(*pSndFile, bLiveRecord);
                modplug::tracker::modevent_t* p = GetModCommand(*pSndFile, editpos);
                if(p->command == 0 || p->command == CmdSmoothMidi || p->command == CmdMidi)
                {   // Write command only if there's no existing command or already a midi macro command.
                    p->command = CmdSmoothMidi;
                    p->param = nByte2;
                    pMainFrm->ThreadSafeSetModified(pModDoc);

                    // Update GUI only if not recording live.
                    if(bLiveRecord == false)
                        InvalidateRow(editpos.nRow);
                }
            }

        default:
            if(CMainFrame::m_dwMidiSetup & MIDISETUP_RESPONDTOPLAYCONTROLMSGS)
            {
                switch(dwMidiData & 0xFF)
                {
                case 0xFA: //Start song
                    if(GetDocument())
                        GetDocument()->OnPlayerPlayFromStart();
                    break;

                case 0xFB: //Continue song
                    if(GetDocument())
                        GetDocument()->OnPlayerPlay();
                    break;

                case 0xFC: //Stop song
                    if(GetDocument())
                        GetDocument()->OnPlayerStop();
                    break;
                }
            }

            if(CMainFrame::m_dwMidiSetup & MIDISETUP_MIDITOPLUG
                && pMainFrm->GetModPlaying() == pModDoc)
            {
                const UINT instr = GetCurrentInstrument();
                IMixPlugin* plug = pSndFile->GetInstrumentPlugin(instr);
                if(plug)
                {
                    plug->MidiSend(dwMidiData);
                    // Sending midi may modify the plug. For now, if MIDI data
                    // is not active sensing, set modified.
                    if(dwMidiData != MIDISTATUS_ACTIVESENSING)
                        pMainFrm->ThreadSafeSetModified(pModDoc);
                }

            }
        break;
    }

    return 0;
}




LRESULT CViewPattern::OnModViewMsg(WPARAM wParam, LPARAM lParam)
//--------------------------------------------------------------
{
    switch(wParam)
    {

    case VIEWMSG_SETCTRLWND:
        m_hWndCtrl = (HWND)lParam;
        SetCurrentPattern(SendCtrlMessage(CTRLMSG_GETCURRENTPATTERN));
        break;

    case VIEWMSG_GETCURRENTPATTERN:
        return m_nPattern;

    case VIEWMSG_SETCURRENTPATTERN:
        if (m_nPattern != (UINT)lParam) SetCurrentPattern(lParam);
        break;

    case VIEWMSG_GETCURRENTPOS:
        return (m_nPattern << 16) | (m_nRow);

    case VIEWMSG_FOLLOWSONG:
        m_dwStatus &= ~PATSTATUS_FOLLOWSONG;
        if (lParam)
        {
            CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
            CModDoc *pModDoc = GetDocument();
            m_dwStatus |= PATSTATUS_FOLLOWSONG;
            if (pModDoc) pModDoc->SetFollowWnd(m_hWnd, MPTNOTIFY_POSITION|MPTNOTIFY_VUMETERS);
            if (pMainFrm) pMainFrm->SetFollowSong(pModDoc, m_hWnd, TRUE, MPTNOTIFY_POSITION|MPTNOTIFY_VUMETERS);
            SetFocus();
        } else
        {
            InvalidateRow();
        }
        break;

    case VIEWMSG_PATTERNLOOP:
        SendCtrlMessage(CTRLMSG_PAT_LOOP, lParam);
        break;

    case VIEWMSG_SETRECORD:
        if (lParam) m_dwStatus |= PATSTATUS_RECORD; else m_dwStatus &= ~PATSTATUS_RECORD;
        break;

    case VIEWMSG_SETSPACING:
        m_nSpacing = lParam;
        break;

    case VIEWMSG_PATTERNPROPERTIES:
        OnPatternProperties();
        GetParentFrame()->SetActiveView(this);
        break;

    case VIEWMSG_SETVUMETERS:
        if (lParam) m_dwStatus |= PATSTATUS_VUMETERS; else m_dwStatus &= ~PATSTATUS_VUMETERS;
        UpdateSizes();
        UpdateScrollSize();
        InvalidatePattern(TRUE);
        break;

    case VIEWMSG_SETPLUGINNAMES:
        if (lParam) m_dwStatus |= PATSTATUS_PLUGNAMESINHEADERS; else m_dwStatus &= ~PATSTATUS_PLUGNAMESINHEADERS;
        UpdateSizes();
        UpdateScrollSize();
        InvalidatePattern(TRUE);
        break;

    case VIEWMSG_DOMIDISPACING:
        if (m_nSpacing)
        {
// -> CODE#0012
// -> DESC="midi keyboard split"
            //CModDoc *pModDoc = GetDocument();
            //CSoundFile * pSndFile = pModDoc->GetSoundFile();
//                    if (timeGetTime() - lParam >= 10)
            int temp = timeGetTime();
            if (temp - lParam >= 60)
// -! NEW_FEATURE#0012
            {
                CModDoc *pModDoc = GetDocument();
                CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
                if ((!(m_dwStatus & PATSTATUS_FOLLOWSONG))
                 || (pMainFrm->GetFollowSong(pModDoc) != m_hWnd)
                 || (pModDoc->GetSoundFile()->IsPaused()))
                {
                    SetCurrentRow(m_nRow + m_nSpacing);
                }
                m_dwStatus &= ~PATSTATUS_MIDISPACINGPENDING;
            } else
            {
// -> CODE#0012
// -> DESC="midi keyboard split"
//                            Sleep(1);
                Sleep(0);
                PostMessage(WM_MOD_VIEWMSG, VIEWMSG_DOMIDISPACING, lParam);
            }
        } else
        {
            m_dwStatus &= ~PATSTATUS_MIDISPACINGPENDING;
        }
        break;

    case VIEWMSG_LOADSTATE:
        if (lParam)
        {
            PATTERNVIEWSTATE *pState = (PATTERNVIEWSTATE *)lParam;
            if (pState->nDetailLevel) m_nDetailLevel = pState->nDetailLevel;
            if (/*(pState->nPattern == m_nPattern) && */(pState->cbStruct == sizeof(PATTERNVIEWSTATE)))
            {
                SetCurrentPattern(pState->nPattern);
                // Fix: Horizontal scrollbar pos screwed when selecting with mouse
                SetCursorPosition( pState->nRow, pState->nCursor );
                SetCurSel(pState->dwBeginSel, pState->dwEndSel);
            }
        }
        break;

    case VIEWMSG_SAVESTATE:
        if (lParam)
        {
            PATTERNVIEWSTATE *pState = (PATTERNVIEWSTATE *)lParam;
            pState->cbStruct = sizeof(PATTERNVIEWSTATE);
            pState->nPattern = m_nPattern;
            pState->nRow = m_nRow;
            pState->nCursor = m_dwCursor;
            pState->dwBeginSel = m_dwBeginSel;
            pState->dwEndSel = m_dwEndSel;
            pState->nDetailLevel = m_nDetailLevel;
            pState->nOrder = SendCtrlMessage(CTRLMSG_GETCURRENTORDER); //rewbs.playSongFromCursor
        }
        break;

    case VIEWMSG_EXPANDPATTERN:
        {
            CModDoc *pModDoc = GetDocument();
            if (pModDoc->ExpandPattern(m_nPattern)) { m_nRow *= 2; SetCurrentPattern(m_nPattern); }
        }
        break;

    case VIEWMSG_SHRINKPATTERN:
        {
            CModDoc *pModDoc = GetDocument();
            if (pModDoc->ShrinkPattern(m_nPattern)) { m_nRow /= 2; SetCurrentPattern(m_nPattern); }
        }
        break;

    case VIEWMSG_COPYPATTERN:
        {
            CModDoc *pModDoc = GetDocument();
            if (pModDoc)
            {
                module_renderer *pSndFile = pModDoc->GetSoundFile();
                pModDoc->CopyPattern(m_nPattern, 0, ((pSndFile->Patterns[m_nPattern].GetNumRows()-1)<<16)|(pSndFile->m_nChannels<<3));
            }
        }
        break;

    case VIEWMSG_PASTEPATTERN:
        {
            CModDoc *pModDoc = GetDocument();
            if (pModDoc) pModDoc->PastePattern(m_nPattern, 0, pm_overwrite);
            InvalidatePattern();
        }
        break;

    case VIEWMSG_AMPLIFYPATTERN:
        OnPatternAmplify();
        break;

    case VIEWMSG_SETDETAIL:
        if (lParam != (LONG)m_nDetailLevel)
        {
            m_nDetailLevel = lParam;
            UpdateSizes();
            UpdateScrollSize();
            SetCurrentColumn(m_dwCursor);
            InvalidatePattern(TRUE);
        }
        break;
    case VIEWMSG_DOSCROLL:
        {
            CPoint p(0,0);  //dummy point;
            CModScrollView::OnMouseWheel(0, lParam, p);
        }
        break;


    default:
        return CModScrollView::OnModViewMsg(wParam, lParam);
    }
    return 0;
}

//rewbs.customKeys
void CViewPattern::CursorJump(uint32_t distance, bool direction, bool snap)
//-----------------------------------------------------------------------
{                                                                                      //up is true
    switch(snap)
    {
        case false: //no snap
            SetCurrentRow(m_nRow + ((int)(direction?-1:1))*distance, TRUE);
            break;

        case true: //snap
            SetCurrentRow((((m_nRow+(int)(direction?-1:0))/distance)+(int)(direction?0:1))*distance, TRUE);
            break;
    }

}


#define ENTER_PCNOTE_VALUE(v, method) \
    { \
        if((v >= 0) && (v <= 9)) \
        {    \
            uint16_t val = p->Get##method##(); \
            /* Move existing digits to left, drop out leftmost digit and */ \
            /* push new digit to the least meaning digit. */ \
            val = (val % 100) * 10 + v; \
            if(val > modplug::tracker::modevent_t::MaxColumnValue) val = modplug::tracker::modevent_t::MaxColumnValue; \
            p->Set##method##(val); \
        } \
    }


// Enter volume effect / number in the pattern.
void CViewPattern::TempEnterVol(int v)
//------------------------------------
{
    CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
    CModDoc *pModDoc = GetDocument();

    if ((pModDoc) && (pMainFrm))
    {

        module_renderer *pSndFile = pModDoc->GetSoundFile();

        PrepareUndo(m_dwBeginSel, m_dwEndSel);

        modplug::tracker::modevent_t* p = pSndFile->Patterns[m_nPattern].GetpModCommand(m_nRow, GetChanFromCursor(m_dwCursor));
        modplug::tracker::modevent_t oldcmd = *p; // This is the command we are about to overwrite

        if(p->IsPcNote())
        {
            ENTER_PCNOTE_VALUE(v, ValueVolCol);
        }
        else
        {
            modplug::tracker::volcmd_t volcmd = p->volcmd;
            UINT vol = p->vol;
            if ((v >= 0) && (v <= 9))
            {
                vol = ((vol * 10) + v) % 100;
                if (!volcmd) volcmd = VolCmdVol;
            }

            //if ((pSndFile->m_nType & MOD_TYPE_MOD) && (volcmd > VolCmdPan)) volcmd = vol = 0;

            UINT bad_max = 64;
            if (volcmd > VolCmdPan)
            {
                bad_max = (pSndFile->m_nType == MOD_TYPE_XM) ? 0x0F : 9;
            }

            if (vol > bad_max) vol %= 10;
            if(pSndFile->GetModSpecifications().HasVolCommand(volcmd))
            {
                p->volcmd = volcmd;
                p->vol = vol;
            }
        }

        if (IsEditingEnabled_bmsg())
        {
            uint32_t sel = CreateCursor(m_nRow) | m_dwCursor;
            SetCurSel(sel, sel);
            sel &= ~7;
            if(oldcmd != *p)
            {
                pModDoc->SetModified();
                InvalidateArea(sel, sel + LAST_COLUMN);
                UpdateIndicator();
            }
        }
        else
        {
            // recording disabled
            *p = oldcmd;
        }
    }
}

void CViewPattern::SetSpacing(int n)
//----------------------------------
{
    if (n != m_nSpacing)
    {
        m_nSpacing = n;
        PostCtrlMessage(CTRLMSG_SETSPACING, m_nSpacing);
    }
}


// Enter an effect letter in the pattenr
void CViewPattern::TempEnterFX(int raw_c, int v)
//------------------------------------------
{
    CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
    CModDoc *pModDoc = GetDocument();

    //XXXih: gross
    auto c = (modplug::tracker::cmd_t) raw_c;

    if(!IsEditingEnabled_bmsg()) return;

    if ((pModDoc) && (pMainFrm))
    {
        module_renderer *pSndFile = pModDoc->GetSoundFile();

        modplug::tracker::modevent_t *p = pSndFile->Patterns[m_nPattern].GetpModCommand(m_nRow, GetChanFromCursor(m_dwCursor));
        modplug::tracker::modevent_t oldcmd = *p; // This is the command we are about to overwrite

        PrepareUndo(m_dwBeginSel, m_dwEndSel);

        if(p->IsPcNote())
        {
            ENTER_PCNOTE_VALUE(c, ValueEffectCol);
        }
        else if(pSndFile->GetModSpecifications().HasCommand(c))
        {

            //LPCSTR lpcmd = (pSndFile->m_nType & (MOD_TYPE_MOD|MOD_TYPE_XM)) ? gszModCommands : gszS3mCommands;
            if (c)
            {
                if ((c == m_cmdOld.command) && (!p->param) && (!p->command)) p->param = m_cmdOld.param;
                else m_cmdOld.param = 0;
                m_cmdOld.command = c;
            }
            p->command = c;
            if(v >= 0)
            {
                p->param = v;
            }

            // Check for MOD/XM Speed/Tempo command
            if ((pSndFile->m_nType & (MOD_TYPE_MOD|MOD_TYPE_XM))
            && ((p->command == CmdSpeed) || (p->command == CmdTempo)))
            {
                p->command = (p->param <= pSndFile->GetModSpecifications().speedMax) ? CmdSpeed : CmdTempo;
            }
        }

        uint32_t sel = CreateCursor(m_nRow) | m_dwCursor;
        SetCurSel(sel, sel);
        sel &= ~7;
        if(oldcmd != *p)
        {
            pModDoc->SetModified();
            InvalidateArea(sel, sel + LAST_COLUMN);
            UpdateIndicator();
        }
    }    // end if mainframe & moddoc exist
}


// Enter an effect param in the pattenr
void CViewPattern::TempEnterFXparam(int v)
//----------------------------------------
{
    CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
    CModDoc *pModDoc = GetDocument();

    if ((pModDoc) && (pMainFrm) && (IsEditingEnabled_bmsg()))
    {
        module_renderer *pSndFile = pModDoc->GetSoundFile();

        modplug::tracker::modevent_t *p = pSndFile->Patterns[m_nPattern].GetpModCommand(m_nRow, GetChanFromCursor(m_dwCursor));
        modplug::tracker::modevent_t oldcmd = *p; // This is the command we are about to overwrite

        PrepareUndo(m_dwBeginSel, m_dwEndSel);

        if(p->IsPcNote())
        {
            ENTER_PCNOTE_VALUE(v, ValueEffectCol);
        }
        else
        {

            p->param = (p->param << 4) | v;
            if (p->command == m_cmdOld.command) m_cmdOld.param = p->param;

            // Check for MOD/XM Speed/Tempo command
            if ((pSndFile->m_nType & (MOD_TYPE_MOD|MOD_TYPE_XM))
             && ((p->command == CmdSpeed) || (p->command == CmdTempo)))
            {
                p->command = (p->param <= pSndFile->GetModSpecifications().speedMax) ? CmdSpeed : CmdTempo;
            }
        }

        uint32_t sel = CreateCursor(m_nRow) | m_dwCursor;
        SetCurSel(sel, sel);
        sel &= ~7;
        if(*p != oldcmd)
        {
            pModDoc->SetModified();
            InvalidateArea(sel, sel + LAST_COLUMN);
            UpdateIndicator();
        }
    }
}


// Stop a note that has been entered
void CViewPattern::TempStopNote(int note, bool fromMidi, const bool bChordMode)
//-----------------------------------------------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    module_renderer *pSndFile = pModDoc->GetSoundFile();
    CMainFrame *pMainFrm = CMainFrame::GetMainFrame();

    // Get playback edit positions from play engine here in case they are needed below.
    const modplug::tracker::rowindex_t nRowPlayback = pSndFile->m_nRow;
    const UINT nTick = pSndFile->m_nTickCount;
    const modplug::tracker::patternindex_t nPatPlayback = pSndFile->m_nPattern;

    const bool isSplit = (pModDoc->GetSplitKeyboardSettings()->IsSplitActive()) && (note <= pModDoc->GetSplitKeyboardSettings()->splitNote);
    UINT ins = 0;
    if (pModDoc)
    {
        if (isSplit)
        {
            ins = pModDoc->GetSplitKeyboardSettings()->splitInstrument;
            if (pModDoc->GetSplitKeyboardSettings()->octaveLink) note += 12 *pModDoc->GetSplitKeyboardSettings()->octaveModifier;
            if (note > NoteMax && note < NoteMinSpecial) note = NoteMax;
            if (note < 0) note = 1;
        }
        if (!ins)    ins = GetCurrentInstrument();
        if (!ins)     ins = m_nFoundInstrument;
        if(bChordMode == true)
        {
            m_dwStatus &= ~PATSTATUS_CHORDPLAYING;
            pModDoc->NoteOff(0, TRUE, ins, m_dwCursor & 0xFFFF);
        }
        else
        {
            pModDoc->NoteOff(note, ((CMainFrame::m_dwPatternSetup & PATTERN_NOTEFADE) || pSndFile->GetNumInstruments() == 0) ? TRUE : FALSE, ins, GetChanFromCursor(m_dwCursor));
        }
    }

    //Enter note off in pattern?
    if ((note < NoteMin) || (note > NoteMax))
        return;
    if (GetColTypeFromCursor(m_dwCursor) > INST_COLUMN && (bChordMode || !fromMidi))
        return;
    if (!pModDoc || !pMainFrm || !(IsEditingEnabled()))
        return;

    const bool bIsLiveRecord = IsLiveRecord(*pMainFrm, *pModDoc, *pSndFile);

    const modplug::tracker::chnindex_t nChnCursor = GetChanFromCursor(m_dwCursor);

    uint8_t* activeNoteMap = isSplit ? splitActiveNoteChannel : activeNoteChannel;
    const modplug::tracker::chnindex_t nChn = (activeNoteMap[note] < pSndFile->GetNumChannels()) ? activeNoteMap[note] : nChnCursor;

    activeNoteMap[note] = 0xFF;    //unlock channel

    if (!((CMainFrame::m_dwPatternSetup & PATTERN_KBDNOTEOFF) || fromMidi))
    {
        // We don't want to write the note-off into the pattern if this feature is disabled and we're not recording from MIDI.
        return;
    }

    // -- write sdx if playing live
    const bool usePlaybackPosition = (!bChordMode) && (bIsLiveRecord && (CMainFrame::m_dwPatternSetup & PATTERN_AUTODELAY));

    //Work out where to put the note off
    modplug::tracker::rowindex_t nRow = m_nRow;
    modplug::tracker::patternindex_t nPat = m_nPattern;

    if(usePlaybackPosition)
        SetEditPos(*pSndFile, nRow, nPat, nRowPlayback, nPatPlayback);

    modplug::tracker::modevent_t* p = pSndFile->Patterns[nPat].GetpModCommand(nRow, nChn);

    //don't overwrite:
    if (p->note || p->instr || p->volcmd)
    {
        //if there's a note in the current location and the song is playing and following,
        //the user probably just tapped the key - let's try the next row down.
        nRow++;
        if (p->note==note && bIsLiveRecord && pSndFile->Patterns[nPat].IsValidRow(nRow))
        {
            p = pSndFile->Patterns[nPat].GetpModCommand(nRow, nChn);
            if (p->note || (!bChordMode && (p->instr || p->volcmd)) )
                return;
        }
        else
            return;
    }

    // Create undo-point.
    //pModDoc->GetPatternUndo()->PrepareUndo(nPat, nChn, nRow, 1, 1);

    // -- write sdx if playing live
    if (usePlaybackPosition && nTick) {    // avoid SD0 which will be mis-interpreted
        if (p->command == 0) {    //make sure we don't overwrite any existing commands.
            p->command = (pSndFile->TypeIsS3M_IT_MPT()) ? CmdS3mCmdEx : CmdModCmdEx;
            p->param   = 0xD0 | bad_min(0xF, nTick);
        }
    }

    //Enter note off
    if(pModDoc->GetSoundFile()->GetModSpecifications().hasNoteOff) // ===
        p->note = NoteKeyOff;
    else if(pModDoc->GetSoundFile()->GetModSpecifications().hasNoteCut) // ^^^
        p->note = NoteNoteCut;
    else { // we don't have anything to cut (MOD format) - use volume or ECx
        if(usePlaybackPosition && nTick) // ECx
        {
            p->command = (pSndFile->TypeIsS3M_IT_MPT()) ? CmdS3mCmdEx : CmdModCmdEx;
            p->param   = 0xC0 | bad_min(0xF, nTick);
        } else // C00
        {
            p->note = NoteNone;
            p->command = CmdVol;
            p->param = 0;
        }
    }
    p->instr = (bChordMode) ? 0 : ins; //p->instr = 0;
    //Writing the instrument as well - probably someone finds this annoying :)
    p->volcmd = VolCmdNone;
    p->vol    = 0;

    pModDoc->SetModified();

    // Update only if not recording live.
    if(bIsLiveRecord == false)
    {
        uint32_t sel = CreateCursor(nRow, nChn, 0);
        InvalidateArea(sel, sel + LAST_COLUMN);
        UpdateIndicator();
    }

    return;

}


// Enter an octave number in the pattern
void CViewPattern::TempEnterOctave(int val)
//-----------------------------------------
{
    CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
    CModDoc *pModDoc = GetDocument();

    if ((pModDoc) && (pMainFrm))
    {

        module_renderer *pSndFile = pModDoc->GetSoundFile();
        UINT nChn = GetChanFromCursor(m_dwCursor);
        PrepareUndo(m_dwBeginSel, m_dwEndSel);

        modplug::tracker::modevent_t* p = pSndFile->Patterns[m_nPattern].GetpModCommand(m_nRow, nChn);
        modplug::tracker::modevent_t oldcmd = *p; // This is the command we are about to overwrite
        if (oldcmd.note)
            TempEnterNote(((oldcmd.note-1)%12)+val*12+1);
    }

}


// Enter an instrument number in the pattern
void CViewPattern::TempEnterIns(int val)
//--------------------------------------
{
    CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
    CModDoc *pModDoc = GetDocument();

    if ((pModDoc) && (pMainFrm))
    {

        module_renderer *pSndFile = pModDoc->GetSoundFile();

        UINT nChn = GetChanFromCursor(m_dwCursor);
        PrepareUndo(m_dwBeginSel, m_dwEndSel);

        modplug::tracker::modevent_t *p = pSndFile->Patterns[m_nPattern].GetpModCommand(m_nRow, nChn);
        modplug::tracker::modevent_t oldcmd = *p;            // This is the command we are about to overwrite

        UINT instr = p->instr, nTotalMax, nTempMax;
        if(p->IsPcNote())    // this is a plugin index
        {
            nTotalMax = MAX_MIXPLUGINS + 1;
            nTempMax = MAX_MIXPLUGINS + 1;
        } else if(pSndFile->GetNumInstruments() > 0)    // this is an instrument index
        {
            nTotalMax = MAX_INSTRUMENTS;
            nTempMax = pSndFile->GetNumInstruments();
        } else
        {
            nTotalMax = MAX_SAMPLES;
            nTempMax = pSndFile->GetNumSamples();
        }

        instr = ((instr * 10) + val) % 1000;
        if (instr >= nTotalMax) instr = instr % 100;
        if (nTempMax < 100)                    // if we're using samples & have less than 100 samples
            instr = instr % 100;    // or if we're using instruments and have less than 100 instruments
                                    // --> ensure the entered instrument value is less than 100.
        p->instr = instr;

        if (IsEditingEnabled_bmsg())
        {
            uint32_t sel = CreateCursor(m_nRow) | m_dwCursor;
            SetCurSel(sel, sel);
            sel &= ~7;
            if(*p != oldcmd)
            {
                pModDoc->SetModified();
                InvalidateArea(sel, sel + LAST_COLUMN);
                UpdateIndicator();
            }
        }
        else
        {
            // recording disabled
            *p = oldcmd;
        }
    }
}


// Enter a note in the pattern
void CViewPattern::TempEnterNote(int note, bool oldStyle, int vol)
//----------------------------------------------------------------
{
    CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
    CModDoc *pModDoc = GetDocument();

    if ((pModDoc) && (pMainFrm))
    {
        bool modified = false;
        modplug::tracker::rowindex_t nRow = m_nRow;
        module_renderer *pSndFile = pModDoc->GetSoundFile();
        const modplug::tracker::rowindex_t nRowPlayback = pSndFile->m_nRow;
        const UINT nTick = pSndFile->m_nTickCount;
        const modplug::tracker::patternindex_t nPatPlayback = pSndFile->m_nPattern;

        const bool bRecordEnabled = IsEditingEnabled();
        const UINT nChn = GetChanFromCursor(m_dwCursor);

        if(note > pSndFile->GetModSpecifications().noteMax && note < NoteMinSpecial)
            note = pSndFile->GetModSpecifications().noteMax;
        else if( note < pSndFile->GetModSpecifications().noteMin)
            note = pSndFile->GetModSpecifications().noteMin;

        // Special case: Convert note off commands to C00 for MOD files
        if((pSndFile->GetType() == MOD_TYPE_MOD) && (note == NoteNoteCut || note == NoteFade || note == NoteKeyOff))
        {
            TempEnterFX(CmdVol, 0);
            return;
        }

        // Check whether the module format supports the note.
        if( pSndFile->GetModSpecifications().HasNote(note) == false )
            return;

        uint8_t recordGroup = pModDoc->IsChannelRecord(nChn);
        const bool bIsLiveRecord = IsLiveRecord(*pMainFrm, *pModDoc, *pSndFile);
        const bool usePlaybackPosition = (bIsLiveRecord && (CMainFrame::m_dwPatternSetup & PATTERN_AUTODELAY) && !(pSndFile->m_dwSongFlags & SONG_STEP));
        //Param control 'note'
        if(modplug::tracker::modevent_t::IsPcNote(note) && bRecordEnabled)
        {
            //pModDoc->GetPatternUndo()->PrepareUndo(m_nPattern, nChn, nRow, 1, 1);
            pSndFile->Patterns[m_nPattern].GetpModCommand(nRow, nChn)->note = note;
            const uint32_t sel = CreateCursor(nRow) | m_dwCursor;
            pModDoc->SetModified();
            InvalidateArea(sel, sel + LAST_COLUMN);
            UpdateIndicator();
            return;
        }


        // -- Chord autodetection: step back if we just entered a note
        if ((bRecordEnabled) && (recordGroup) && !bIsLiveRecord)
        {
            if ((m_nSpacing > 0) && (m_nSpacing <= MAX_SPACING))
            {
                if ((timeGetTime() - m_dwLastNoteEntryTime < CMainFrame::gnAutoChordWaitTime)
                    && (nRow >= m_nSpacing) && (!m_bLastNoteEntryBlocked))
                {
                    nRow -= m_nSpacing;
                }
            }
        }
        m_dwLastNoteEntryTime = timeGetTime();

        modplug::tracker::patternindex_t nPat = m_nPattern;

        if(usePlaybackPosition)
            SetEditPos(*pSndFile, nRow, nPat, nRowPlayback, nPatPlayback);

        // -- Work out where to put the new note
        modplug::tracker::modevent_t *pTarget = pSndFile->Patterns[nPat].GetpModCommand(nRow, nChn);
        modplug::tracker::modevent_t newcmd = *pTarget;

        // If record is enabled, create undo point.
        if(bRecordEnabled)
        {
            //pModDoc->GetPatternUndo()->PrepareUndo(nPat, nChn, nRow, 1, 1);
        }

        // We're overwriting a PC note here.
        if(pTarget->IsPcNote() && !modplug::tracker::modevent_t::IsPcNote(note))
        {
            newcmd.Clear();
        }

        // -- write note and instrument data.
        const bool isSplit = HandleSplit(&newcmd, note);

        // Nice idea actually: Use lower section of the keyboard to play chords (but it won't work 100% correctly this way...)
        /*if(isSplit)
        {
            TempEnterChord(note);
            return;
        }*/

        // -- write vol data
        int volWrite = -1;
        if (vol >= 0 && vol <= 64 && !(isSplit && pModDoc->GetSplitKeyboardSettings()->splitVolume))    //write valid volume, as long as there's no split volume override.
        {
            volWrite = vol;
        } else if (isSplit && pModDoc->GetSplitKeyboardSettings()->splitVolume)    //cater for split volume override.
        {
            if (pModDoc->GetSplitKeyboardSettings()->splitVolume > 0 && pModDoc->GetSplitKeyboardSettings()->splitVolume <= 64)
            {
                volWrite = pModDoc->GetSplitKeyboardSettings()->splitVolume;
            }
        }

        if(volWrite != -1)
        {
            if(pSndFile->GetModSpecifications().HasVolCommand(VolCmdVol))
            {
                newcmd.volcmd = VolCmdVol;
                newcmd.vol = (modplug::tracker::vol_t)volWrite;
            } else
            {
                newcmd.command = CmdVol;
                newcmd.param = (modplug::tracker::param_t)volWrite;
            }
        }

        // -- write sdx if playing live
        if (usePlaybackPosition && nTick)    // avoid SD0 which will be mis-interpreted
        {
            if (newcmd.command == CmdNone)    //make sure we don't overwrite any existing commands.
            {
                newcmd.command = (pSndFile->TypeIsS3M_IT_MPT()) ? CmdS3mCmdEx : CmdModCmdEx;
                UINT maxSpeed = 0x0F;
                if(pSndFile->m_nMusicSpeed > 0) maxSpeed = bad_min(0x0F, pSndFile->m_nMusicSpeed - 1);
                newcmd.param = 0xD0 + bad_min(maxSpeed, nTick);
            }
        }

        // -- old style note cut/off/fade: erase instrument number
        if (oldStyle && newcmd.note >= NoteMinSpecial)
            newcmd.instr = 0;


        // -- if recording, write out modified command.
        if (bRecordEnabled && *pTarget != newcmd)
        {
            *pTarget = newcmd;
            modified = true;
        }


        // -- play note
        if ((CMainFrame::m_dwPatternSetup & (PATTERN_PLAYNEWNOTE|PATTERN_PLAYEDITROW)) || (!bRecordEnabled))
        {
            const bool playWholeRow = ((CMainFrame::m_dwPatternSetup & PATTERN_PLAYEDITROW) && !bIsLiveRecord);
            if (playWholeRow)
            {
                // play the whole row in "step mode"
                PatternStep(false);
            }
            if (!playWholeRow || !bRecordEnabled)
            {
                // NOTE: This code is *also* used for the PATTERN_PLAYEDITROW edit mode because of some unforseeable race conditions when modifying pattern data.
                // We have to use this code when editing is disabled or else we will get some stupid hazards, because we would first have to write the new note
                // data to the pattern and then remove it again - but often, it is actually removed before the row is parsed by the soundlib.

                // just play the newly inserted note using the already specified instrument...
                UINT nPlayIns = newcmd.instr;
                if(!nPlayIns && (note <= NoteMax))
                {
                    // ...or one that can be found on a previous row of this pattern.
                    modplug::tracker::modevent_t *search = pTarget;
                    UINT srow = nRow;
                    while (srow-- > 0)
                    {
                        search -= pSndFile->m_nChannels;
                        if (search->instr)
                        {
                            nPlayIns = search->instr;
                            m_nFoundInstrument = nPlayIns;  //used to figure out which instrument to stop on key release.
                            break;
                        }
                    }
                }
                BOOL bNotPlaying = ((pMainFrm->GetModPlaying() == pModDoc) && (pMainFrm->IsPlaying())) ? FALSE : TRUE;
                //pModDoc->PlayNote(p->note, nPlayIns, 0, bNotPlaying, -1, 0, 0, nChn);    //rewbs.vstiLive - added extra args
                pModDoc->PlayNote(newcmd.note, nPlayIns, 0, bNotPlaying, 4*vol, 0, 0, nChn);    //rewbs.vstiLive - added extra args
            }
        }


        // -- if recording, handle post note entry behaviour (move cursor etc..)
        if(bRecordEnabled)
        {
            uint32_t sel = CreateCursor(nRow) | m_dwCursor;
            if(bIsLiveRecord == false)
            {   // Update only when not recording live.
                SetCurSel(sel, sel);
                sel &= ~7;
            }

            if(modified) //has it really changed?
            {
                pModDoc->SetModified();
                if(bIsLiveRecord == false)
                {   // Update only when not recording live.
                    InvalidateArea(sel, sel + LAST_COLUMN);
                    UpdateIndicator();
                }
            }

            // Set new cursor position (row spacing)
            if (!bIsLiveRecord)
            {
                if((m_nSpacing > 0) && (m_nSpacing <= MAX_SPACING))
                {
                    if(nRow + m_nSpacing < pSndFile->Patterns[nPat].GetNumRows() || (CMainFrame::m_dwPatternSetup & PATTERN_CONTSCROLL))
                    {
                        SetCurrentRow(nRow + m_nSpacing, (CMainFrame::m_dwPatternSetup & PATTERN_CONTSCROLL) ? true: false);
                        m_bLastNoteEntryBlocked = false;
                    } else
                    {
                        m_bLastNoteEntryBlocked = true;  // if the cursor is block by the end of the pattern here,
                    }                                                                 // we must remember to not step back should the next note form a chord.

                }
                uint32_t sel = CreateCursor(m_nRow) | m_dwCursor;
                SetCurSel(sel, sel);
            }

            uint8_t* activeNoteMap = isSplit ? splitActiveNoteChannel : activeNoteChannel;
            if (newcmd.note <= NoteMax)
                activeNoteMap[newcmd.note] = nChn;

            //Move to next channel if required
            if (recordGroup)
            {
                bool channelLocked;

                UINT n = nChn;
                for (UINT i=1; i<pSndFile->m_nChannels; i++)
                {
                    if (++n >= pSndFile->m_nChannels) n = 0; //loop around

                    channelLocked = false;
                    for (int k=0; k<NoteMax; k++)
                    {
                        if (activeNoteChannel[k]==n  || splitActiveNoteChannel[k]==n)
                        {
                            channelLocked = true;
                            break;
                        }
                    }

                    if (pModDoc->IsChannelRecord(n)==recordGroup && !channelLocked)
                    {
                        SetCurrentColumn(n<<3);
                        break;
                    }
                }
            }
        }
    }

}


// Enter a chord in the pattern
void CViewPattern::TempEnterChord(int note)
//-----------------------------------------
{
    CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
    CModDoc *pModDoc = GetDocument();
    UINT nPlayChord = 0;
    uint8_t chordplaylist[3];

    if ((pModDoc) && (pMainFrm))
    {
        module_renderer *pSndFile = pModDoc->GetSoundFile();
        const modplug::tracker::chnindex_t nChn = GetChanFromCursor(m_dwCursor);
        UINT nPlayIns = 0;
        // Simply backup the whole row.
        //pModDoc->GetPatternUndo()->PrepareUndo(m_nPattern, nChn, m_nRow, pSndFile->GetNumChannels(), 1);

        const PatternRow prowbase = pSndFile->Patterns[m_nPattern].GetRow(m_nRow);
        modplug::tracker::modevent_t* pTarget = &prowbase[nChn];
        // Save old row contents
        vector<modplug::tracker::modevent_t> newrow(pSndFile->GetNumChannels());
        for(modplug::tracker::chnindex_t n = 0; n < pSndFile->GetNumChannels(); n++)
        {
            newrow[n] = prowbase[n];
        }
        // This is being written to
        modplug::tracker::modevent_t* p = &newrow[nChn];


        const bool bIsLiveRecord = IsLiveRecord(*pMainFrm, *pModDoc, *pSndFile);
        const bool bRecordEnabled = IsEditingEnabled();

        // -- establish note data
        //const bool isSplit = HandleSplit(p, note);
        HandleSplit(p, note);

        PMPTCHORD pChords = pMainFrm->GetChords();
        UINT baseoctave = pMainFrm->GetBaseOctave();
        UINT nchord = note - baseoctave * 12 - 1;
        UINT nNote;
        bool modified = false;
        if (nchord < 3 * 12)
        {
            UINT nchordnote = pChords[nchord].key + baseoctave * 12 + 1;
            // Rev. 244, commented the isSplit conditions below, can't see the point in it.
            //if (isSplit)
            //    nchordnote = pChords[nchord].key + baseoctave*(p->note%12) + 1;
            //else
            //    nchordnote = pChords[nchord].key + baseoctave*12 + 1;
            if (nchordnote <= NoteMax)
            {
                UINT nchordch = nChn, nchno = 0;
                nNote = nchordnote;
                p->note = nNote;
                uint8_t recordGroup, currentRecordGroup = 0;

                recordGroup = pModDoc->IsChannelRecord(nChn);

                for (UINT kchrd=1; kchrd<pSndFile->m_nChannels; kchrd++)
                {
                    if ((nchno > 2) || (!pChords[nchord].notes[nchno])) break;
                    if (++nchordch >= pSndFile->m_nChannels) nchordch = 0;

                    currentRecordGroup = pModDoc->IsChannelRecord(nchordch);
                    if (!recordGroup)
                        recordGroup = currentRecordGroup;  //record group found

                    UINT n = ((nchordnote-1)/12) * 12 + pChords[nchord].notes[nchno];
                    if(bRecordEnabled)
                    {
                        if ((nchordch != nChn) && recordGroup && (currentRecordGroup == recordGroup) && (n <= NoteMax))
                        {
                            newrow[nchordch].note = n;
                            if (p->instr) newrow[nchordch].instr = p->instr;
                            if(prowbase[nchordch] != newrow[nchordch])
                            {
                                modified = true;
                            }

                            nchno++;
                            if (CMainFrame::m_dwPatternSetup & PATTERN_PLAYNEWNOTE)
                            {
                                if ((n) && (n <= NoteMax)) chordplaylist[nPlayChord++] = n;
                            }
                        }
                    } else
                    {
                        nchno++;
                        if ((n) && (n <= NoteMax)) chordplaylist[nPlayChord++] = n;
                    }
                }
            }
        }


        // -- write notedata
        if(bRecordEnabled)
        {
            uint32_t sel = CreateCursor(m_nRow) | m_dwCursor;
            SetCurSel(sel, sel);
            sel = GetRowFromCursor(sel) | GetChanFromCursor(sel);
            if(modified)
            {
                for(modplug::tracker::chnindex_t n = 0; n < pSndFile->GetNumChannels(); n++)
                {
                    prowbase[n] = newrow[n];
                }
                pModDoc->SetModified();
                InvalidateRow();
                UpdateIndicator();
            }
        }


        // -- play note
        if ((CMainFrame::m_dwPatternSetup & (PATTERN_PLAYNEWNOTE|PATTERN_PLAYEDITROW)) || (!bRecordEnabled))
        {
            const bool playWholeRow = ((CMainFrame::m_dwPatternSetup & PATTERN_PLAYEDITROW) && !bIsLiveRecord);
            if (playWholeRow)
            {
                // play the whole row in "step mode"
                PatternStep(false);
            }
            if (!playWholeRow || !bRecordEnabled)
            {
                // NOTE: This code is *also* used for the PATTERN_PLAYEDITROW edit mode because of some unforseeable race conditions when modifying pattern data.
                // We have to use this code when editing is disabled or else we will get some stupid hazards, because we would first have to write the new note
                // data to the pattern and then remove it again - but often, it is actually removed before the row is parsed by the soundlib.
                // just play the newly inserted notes...

                if (p->instr)
                {
                    // ...using the already specified instrument
                    nPlayIns = p->instr;
                } else if ((!p->instr) && (p->note <= NoteMax))
                {
                    // ...or one that can be found on a previous row of this pattern.
                    modplug::tracker::modevent_t *search = pTarget;
                    UINT srow = m_nRow;
                    while (srow-- > 0)
                    {
                        search -= pSndFile->m_nChannels;
                        if (search->instr)
                        {
                            nPlayIns = search->instr;
                            m_nFoundInstrument = nPlayIns;  //used to figure out which instrument to stop on key release.
                            break;
                        }
                    }
                }
                BOOL bNotPlaying = ((pMainFrm->GetModPlaying() == pModDoc) && (pMainFrm->IsPlaying())) ? FALSE : TRUE;
                pModDoc->PlayNote(p->note, nPlayIns, 0, bNotPlaying, -1, 0, 0, nChn);    //rewbs.vstiLive - added extra args
                for (UINT kplchrd=0; kplchrd<nPlayChord; kplchrd++)
                {
                    if (chordplaylist[kplchrd])
                    {
                        pModDoc->PlayNote(chordplaylist[kplchrd], nPlayIns, 0, FALSE, -1, 0, 0, nChn);    //rewbs.vstiLive -         - added extra args
                        m_dwStatus |= PATSTATUS_CHORDPLAYING;
                    }
                }
            }
        } // end play note


        // Set new cursor position (row spacing) - only when not recording live
        if (bRecordEnabled && !bIsLiveRecord)
        {
            if ((m_nSpacing > 0) && (m_nSpacing <= MAX_SPACING))
                SetCurrentRow(m_nRow + m_nSpacing);
            uint32_t sel = CreateCursor(m_nRow) | m_dwCursor;
            SetCurSel(sel, sel);
        }

    } // end mainframe and moddoc exist
}


void CViewPattern::OnClearField(int field, bool step, bool ITStyle)
//-----------------------------------------------------------------
{
    CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
    CModDoc *pModDoc = GetDocument();

    if ((pModDoc) && (pMainFrm))
    {
        //if we have a selection we want to do something different
        if (m_dwBeginSel != m_dwEndSel)
        {
            OnClearSelection(ITStyle);
            return;
        }

        PrepareUndo(m_dwBeginSel, m_dwEndSel);
        UINT nChn = GetChanFromCursor(m_dwCursor);
        module_renderer *pSndFile = pModDoc->GetSoundFile();
        modplug::tracker::modevent_t *p = pSndFile->Patterns[m_nPattern] +  m_nRow*pSndFile->m_nChannels + nChn;
        modplug::tracker::modevent_t oldcmd = *p;

        switch(field)
        {
            case NOTE_COLUMN:    if(p->IsPcNote()) p->Clear(); else {p->note = NoteNone; if (ITStyle) p->instr = 0;}  break;                //Note
            case INST_COLUMN:    p->instr = 0; break;                                //instr
            case VOL_COLUMN:    p->vol = 0; p->volcmd = VolCmdNone; break;        //Vol
            case EFFECT_COLUMN:    p->command = CmdNone;        break;                                //Effect
            case PARAM_COLUMN:    p->param = 0; break;                                //Param
            default:                    p->Clear();                                                        //If not specified, delete them all! :)
        }
        if((field == 3 || field == 4) && (p->IsPcNote()))
            p->SetValueEffectCol(0);

        if(IsEditingEnabled_bmsg())
        {
            uint32_t sel = CreateCursor(m_nRow) | m_dwCursor;
            SetCurSel(sel, sel);
            sel = GetRowFromCursor(sel) | GetChanFromCursor(sel);
            if(*p != oldcmd)
            {
                pModDoc->SetModified();
                InvalidateRow();
                UpdateIndicator();
            }
            if (step && (((pMainFrm->GetFollowSong(pModDoc) != m_hWnd) || (pSndFile->IsPaused())
             || (!(m_dwStatus & PATSTATUS_FOLLOWSONG)))))
            {
                if ((m_nSpacing > 0) && (m_nSpacing <= MAX_SPACING))
                    SetCurrentRow(m_nRow+m_nSpacing);
                uint32_t sel = CreateCursor(m_nRow) | m_dwCursor;
                SetCurSel(sel, sel);
            }
        } else
        {
            *p = oldcmd;
        }
    }
}



void CViewPattern::OnInitMenu(CMenu* pMenu)
//-----------------------------------------
{
    CModScrollView::OnInitMenu(pMenu);

    //rewbs: ensure modifiers are reset when we go into menu
/*    m_dwStatus &= ~PATSTATUS_KEYDRAGSEL;
    m_dwStatus &= ~PATSTATUS_CTRLDRAGSEL;
    CMainFrame::GetMainFrame()->GetInputHandler()->SetModifierMask(0);
*/    //end rewbs

}

void CViewPattern::TogglePluginEditor(int chan)
//---------------------------------------------
{
    CModDoc *pModDoc = GetDocument(); if (!pModDoc) return;
    module_renderer *pSndFile = pModDoc->GetSoundFile(); if (!pSndFile) return;

    int plug = pSndFile->ChnSettings[chan].nMixPlugin;
    if (plug > 0)
        pModDoc->TogglePluginEditor(plug-1);

    return;
}


void CViewPattern::OnSelectInstrument(UINT nID)
//---------------------------------------------
{
    const UINT newIns = nID - ID_CHANGE_INSTRUMENT;

    if (newIns == 0)
    {
        RowMask sp = {false, true, false, false, false};    // Setup mask to only clear instrument data in OnClearSelection
        OnClearSelection(false, sp);    // Clears instrument selection from pattern
    } else
    {
        SetSelectionInstrument(newIns);
    }
}

void CViewPattern::OnSelectPCNoteParam(UINT nID)
//----------------------------------------------
{
    CModDoc *pModDoc = GetDocument(); if (!pModDoc) return;
    module_renderer *pSndFile = pModDoc->GetSoundFile(); if (!pSndFile) return;

    UINT paramNdx = nID - ID_CHANGE_PCNOTE_PARAM;
    bool bModified = false;
    modplug::tracker::modevent_t *p;
    for(modplug::tracker::rowindex_t nRow = GetSelectionStartRow(); nRow <= GetSelectionEndRow(); nRow++)
    {
        for(modplug::tracker::chnindex_t nChn = GetSelectionStartChan(); nChn <= GetSelectionEndChan(); nChn++)
        {
            p = pSndFile->Patterns[m_nPattern] + nRow * pSndFile->GetNumChannels() + nChn;
            if(p && p->IsPcNote() && (p->GetValueVolCol() != paramNdx))
            {
                bModified = true;
                p->SetValueVolCol(paramNdx);
            }
        }
    }
    if (bModified)
    {
        pModDoc->SetModified();
        pModDoc->UpdateAllViews(NULL, HINT_PATTERNDATA | (m_nPattern << HINT_SHIFT_PAT), NULL);
    }
}


void CViewPattern::OnSelectPlugin(UINT nID)
//-----------------------------------------
{
    CModDoc *pModDoc = GetDocument(); if (!pModDoc) return;
    module_renderer *pSndFile = pModDoc->GetSoundFile(); if (!pSndFile) return;

    if (m_nMenuOnChan)
    {
        UINT newPlug = nID-ID_PLUGSELECT;
        if (newPlug <= MAX_MIXPLUGINS && newPlug != pSndFile->ChnSettings[m_nMenuOnChan-1].nMixPlugin)
        {
            pSndFile->ChnSettings[m_nMenuOnChan-1].nMixPlugin = newPlug;
            if(pSndFile->GetModSpecifications().supportsPlugins)
                pModDoc->SetModified();
            InvalidateChannelsHeaders();
        }
    }
}

//rewbs.merge
bool CViewPattern::HandleSplit(modplug::tracker::modevent_t* p, int note)
//-----------------------------------------------------
{
    CModDoc *pModDoc = GetDocument(); if (!pModDoc) return false;

    modplug::tracker::instr_t ins = GetCurrentInstrument();
    bool isSplit = false;

    if(pModDoc->GetSplitKeyboardSettings()->IsSplitActive())
    {
        if(note <= pModDoc->GetSplitKeyboardSettings()->splitNote)
        {
            isSplit = true;

            if (pModDoc->GetSplitKeyboardSettings()->octaveLink && note <= NoteMax)
            {
                note += 12 * pModDoc->GetSplitKeyboardSettings()->octaveModifier;
                note = CLAMP(note, NoteMin, NoteMax);
            }
            if (pModDoc->GetSplitKeyboardSettings()->splitInstrument)
            {
                p->instr = pModDoc->GetSplitKeyboardSettings()->splitInstrument;
            }
        }
    }

    p->note = note;
    if(ins)
        p->instr = ins;

    return isSplit;
}

bool CViewPattern::BuildPluginCtxMenu(HMENU hMenu, UINT nChn, module_renderer* pSndFile)
//---------------------------------------------------------------------------------
{
    CHAR s[512];
    bool itemFound;
    for (UINT plug=0; plug<=MAX_MIXPLUGINS; plug++)    {

        itemFound=false;
        s[0] = 0;

        if (!plug) {
            strcpy(s, "No plugin");
            itemFound=true;
        } else    {
            PSNDMIXPLUGIN p = &(pSndFile->m_MixPlugins[plug-1]);
            if (p->Info.szLibraryName[0]) {
                wsprintf(s, "FX%d: %s", plug, p->Info.szName);
                itemFound=true;
            }
        }

        if (itemFound){
            m_nMenuOnChan=nChn+1;
            if (plug == pSndFile->ChnSettings[nChn].nMixPlugin) {
                AppendMenu(hMenu, (MF_STRING|MF_CHECKED), ID_PLUGSELECT+plug, s);
            } else {
                AppendMenu(hMenu, MF_STRING, ID_PLUGSELECT+plug, s);
            }
        }
    }
    return true;
}

bool CViewPattern::BuildSoloMuteCtxMenu( HMENU hMenu, UINT nChn, module_renderer* pSndFile )
{
    AppendMenu(hMenu, (pSndFile->ChnSettings[nChn].dwFlags & CHN_MUTE) ?
                       (MF_STRING|MF_CHECKED) : MF_STRING, ID_PATTERN_MUTE,
                       "Mute Channel");
    bool bSolo = false, bUnmuteAll = false;
    bool bSoloPending = false, bUnmuteAllPending = false; // doesn't work perfectly yet

    for (modplug::tracker::chnindex_t i = 0; i < pSndFile->m_nChannels; i++) {
        if (i != nChn)
        {
            if (!(pSndFile->ChnSettings[i].dwFlags & CHN_MUTE)) bSolo = bSoloPending = true;
            if (!((~pSndFile->ChnSettings[i].dwFlags & CHN_MUTE) && pSndFile->m_bChannelMuteTogglePending[i])) bSoloPending = true;
        }
        else
        {
            if (pSndFile->ChnSettings[i].dwFlags & CHN_MUTE) bSolo = bSoloPending = true;
            if ((~pSndFile->ChnSettings[i].dwFlags & CHN_MUTE) && pSndFile->m_bChannelMuteTogglePending[i]) bSoloPending = true;
        }
        if (pSndFile->ChnSettings[i].dwFlags & CHN_MUTE) bUnmuteAll = bUnmuteAllPending = true;
        if ((~pSndFile->ChnSettings[i].dwFlags & CHN_MUTE) && pSndFile->m_bChannelMuteTogglePending[i]) bUnmuteAllPending = true;
    }
    if (bSolo) AppendMenu(hMenu, MF_STRING, ID_PATTERN_SOLO, "Solo Channel");
    if (bUnmuteAll) AppendMenu(hMenu, MF_STRING, ID_PATTERN_UNMUTEALL, "Unmute All");

    AppendMenu(hMenu,
            pSndFile->m_bChannelMuteTogglePending[nChn] ?
                    (MF_STRING|MF_CHECKED) : MF_STRING,
             ID_PATTERN_TRANSITIONMUTE,
            (pSndFile->ChnSettings[nChn].dwFlags & CHN_MUTE) ?
            "On transition: Unmute" :
            "On transition: Mute");

    if (bUnmuteAllPending) AppendMenu(hMenu, MF_STRING, ID_PATTERN_TRANSITION_UNMUTEALL, "On transition: Unmute all");
    if (bSoloPending) AppendMenu(hMenu, MF_STRING, ID_PATTERN_TRANSITIONSOLO, "On transition: Solo");

    AppendMenu(hMenu, MF_STRING, ID_PATTERN_CHNRESET, "Reset Channel");

    return true;
}

bool CViewPattern::BuildRecordCtxMenu(HMENU hMenu, UINT nChn, CModDoc* pModDoc)
//-----------------------------------------------------------------------------
{
    AppendMenu(hMenu, pModDoc->IsChannelRecord1(nChn) ? (MF_STRING|MF_CHECKED) : MF_STRING, ID_EDIT_RECSELECT, "Record select");
    AppendMenu(hMenu, pModDoc->IsChannelRecord2(nChn) ? (MF_STRING|MF_CHECKED) : MF_STRING, ID_EDIT_SPLITRECSELECT, "Split Record select");
    return true;
}



bool CViewPattern::BuildRowInsDelCtxMenu( HMENU hMenu )
{
    CString label = "";
    if (GetSelectionStartRow() != GetSelectionEndRow()) {
        label = "Rows";
    } else {
        label = "Row";
    }

    AppendMenu(hMenu, MF_STRING, ID_PATTERN_INSERTROW, "Insert " + label);
    AppendMenu(hMenu, MF_STRING, ID_PATTERN_DELETEROW, "Delete " + label);
    return true;
}

bool CViewPattern::BuildMiscCtxMenu( HMENU hMenu )
{
    AppendMenu(hMenu, MF_STRING, ID_SHOWTIMEATROW, "Show row play time");
    return true;
}

bool CViewPattern::BuildSelectionCtxMenu( HMENU hMenu )
{
    AppendMenu(hMenu, MF_STRING, ID_EDIT_SELECTCOLUMN, "Select Column");
    AppendMenu(hMenu, MF_STRING, ID_EDIT_SELECT_ALL, "Select Pattern");
    return true;
}

bool CViewPattern::BuildGrowShrinkCtxMenu( HMENU hMenu )
{
    AppendMenu(hMenu, MF_STRING, ID_GROW_SELECTION, "Grow selection");
    AppendMenu(hMenu, MF_STRING, ID_SHRINK_SELECTION, "Shrink selection");
    return true;
}

bool CViewPattern::BuildNoteInterpolationCtxMenu( HMENU hMenu, module_renderer* pSndFile )
{
    CArray<UINT, UINT> validChans;
    uint32_t greyed = MF_GRAYED;

    UINT startRow = GetSelectionStartRow();
    UINT endRow   = GetSelectionEndRow();

    if (ListChansWhereColSelected(NOTE_COLUMN, validChans) > 0)
    {
        for (int valChnIdx=0; valChnIdx<validChans.GetCount(); valChnIdx++)
        {
            if (IsInterpolationPossible(startRow, endRow,
                                        validChans[valChnIdx], NOTE_COLUMN, pSndFile))
            {
                greyed=0;    //Can do interpolation.
                break;
            }
        }

    }
    if (!greyed || !(CMainFrame::m_dwPatternSetup&PATTERN_OLDCTXMENUSTYLE))
    {
        AppendMenu(hMenu, MF_STRING|greyed, ID_PATTERN_INTERPOLATE_NOTE, "Interpolate Note");
        return true;
    }
    return false;
}

bool CViewPattern::BuildVolColInterpolationCtxMenu( HMENU hMenu, module_renderer* pSndFile )
{
    CArray<UINT, UINT> validChans;
    uint32_t greyed = MF_GRAYED;

    UINT startRow = GetSelectionStartRow();
    UINT endRow   = GetSelectionEndRow();

    if (ListChansWhereColSelected(VOL_COLUMN, validChans) > 0)
    {
        for (int valChnIdx=0; valChnIdx<validChans.GetCount(); valChnIdx++)
        {
            if (IsInterpolationPossible(startRow, endRow,
                                        validChans[valChnIdx], VOL_COLUMN, pSndFile))
            {
                greyed = 0;    //Can do interpolation.
                break;
            }
        }
    }
    if (!greyed || !(CMainFrame::m_dwPatternSetup&PATTERN_OLDCTXMENUSTYLE))
    {
        AppendMenu(hMenu, MF_STRING|greyed, ID_PATTERN_INTERPOLATE_VOLUME, "Interpolate Vol Col");
        return true;
    }
    return false;
}


bool CViewPattern::BuildEffectInterpolationCtxMenu( HMENU hMenu, module_renderer* pSndFile )
{
    CArray<UINT, UINT> validChans;
    uint32_t greyed = MF_GRAYED;

    UINT startRow = GetSelectionStartRow();
    UINT endRow   = GetSelectionEndRow();

    if (ListChansWhereColSelected(EFFECT_COLUMN, validChans) > 0)
    {
        for (int valChnIdx=0; valChnIdx<validChans.GetCount(); valChnIdx++)
        {
            if  (IsInterpolationPossible(startRow, endRow, validChans[valChnIdx], EFFECT_COLUMN, pSndFile))
            {
                greyed=0;    //Can do interpolation.
                break;
            }
        }
    }

    if (ListChansWhereColSelected(PARAM_COLUMN, validChans) > 0)
    {
        for (int valChnIdx=0; valChnIdx<validChans.GetCount(); valChnIdx++)
        {
            if  (IsInterpolationPossible(startRow, endRow, validChans[valChnIdx], EFFECT_COLUMN, pSndFile))
            {
                greyed = 0;    //Can do interpolation.
                break;
            }
        }
    }


    if (!greyed || !(CMainFrame::m_dwPatternSetup&PATTERN_OLDCTXMENUSTYLE))
    {
        AppendMenu(hMenu, MF_STRING|greyed, ID_PATTERN_INTERPOLATE_EFFECT, "Interpolate Effect");
        return true;
    }
    return false;
}

bool CViewPattern::BuildEditCtxMenu( HMENU hMenu, CModDoc* pModDoc )
{
    HMENU pasteSpecialMenu = ::CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, ID_EDIT_CUT, "Cut");
    AppendMenu(hMenu, MF_STRING, ID_EDIT_COPY, "Copy");
    AppendMenu(hMenu, MF_STRING, ID_EDIT_PASTE, "Paste");
    AppendMenu(hMenu, MF_POPUP, (UINT)pasteSpecialMenu, "Paste Special");
    AppendMenu(pasteSpecialMenu, MF_STRING, ID_EDIT_MIXPASTE, "Mix Paste");
    AppendMenu(pasteSpecialMenu, MF_STRING, ID_EDIT_MIXPASTE_ITSTYLE, "Mix Paste (IT Style)");
    AppendMenu(pasteSpecialMenu, MF_STRING, ID_EDIT_PASTEFLOOD, "Paste Flood");
    AppendMenu(pasteSpecialMenu, MF_STRING, ID_EDIT_PUSHFORWARDPASTE, "Push Forward Paste (Insert)");

    AppendMenu(hMenu, MF_STRING, ID_CLEAR_SELECTION, "Clear selection");

    return true;
}

bool CViewPattern::BuildVisFXCtxMenu( HMENU hMenu )
{
    CArray<UINT, UINT> validChans;
    uint32_t greyed = (ListChansWhereColSelected(EFFECT_COLUMN, validChans)>0)?FALSE:MF_GRAYED;

    if (!greyed || !(CMainFrame::m_dwPatternSetup&PATTERN_OLDCTXMENUSTYLE)) {
        AppendMenu(hMenu, MF_STRING|greyed, ID_PATTERN_VISUALIZE_EFFECT, "Visualize Effect");
        return true;
    }
    return false;
}

bool CViewPattern::BuildRandomCtxMenu( HMENU hMenu )
{
    AppendMenu(hMenu, MF_STRING, ID_PATTERN_OPEN_RANDOMIZER, "Randomize...");
    return true;
}

bool CViewPattern::BuildTransposeCtxMenu( HMENU hMenu )
{
    CArray<UINT, UINT> validChans;
    uint32_t greyed = (ListChansWhereColSelected(NOTE_COLUMN, validChans)>0)?FALSE:MF_GRAYED;

    //AppendMenu(hMenu, MF_STRING, ID_RUN_SCRIPT, "Run script");

    if (!greyed || !(CMainFrame::m_dwPatternSetup&PATTERN_OLDCTXMENUSTYLE)) {
        AppendMenu(hMenu, MF_STRING|greyed, ID_TRANSPOSE_UP, "Transpose +1");
        AppendMenu(hMenu, MF_STRING|greyed, ID_TRANSPOSE_DOWN, "Transpose -1");
        AppendMenu(hMenu, MF_STRING|greyed, ID_TRANSPOSE_OCTUP, "Transpose +12");
        AppendMenu(hMenu, MF_STRING|greyed, ID_TRANSPOSE_OCTDOWN, "Transpose -12");
        return true;
    }
    return false;
}

bool CViewPattern::BuildAmplifyCtxMenu( HMENU hMenu )
{
    CArray<UINT, UINT> validChans;
    uint32_t greyed = (ListChansWhereColSelected(VOL_COLUMN, validChans)>0)?FALSE:MF_GRAYED;

    if (!greyed || !(CMainFrame::m_dwPatternSetup&PATTERN_OLDCTXMENUSTYLE)) {
        AppendMenu(hMenu, MF_STRING|greyed, ID_PATTERN_AMPLIFY, "Amplify");
        return true;
    }
    return false;
}


bool CViewPattern::BuildChannelControlCtxMenu(HMENU hMenu)
//--------------------------------------------------------
{
    AppendMenu(hMenu, MF_SEPARATOR, 0, "");

    AppendMenu(hMenu, MF_STRING, ID_PATTERN_DUPLICATECHANNEL, "Duplicate this channel");

    HMENU addChannelMenu = ::CreatePopupMenu();
    AppendMenu(hMenu, MF_POPUP, (UINT)addChannelMenu, "Add channel\t");
    AppendMenu(addChannelMenu, MF_STRING, ID_PATTERN_ADDCHANNEL_FRONT, "Before this channel");
    AppendMenu(addChannelMenu, MF_STRING, ID_PATTERN_ADDCHANNEL_AFTER, "After this channel");

    HMENU removeChannelMenu = ::CreatePopupMenu();
    AppendMenu(hMenu, MF_POPUP, (UINT)removeChannelMenu, "Remove channel\t");
    AppendMenu(removeChannelMenu, MF_STRING, ID_PATTERN_REMOVECHANNEL, "Remove this channel\t");
    AppendMenu(removeChannelMenu, MF_STRING, ID_PATTERN_REMOVECHANNELDIALOG, "Choose channels to remove...\t");


    return false;
}


bool CViewPattern::BuildSetInstCtxMenu( HMENU hMenu, module_renderer* pSndFile )
{
    CArray<UINT, UINT> validChans;
    uint32_t greyed = (ListChansWhereColSelected(INST_COLUMN, validChans)>0)?FALSE:MF_GRAYED;

    if (!greyed || !(CMainFrame::m_dwPatternSetup & PATTERN_OLDCTXMENUSTYLE))
    {
        bool isPcNote = false;
        modplug::tracker::modevent_t *mSelStart = nullptr;
        if((pSndFile != nullptr) && (pSndFile->Patterns.IsValidPat(m_nPattern)))
        {
            mSelStart = pSndFile->Patterns[m_nPattern].GetpModCommand(GetSelectionStartRow(), GetSelectionStartChan());
            if(mSelStart != nullptr && mSelStart->IsPcNote())
            {
                isPcNote = true;
            }
        }
        if(isPcNote)
            return false;

        // Create the new menu and add it to the existing menu.
        HMENU instrumentChangeMenu = ::CreatePopupMenu();
        AppendMenu(hMenu, MF_POPUP|greyed, (UINT)instrumentChangeMenu, "Change Instrument");

        if(pSndFile == nullptr || pSndFile->GetpModDoc() == nullptr)
            return false;

        CModDoc* const pModDoc = pSndFile->GetpModDoc();

        if(!greyed)
        {
            if (pSndFile->m_nInstruments)
            {
                for (UINT i=1; i<=pSndFile->m_nInstruments; i++)
                {
                    if (pSndFile->Instruments[i] == NULL)
                        continue;

                    CString instString = pModDoc->GetPatternViewInstrumentName(i, true);
                    if(instString.GetLength() > 0) AppendMenu(instrumentChangeMenu, MF_STRING, ID_CHANGE_INSTRUMENT+i, pModDoc->GetPatternViewInstrumentName(i));
                    //Adding the entry to the list only if it has some name, since if the name is empty,
                    //it likely is some non-used instrument.
                }

            }
            else
            {
                CHAR s[256];
                UINT nmax = pSndFile->m_nSamples;
                while ((nmax > 1) && (pSndFile->Samples[nmax].sample_data.generic == NULL) && (!pSndFile->m_szNames[nmax][0])) nmax--;
                for (UINT i=1; i<=nmax; i++) if ((pSndFile->m_szNames[i][0]) || (pSndFile->Samples[i].sample_data.generic))
                {
                    wsprintf(s, "%02d: %s", i, pSndFile->m_szNames[i]);
                    AppendMenu(instrumentChangeMenu, MF_STRING, ID_CHANGE_INSTRUMENT+i, s);
                }
            }

            //Add options to remove instrument from selection.
            AppendMenu(instrumentChangeMenu, MF_SEPARATOR, 0, 0);
            AppendMenu(instrumentChangeMenu, MF_STRING, ID_CHANGE_INSTRUMENT, "Remove instrument");
            AppendMenu(instrumentChangeMenu, MF_STRING, ID_CHANGE_INSTRUMENT + GetCurrentInstrument(), "Set to current instrument");
        }
        return true;
    }
    return false;
}


bool CViewPattern::BuildChannelMiscCtxMenu(HMENU hMenu, module_renderer* pSndFile)
//---------------------------------------------------------------------------
{
    if((pSndFile->m_nType & (MOD_TYPE_IT|MOD_TYPE_MPT)) == 0) return false;
    AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
    AppendMenu(hMenu, MF_STRING, ID_CHANNEL_RENAME, "Rename channel");
    return true;
}


// Context menu for Param Control notes
bool CViewPattern::BuildPCNoteCtxMenu( HMENU hMenu, module_renderer* pSndFile )
{
    modplug::tracker::modevent_t *mSelStart = nullptr;
    if((pSndFile == nullptr) || (!pSndFile->Patterns.IsValidPat(m_nPattern)))
        return false;
    mSelStart = pSndFile->Patterns[m_nPattern].GetpModCommand(GetSelectionStartRow(), GetSelectionStartChan());
    if((mSelStart == nullptr) || (!mSelStart->IsPcNote()))
        return false;

    char s[72];

    // Create sub menu for "change plugin"
    HMENU pluginChangeMenu = ::CreatePopupMenu();
    AppendMenu(hMenu, MF_POPUP, (UINT)pluginChangeMenu, "Change Plugin");
    for(PLUGINDEX nPlg = 0; nPlg < MAX_MIXPLUGINS; nPlg++)
    {
        if(pSndFile->m_MixPlugins[nPlg].pMixPlugin != nullptr)
        {
            wsprintf(s, "%02d: %s", nPlg + 1, pSndFile->m_MixPlugins[nPlg].GetName());
            AppendMenu(pluginChangeMenu, MF_STRING | ((nPlg + 1) == mSelStart->instr) ? MF_CHECKED : 0, ID_CHANGE_INSTRUMENT + nPlg + 1, s);
        }
    }

    if(mSelStart->instr >= 1 && mSelStart->instr <= MAX_MIXPLUGINS)
    {
        CVstPlugin *plug = (CVstPlugin *)(pSndFile->m_MixPlugins[mSelStart->instr - 1].pMixPlugin);

        if(plug != nullptr)
        {

            // Create sub menu for "change plugin param"
            HMENU paramChangeMenu = ::CreatePopupMenu();
            AppendMenu(hMenu, MF_POPUP, (UINT)paramChangeMenu, "Change Plugin Parameter\t");

            char sname[64];
            uint16_t nThisParam = mSelStart->GetValueVolCol();
            UINT nParams = plug->GetNumParameters();
            for (UINT i = 0; i < nParams; i++)
            {
                plug->GetParamName(i, sname, sizeof(sname));
                wsprintf(s, "%02d: %s", i, sname);
                AppendMenu(paramChangeMenu, MF_STRING | (i == nThisParam) ? MF_CHECKED : 0, ID_CHANGE_PCNOTE_PARAM + i, s);
            }
        }

        AppendMenu(hMenu, MF_STRING, ID_PATTERN_EDIT_PCNOTE_PLUGIN, "Toggle plugin editor");
    }

    return true;
}


modplug::tracker::rowindex_t CViewPattern::GetSelectionStartRow()
//-------------------------------------------
{
    return bad_min(GetRowFromCursor(m_dwBeginSel), GetRowFromCursor(m_dwEndSel));
}

modplug::tracker::rowindex_t CViewPattern::GetSelectionEndRow()
//-----------------------------------------
{
    return bad_max(GetRowFromCursor(m_dwBeginSel), GetRowFromCursor(m_dwEndSel));
}

modplug::tracker::chnindex_t CViewPattern::GetSelectionStartChan()
//------------------------------------------------
{
    return bad_min(GetChanFromCursor(m_dwBeginSel), GetChanFromCursor(m_dwEndSel));
}

modplug::tracker::chnindex_t CViewPattern::GetSelectionEndChan()
//----------------------------------------------
{
    return bad_max(GetChanFromCursor(m_dwBeginSel), GetChanFromCursor(m_dwEndSel));
}

UINT CViewPattern::ListChansWhereColSelected(PatternColumns colType, CArray<UINT,UINT> &chans) {
//----------------------------------------------------------------------------------
    chans.RemoveAll();
    UINT startChan = GetSelectionStartChan();
    UINT endChan   = GetSelectionEndChan();

    if (GetColTypeFromCursor(m_dwBeginSel) <= colType) {
        chans.Add(startChan);    //first selected chan includes this col type
    }
    for (UINT chan=startChan+1; chan<endChan; chan++) {
        chans.Add(chan); //All chans between first & last must include this col type
    }
    if ((startChan != endChan) && colType <= GetColTypeFromCursor(m_dwEndSel)) {
        chans.Add(endChan);            //last selected chan includes this col type
    }

    return chans.GetCount();
}


bool CViewPattern::IsInterpolationPossible(modplug::tracker::rowindex_t startRow, modplug::tracker::rowindex_t endRow, modplug::tracker::chnindex_t chan,
                                           PatternColumns colType, module_renderer* pSndFile)
//--------------------------------------------------------------------------------------
{
    if (startRow == endRow)
        return false;

    bool result = false;
    const modplug::tracker::modevent_t startRowMC = *pSndFile->Patterns[m_nPattern].GetpModCommand(startRow, chan);
    const modplug::tracker::modevent_t endRowMC = *pSndFile->Patterns[m_nPattern].GetpModCommand(endRow, chan);
    UINT startRowCmd, endRowCmd;

    if(colType == EFFECT_COLUMN && (startRowMC.IsPcNote() || endRowMC.IsPcNote()))
        return true;

    switch (colType)
    {
        case NOTE_COLUMN:
            startRowCmd = startRowMC.note;
            endRowCmd = endRowMC.note;
            result = (startRowCmd >= NoteMin && endRowCmd >= NoteMin);
            break;
        case EFFECT_COLUMN:
            startRowCmd = startRowMC.command;
            endRowCmd = endRowMC.command;
            result = (startRowCmd == endRowCmd && startRowCmd != CmdNone) || (startRowCmd != CmdNone && endRowCmd == CmdNone) || (startRowCmd == CmdNone && endRowCmd != CmdNone);
            break;
        case VOL_COLUMN:
            startRowCmd = startRowMC.volcmd;
            endRowCmd = endRowMC.volcmd;
            result = (startRowCmd == endRowCmd && startRowCmd != VolCmdNone) || (startRowCmd != VolCmdNone && endRowCmd == VolCmdNone) || (startRowCmd == VolCmdNone && endRowCmd != VolCmdNone);
            break;
        default:
            result = false;
    }
    return result;
}

void CViewPattern::OnRButtonDblClk(UINT nFlags, CPoint point)
//-----------------------------------------------------------
{
    OnRButtonDown(nFlags, point);
    CModScrollView::OnRButtonDblClk(nFlags, point);
}

void CViewPattern::OnTogglePendingMuteFromClick()
//-----------------------------------------------
{
    TogglePendingMute(GetChanFromCursor(m_nMenuParam));
}

void CViewPattern::OnPendingSoloChnFromClick()
//-----------------------------------------------
{
    PendingSoloChn(GetChanFromCursor(m_nMenuParam));
}

void CViewPattern::OnPendingUnmuteAllChnFromClick()
//----------------------------------------------
{
    GetDocument()->GetSoundFile()->GetPlaybackEventer().PatternTransitionChnUnmuteAll();
    InvalidateChannelsHeaders();
}

void CViewPattern::PendingSoloChn(const modplug::tracker::chnindex_t nChn)
//---------------------------------------------
{
    GetDocument()->GetSoundFile()->GetPlaybackEventer().PatternTranstionChnSolo(nChn);
    InvalidateChannelsHeaders();
}

void CViewPattern::TogglePendingMute(UINT nChn)
//---------------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    module_renderer *pSndFile;
    pSndFile = pModDoc->GetSoundFile();
    pSndFile->m_bChannelMuteTogglePending[nChn]=!pSndFile->m_bChannelMuteTogglePending[nChn];
    InvalidateChannelsHeaders();
}


bool CViewPattern::IsEditingEnabled_bmsg()
//----------------------------------------
{
    if(IsEditingEnabled()) return true;

    HMENU hMenu;

    if ( (hMenu = ::CreatePopupMenu()) == NULL) return false;

    CPoint pt = GetPointFromPosition(m_dwCursor);

    AppendMenu(hMenu, MF_STRING, IDC_PATTERN_RECORD, "Editing(record) is disabled; click here to enable it.");
    //To check: It seems to work the way it should, but still is it ok to use IDC_PATTERN_RECORD here since it is not
    //'aimed' to be used here.

    ClientToScreen(&pt);
    ::TrackPopupMenu(hMenu, TPM_LEFTALIGN, pt.x, pt.y, 0, m_hWnd, NULL);

    ::DestroyMenu(hMenu);

    return false;
}


void CViewPattern::OnShowTimeAtRow()
//----------------------------------
{
    CModDoc* pModDoc = GetDocument();
    module_renderer* pSndFile = (pModDoc) ? pModDoc->GetSoundFile() : 0;
    if(!pSndFile) return;

    CString msg;
    modplug::tracker::orderindex_t currentOrder = SendCtrlMessage(CTRLMSG_GETCURRENTORDER);
    if(pSndFile->Order[currentOrder] == m_nPattern)
    {
        const double t = pSndFile->GetPlaybackTimeAt(currentOrder, m_nRow, false);
        if(t < 0)
            msg.Format("Unable to determine the time. Possible cause: No order %d, row %d found from play sequence", currentOrder, m_nRow);
        else
        {
            const uint32_t minutes = static_cast<uint32_t>(t/60.0);
            const float seconds = t - minutes*60;
            msg.Format("Estimate for playback time at order %d(pattern %d), row %d: %d minute%s %.2f seconds", currentOrder, m_nPattern, m_nRow, minutes, (minutes == 1) ? "" : "s", seconds);
        }
    }
    else
        msg.Format("Unable to determine the time: pattern at current order(=%d) does not correspond to pattern at pattern view(=pattern %d).", currentOrder, m_nPattern);

    MessageBox(msg);
}



void CViewPattern::SetEditPos(const module_renderer& rSndFile,
                              modplug::tracker::rowindex_t& iRow, modplug::tracker::patternindex_t& iPat,
                              const modplug::tracker::rowindex_t iRowCandidate, const modplug::tracker::patternindex_t iPatCandidate) const
//-------------------------------------------------------------------------------------------
{
    if(rSndFile.Patterns.IsValidPat(iPatCandidate) && rSndFile.Patterns[iPatCandidate].IsValidRow(iRowCandidate))
    { // Case: Edit position candidates are valid -- use them.
        iPat = iPatCandidate;
        iRow = iRowCandidate;
    }
    else // Case: Edit position candidates are not valid -- set edit cursor position instead.
    {
        iPat = m_nPattern;
        iRow = m_nRow;
    }
}


void CViewPattern::OnRenameChannel()
//----------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if(pModDoc == nullptr) return;
    module_renderer *pSndFile = pModDoc->GetSoundFile();
    if(pSndFile == nullptr) return;

    modplug::tracker::chnindex_t nChn = GetChanFromCursor(m_nMenuParam);
    CChannelRenameDlg dlg(this, pSndFile->ChnSettings[nChn].szName, nChn + 1);
    if(dlg.DoModal() != IDOK || dlg.bChanged == false) return;

    strcpy(pSndFile->ChnSettings[nChn].szName, dlg.m_sName);
    pModDoc->SetModified();
    pModDoc->UpdateAllViews(NULL, HINT_MODCHANNELS);
}


void CViewPattern::SetSplitKeyboardSettings()
//-------------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if(pModDoc == nullptr) return;
    module_renderer *pSndFile = pModDoc->GetSoundFile();
    if(pSndFile == nullptr) return;

    CSplitKeyboadSettings dlg(CMainFrame::GetMainFrame(), pSndFile, pModDoc->GetSplitKeyboardSettings());
    if (dlg.DoModal() == IDOK)
        pModDoc->UpdateAllViews(NULL, HINT_INSNAMES|HINT_SMPNAMES);
}


void CViewPattern::ExecutePaste(enmPatternPasteModes pasteMode)
//-------------------------------------------------------------
{
    CModDoc *pModDoc = GetDocument();

    if (pModDoc && IsEditingEnabled_bmsg())
    {
        pModDoc->PastePattern(m_nPattern, m_dwBeginSel, pasteMode);
        InvalidatePattern(FALSE);
        SetFocus();
    }
}


void CViewPattern::OnTogglePCNotePluginEditor()
//---------------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if(pModDoc == nullptr) return;
    module_renderer *pSndFile = pModDoc->GetSoundFile();
    if((pSndFile == nullptr) || (!pSndFile->Patterns.IsValidPat(m_nPattern)))
        return;

    modplug::tracker::modevent_t *mSelStart = nullptr;
    mSelStart = pSndFile->Patterns[m_nPattern].GetpModCommand(GetSelectionStartRow(), GetSelectionStartChan());
    if((mSelStart == nullptr) || (!mSelStart->IsPcNote()))
        return;
    if(mSelStart->instr < 1 || mSelStart->instr > MAX_MIXPLUGINS)
        return;

    PLUGINDEX nPlg = (PLUGINDEX)(mSelStart->instr - 1);
    pModDoc->TogglePluginEditor(nPlg);
}


modplug::tracker::rowindex_t CViewPattern::GetRowsPerBeat() const
//-------------------------------------------
{
    module_renderer *pSndFile;
    if(GetDocument() == nullptr || (pSndFile = GetDocument()->GetSoundFile()) == nullptr)
        return 0;
    if(!pSndFile->Patterns[m_nPattern].GetOverrideSignature())
        return pSndFile->m_nDefaultRowsPerBeat;
    else
        return pSndFile->Patterns[m_nPattern].GetRowsPerBeat();
}


modplug::tracker::rowindex_t CViewPattern::GetRowsPerMeasure() const
//----------------------------------------------
{
    module_renderer *pSndFile;
    if(GetDocument() == nullptr || (pSndFile = GetDocument()->GetSoundFile()) == nullptr)
        return 0;
    if(!pSndFile->Patterns[m_nPattern].GetOverrideSignature())
        return pSndFile->m_nDefaultRowsPerMeasure;
    else
        return pSndFile->Patterns[m_nPattern].GetRowsPerMeasure();
}


void CViewPattern::SetSelectionInstrument(const modplug::tracker::instrumentindex_t nIns)
//-------------------------------------------------------------------
{
    CModDoc *pModDoc;
    module_renderer *pSndFile;
    modplug::tracker::modevent_t *p;
    bool bModified = false;

    if (!nIns) return;
    if ((pModDoc = GetDocument()) == nullptr) return;
    pSndFile = pModDoc->GetSoundFile();
    if(!pSndFile) return;
    p = pSndFile->Patterns[m_nPattern];
    if (!p) return;
    BeginWaitCursor();
    PrepareUndo(m_dwBeginSel, m_dwEndSel);

    //rewbs: re-written to work regardless of selection
    UINT startRow  = GetSelectionStartRow();
    UINT endRow    = GetSelectionEndRow();
    UINT startChan = GetSelectionStartChan();
    UINT endChan   = GetSelectionEndChan();

    for (UINT r=startRow; r<endRow+1; r++)
    {
        p = pSndFile->Patterns[m_nPattern] + r * pSndFile->m_nChannels + startChan;
        for (UINT c = startChan; c < endChan + 1; c++, p++)
        {
            // If a note or an instr is present on the row, do the change, if required.
            // Do not set instr if note and instr are both blank.
            // Do set instr if note is a PC note and instr is blank.
            if ( ((p->note > 0 && p->note < NoteMinSpecial) || p->IsPcNote() || p->instr) && (p->instr!=nIns) )
            {
                p->instr = nIns;
                bModified = true;
            }
        }
    }

    if (bModified)
    {
        pModDoc->SetModified();
        pModDoc->UpdateAllViews(NULL, HINT_PATTERNDATA | (m_nPattern << HINT_SHIFT_PAT), NULL);
    }
    EndWaitCursor();
}


// Select a whole beat (selectBeat = true) or measure.
void CViewPattern::SelectBeatOrMeasure(bool selectBeat)
//-----------------------------------------------------
{
    const modplug::tracker::rowindex_t adjust = selectBeat ? GetRowsPerBeat() : GetRowsPerMeasure();

    // Snap to start of beat / measure of upper-left corner of current selection
    const modplug::tracker::rowindex_t startRow = GetSelectionStartRow() - (GetSelectionStartRow() % adjust);
    // Snap to end of beat / measure of lower-right corner of current selection
    const modplug::tracker::rowindex_t endRow = GetSelectionEndRow() + adjust - (GetSelectionEndRow() % adjust) - 1;

    modplug::tracker::chnindex_t startChannel = GetSelectionStartChan(), endChannel = GetSelectionEndChan();
    UINT startColumn = 0, endColumn = 0;

    if(m_dwBeginSel == m_dwEndSel)
    {
        // No selection has been made yet => expand selection to whole channel.
        endColumn = LAST_COLUMN;    // Extend to param column
    } else if(startRow == GetSelectionStartRow() && endRow == GetSelectionEndRow())
    {
        // Whole beat or measure is already selected
        if(GetColTypeFromCursor(m_dwBeginSel) == NOTE_COLUMN && GetColTypeFromCursor(m_dwEndSel) == LAST_COLUMN)
        {
            // Whole channel is already selected => expand selection to whole row.
            startChannel = startColumn = 0;
            endChannel = MAX_BASECHANNELS;
            endColumn = LAST_COLUMN;
        } else
        {
            // Channel is only partly selected => expand to whole channel first.
            endColumn = LAST_COLUMN;    // Extend to param column
        }
    }
    else
    {
        // Some arbitrary selection: Remember start / end column
        startColumn = GetColTypeFromCursor(m_dwBeginSel);
        endColumn = GetColTypeFromCursor(m_dwEndSel);
    }
    SetCurSel(CreateCursor(startRow, startChannel, startColumn), CreateCursor(endRow, endChannel, endColumn));
}
