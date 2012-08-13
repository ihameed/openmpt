#include "stdafx.h"
#include "mptrack.h"
#include "mainfrm.h"
#include "moddoc.h"
#include "globals.h"
#include "ctrl_com.h"
#include "view_com.h"

#include "gui/qt4/comment_view.h"
#include "qwinwidget.h"

BEGIN_MESSAGE_MAP(CCtrlComments, CModControlDlg)
    //{{AFX_MSG_MAP(CCtrlComments)
    ON_MESSAGE(WM_MOD_KEYCOMMAND,	OnCustomKeyMsg)	//rewbs.customKeys
    ON_EN_CHANGE(IDC_EDIT_COMMENTS,		OnCommentsChanged)
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
    qwinwidget = std::make_shared<QWinWidget>(this);
    qwinwidget->setLayout(new QVBoxLayout);
    commentbox = new modplug::gui::qt4::comment_view(m_pSndFile);
    qwinwidget->layout()->addWidget(commentbox);
    CModControlDlg::OnInitDialog();
    // Initialize comments
    m_hFont = NULL;
    m_EditComments.SetMargins(4, 0);
    UpdateView(HINT_MODTYPE|HINT_MODCOMMENTS|HINT_MPTOPTIONS, NULL);
    m_EditComments.SetFocus();
    m_EditComments.ShowWindow(0);
    m_bInitialized = TRUE;
    qwinwidget->show();
    return FALSE;
}


void CCtrlComments::RecalcLayout()
//--------------------------------
{
    if ((!m_hWnd) || (!m_EditComments.m_hWnd)) return;
    CRect rcClient, rect;
    GetClientRect(&rcClient);
    qwinwidget->move(0, 0);
    qwinwidget->resize(rcClient.Width(), rcClient.Height());
}


void CCtrlComments::UpdateView(uint32_t dwHint, CObject *pHint) {
    DEBUG_FUNC("dwHint = %x", dwHint);
    if ((pHint == this) || (!m_pSndFile) || (!(dwHint & (HINT_MODCOMMENTS|HINT_MPTOPTIONS|HINT_MODTYPE)))) return;

    commentbox->legacy_set_comments_from_module(dwHint & HINT_MODCOMMENTS);
}

void CCtrlComments::OnCommentsChanged() {
    DEBUG_FUNC("");

    commentbox->legacy_update_module_comment();
}

LRESULT CCtrlComments::OnCustomKeyMsg(WPARAM wParam, LPARAM) {
    if (wParam == kcNull) {
    	return NULL;
    }
    return wParam;
}