#include "stdafx.h"
#include "mptrack.h"
#include "mainfrm.h"
#include "moddoc.h"
#include "globals.h"
#include "ctrl_com.h"
#include "view_com.h"

BEGIN_MESSAGE_MAP(CCtrlComments, CModControlDlg)
    //{{AFX_MSG_MAP(CCtrlComments)
    ON_MESSAGE(WM_MOD_KEYCOMMAND,        OnCustomKeyMsg)        //rewbs.customKeys
    ON_EN_CHANGE(IDC_EDIT_COMMENTS,                OnCommentsChanged)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CCtrlComments::DoDataExchange(CDataExchange* pDX)
//----------------------------------------------------
{
    CModControlDlg::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CCtrlComments)
    DDX_Control(pDX, IDC_EDIT_COMMENTS, m_EditComments);
    //}}AFX_DATA_MAP
}


CCtrlComments::CCtrlComments()
//----------------------------
{
    m_nLockCount = 0;
    m_hFont = NULL;
}


CRuntimeClass *CCtrlComments::GetAssociatedViewClass()
//----------------------------------------------------
{
    return RUNTIME_CLASS(CViewComments);
}


void CCtrlComments::OnDeactivatePage()
//------------------------------------
{
    CModControlDlg::OnDeactivatePage();
}


BOOL CCtrlComments::OnInitDialog()
//--------------------------------
{
    CModControlDlg::OnInitDialog();
    // Initialize comments
    m_hFont = NULL;
    m_EditComments.SetMargins(4, 0);
    UpdateView(HINT_MODTYPE|HINT_MODCOMMENTS|HINT_MPTOPTIONS, NULL);
    m_EditComments.SetFocus();
    m_EditComments.ShowWindow(0);
    m_bInitialized = TRUE;
    return FALSE;
}


void CCtrlComments::RecalcLayout()
//--------------------------------
{
    if ((!m_hWnd) || (!m_EditComments.m_hWnd)) return;
    CRect rcClient, rect;
    GetClientRect(&rcClient);
}


void CCtrlComments::UpdateView(uint32_t dwHint, CObject *pHint) {
    DEBUG_FUNC("dwHint = %x", dwHint);
    if ((pHint == this) || (!m_pSndFile) || (!(dwHint & (HINT_MODCOMMENTS|HINT_MPTOPTIONS|HINT_MODTYPE)))) return;
}

void CCtrlComments::OnCommentsChanged() {
    DEBUG_FUNC("");
}

LRESULT CCtrlComments::OnCustomKeyMsg(WPARAM wParam, LPARAM) {
    if (wParam == kcNull) {
            return NULL;
    }
    return wParam;
}