#include "stdafx.h"
#include "moddoc.h"
#include "mainfrm.h"
#include "dlg_misc.h"
#include "ChildFrm.h"
#include "vstplug.h"
#include "legacy_soundlib/midi.h"
#include "version.h"

#pragma warning(disable:4244) //"conversion from 'type1' to 'type2', possible loss of data"

using namespace modplug::tracker;

///////////////////////////////////////////////////////////////////////
// CModTypeDlg


BEGIN_MESSAGE_MAP(CModTypeDlg, CDialog)
    //{{AFX_MSG_MAP(CModTypeDlg)
    ON_COMMAND(IDC_CHECK1,                OnCheck1)
    ON_COMMAND(IDC_CHECK2,                OnCheck2)
    ON_COMMAND(IDC_CHECK3,                OnCheck3)
    ON_COMMAND(IDC_CHECK4,                OnCheck4)
    ON_COMMAND(IDC_CHECK5,                OnCheck5)
// -> CODE#0023
// -> DESC="IT project files (.itp)"
    ON_COMMAND(IDC_CHECK6,                OnCheck6)
    ON_COMMAND(IDC_CHECK_PT1X,        OnCheckPT1x)
    ON_CBN_SELCHANGE(IDC_COMBO1,UpdateDialog)
// -! NEW_FEATURE#0023

    ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTW, 0, 0xFFFF, &CModTypeDlg::OnToolTipNotify)
    ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTA, 0, 0xFFFF, &CModTypeDlg::OnToolTipNotify)

    //}}AFX_MSG_MAP

END_MESSAGE_MAP()


void CModTypeDlg::DoDataExchange(CDataExchange* pDX)
//--------------------------------------------------
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CModTypeDlg)
    DDX_Control(pDX, IDC_COMBO1,                m_TypeBox);
    DDX_Control(pDX, IDC_COMBO2,                m_ChannelsBox);
    DDX_Control(pDX, IDC_COMBO_TEMPOMODE,        m_TempoModeBox);
    DDX_Control(pDX, IDC_COMBO_MIXLEVELS,        m_PlugMixBox);
    DDX_Control(pDX, IDC_CHECK1,                m_CheckBox1);
    DDX_Control(pDX, IDC_CHECK2,                m_CheckBox2);
    DDX_Control(pDX, IDC_CHECK3,                m_CheckBox3);
    DDX_Control(pDX, IDC_CHECK4,                m_CheckBox4);
    DDX_Control(pDX, IDC_CHECK5,                m_CheckBox5);
// -> CODE#0023
// -> DESC="IT project files (.itp)"
    DDX_Control(pDX, IDC_CHECK6,                m_CheckBox6);
// -! NEW_FEATURE#0023
    DDX_Control(pDX, IDC_CHECK_PT1X,        m_CheckBoxPT1x);
    //}}AFX_DATA_MAP
}


BOOL CModTypeDlg::OnInitDialog()
//------------------------------
{
    CDialog::OnInitDialog();
    m_nType = m_pSndFile->GetType();
    m_nChannels = m_pSndFile->GetNumChannels();
    m_dwSongFlags = m_pSndFile->m_dwSongFlags;
    SetDlgItemInt(IDC_ROWSPERBEAT, m_pSndFile->m_nDefaultRowsPerBeat);
    SetDlgItemInt(IDC_ROWSPERMEASURE, m_pSndFile->m_nDefaultRowsPerMeasure);

    m_TypeBox.SetItemData(m_TypeBox.AddString("ProTracker MOD"), MOD_TYPE_MOD);
    m_TypeBox.SetItemData(m_TypeBox.AddString("ScreamTracker S3M"), MOD_TYPE_S3M);
    m_TypeBox.SetItemData(m_TypeBox.AddString("FastTracker XM"), MOD_TYPE_XM);
    m_TypeBox.SetItemData(m_TypeBox.AddString("Impulse Tracker IT"), MOD_TYPE_IT);
// -> CODE#0023
// -> DESC="IT project files (.itp)"
    m_TypeBox.SetItemData(m_TypeBox.AddString("Impulse Tracker Project ITP"), MOD_TYPE_IT);
    m_TypeBox.SetItemData(m_TypeBox.AddString("OpenMPT MPTM"), MOD_TYPE_MPT);
// -! NEW_FEATURE#0023
    switch(m_nType)
    {
    case MOD_TYPE_S3M:        m_TypeBox.SetCurSel(1); break;
    case MOD_TYPE_XM:        m_TypeBox.SetCurSel(2); break;
// -> CODE#0023
// -> DESC="IT project files (.itp)"
    case MOD_TYPE_IT:        m_TypeBox.SetCurSel(m_pSndFile->m_dwSongFlags & SONG_ITPROJECT ? 4 : 3); break;
// -! NEW_FEATURE#0023
    case MOD_TYPE_MPT:        m_TypeBox.SetCurSel(5); break;
    default:                        m_TypeBox.SetCurSel(0); break;
    }

    UpdateChannelCBox();

    m_TempoModeBox.SetItemData(m_TempoModeBox.AddString("Classic"), tempo_mode_classic);
    m_TempoModeBox.SetItemData(m_TempoModeBox.AddString("Alternative"), tempo_mode_alternative);
    m_TempoModeBox.SetItemData(m_TempoModeBox.AddString("Modern (accurate)"), tempo_mode_modern);
    m_TempoModeBox.SetCurSel(0);
    for(int i = m_TempoModeBox.GetCount(); i > 0; i--)
    {
            if(m_TempoModeBox.GetItemData(i) == m_pSndFile->m_nTempoMode)
            {
                    m_TempoModeBox.SetCurSel(i);
                    break;
            }
    }

    m_PlugMixBox.SetItemData(m_PlugMixBox.AddString("OpenMPT 1.17RC3"),                mixLevels_117RC3);
    if(m_pSndFile->m_nMixLevels == mixLevels_117RC2)        // Only shown for backwards compatibility with existing tunes
            m_PlugMixBox.SetItemData(m_PlugMixBox.AddString("OpenMPT 1.17RC2"),        mixLevels_117RC2);
    if(m_pSndFile->m_nMixLevels == mixLevels_117RC1)        // Dito
            m_PlugMixBox.SetItemData(m_PlugMixBox.AddString("OpenMPT 1.17RC1"),        mixLevels_117RC1);
    m_PlugMixBox.SetItemData(m_PlugMixBox.AddString("Original (MPT 1.16)"),        mixLevels_original);
    m_PlugMixBox.SetItemData(m_PlugMixBox.AddString("Compatible"),                        mixLevels_compatible);
    //m_PlugMixBox.SetItemData(m_PlugMixBox.AddString("Test"),                                mixLevels_Test);
    m_PlugMixBox.SetCurSel(0);
    for(int i = m_PlugMixBox.GetCount(); i > 0; i--)
    {
            if(m_PlugMixBox.GetItemData(i) == m_pSndFile->m_nMixLevels)
            {
                    m_PlugMixBox.SetCurSel(i);
                    break;
            }
    }

    SetDlgItemText(IDC_TEXT_CREATEDWITH, "Created with:");
    SetDlgItemText(IDC_TEXT_SAVEDWITH, "Last saved with:");

    SetDlgItemText(IDC_EDIT_CREATEDWITH, MptVersion::ToStr(m_pSndFile->m_dwCreatedWithVersion));
    SetDlgItemText(IDC_EDIT_SAVEDWITH, MptVersion::ToStr(m_pSndFile->m_dwLastSavedWithVersion));

    UpdateDialog();

    EnableToolTips(TRUE);
    return TRUE;
}


void CModTypeDlg::UpdateChannelCBox()
//-----------------------------------
{
    const MODTYPE type = m_TypeBox.GetItemData(m_TypeBox.GetCurSel());
    modplug::tracker::chnindex_t currChanSel = m_ChannelsBox.GetItemData(m_ChannelsBox.GetCurSel());
    const modplug::tracker::chnindex_t minChans = module_renderer::GetModSpecifications(type).channelsMin;
    const modplug::tracker::chnindex_t maxChans = module_renderer::GetModSpecifications(type).channelsMax;
    if(m_ChannelsBox.GetCount() < 1
            ||
       m_ChannelsBox.GetItemData(0) != minChans
            ||
       m_ChannelsBox.GetItemData(m_ChannelsBox.GetCount()-1) != maxChans
      )
    {
            if(m_ChannelsBox.GetCount() < 1) currChanSel = m_nChannels;
            char s[256];
            m_ChannelsBox.ResetContent();
            for (modplug::tracker::chnindex_t i=minChans; i<=maxChans; i++)
            {
                    wsprintf(s, "%d Channel%s", i, (i != 1) ? "s" : "");
                    m_ChannelsBox.SetItemData(m_ChannelsBox.AddString(s), i);
            }
            if(currChanSel > maxChans)
                    m_ChannelsBox.SetCurSel(m_ChannelsBox.GetCount()-1);
            else
                    m_ChannelsBox.SetCurSel(currChanSel-minChans);
    }
}


void CModTypeDlg::UpdateDialog()
//------------------------------
{
    UpdateChannelCBox();

    m_CheckBox1.SetCheck((m_pSndFile->m_dwSongFlags & SONG_LINEARSLIDES) ? MF_CHECKED : 0);
    m_CheckBox2.SetCheck((m_pSndFile->m_dwSongFlags & SONG_FASTVOLSLIDES) ? MF_CHECKED : 0);
    m_CheckBox3.SetCheck((m_pSndFile->m_dwSongFlags & SONG_ITOLDEFFECTS) ? MF_CHECKED : 0);
    m_CheckBox4.SetCheck((m_pSndFile->m_dwSongFlags & SONG_ITCOMPATGXX) ? MF_CHECKED : 0);
    m_CheckBox5.SetCheck((m_pSndFile->m_dwSongFlags & SONG_EXFILTERRANGE) ? MF_CHECKED : 0);
    m_CheckBoxPT1x.SetCheck((m_pSndFile->m_dwSongFlags & SONG_PT1XMODE) ? MF_CHECKED : 0);

// -> CODE#0023
// -> DESC="IT project files (.itp)"
    m_CheckBox6.SetCheck((m_pSndFile->m_dwSongFlags & SONG_ITPEMBEDIH) ? MF_CHECKED : 0);
// -! NEW_FEATURE#0023

    m_CheckBox1.EnableWindow((m_pSndFile->m_nType & (MOD_TYPE_XM|MOD_TYPE_IT|MOD_TYPE_MPT)) ? TRUE : FALSE);
    m_CheckBox2.EnableWindow((m_pSndFile->m_nType == MOD_TYPE_S3M) ? TRUE : FALSE);
    m_CheckBox3.EnableWindow((m_pSndFile->m_nType & (MOD_TYPE_IT|MOD_TYPE_MPT)) ? TRUE : FALSE);
    m_CheckBox4.EnableWindow((m_pSndFile->m_nType & (MOD_TYPE_IT|MOD_TYPE_MPT)) ? TRUE : FALSE);
    m_CheckBox5.EnableWindow((m_pSndFile->m_nType & (MOD_TYPE_XM|MOD_TYPE_IT|MOD_TYPE_MPT)) ? TRUE : FALSE);
    m_CheckBoxPT1x.EnableWindow((m_pSndFile->m_nType & (MOD_TYPE_MOD)) ? TRUE : FALSE);

// -> CODE#0023
// -> DESC="IT project files (.itp)"
    m_CheckBox6.EnableWindow(m_TypeBox.GetCurSel() == 4 ? TRUE : FALSE);
// -! NEW_FEATURE#0023

    const bool XMorITorMPT = ((m_TypeBox.GetItemData(m_TypeBox.GetCurSel()) & (MOD_TYPE_XM | MOD_TYPE_IT | MOD_TYPE_MPT)) != 0);
    const bool ITorMPT = ((m_TypeBox.GetItemData(m_TypeBox.GetCurSel()) & (MOD_TYPE_IT | MOD_TYPE_MPT)) != 0);

    // Misc Flags
    if(ITorMPT)
    {
            GetDlgItem(IDC_CHK_COMPATPLAY)->SetWindowText(_T("More Impulse Tracker compatible playback"));
    } else
    {
            GetDlgItem(IDC_CHK_COMPATPLAY)->SetWindowText(_T("More Fasttracker 2 compatible playback"));
    }

    GetDlgItem(IDC_CHK_COMPATPLAY)->ShowWindow(XMorITorMPT);
    GetDlgItem(IDC_CHK_MIDICCBUG)->ShowWindow(XMorITorMPT);
    GetDlgItem(IDC_CHK_OLDRANDOM)->ShowWindow(ITorMPT);

    CheckDlgButton(IDC_CHK_COMPATPLAY, m_pSndFile->GetModFlag(MSF_COMPATIBLE_PLAY));
    CheckDlgButton(IDC_CHK_MIDICCBUG, m_pSndFile->GetModFlag(MSF_MIDICC_BUGEMULATION));
    CheckDlgButton(IDC_CHK_OLDRANDOM, m_pSndFile->GetModFlag(MSF_OLDVOLSWING));

    // Mixmode Box
    GetDlgItem(IDC_TEXT_MIXMODE)->ShowWindow(XMorITorMPT);
    m_PlugMixBox.ShowWindow(XMorITorMPT);

    // Tempo mode box
    m_TempoModeBox.ShowWindow(XMorITorMPT);
    GetDlgItem(IDC_ROWSPERBEAT)->ShowWindow(XMorITorMPT);
    GetDlgItem(IDC_ROWSPERMEASURE)->ShowWindow(XMorITorMPT);
    GetDlgItem(IDC_TEXT_ROWSPERBEAT)->ShowWindow(XMorITorMPT);
    GetDlgItem(IDC_TEXT_ROWSPERMEASURE)->ShowWindow(XMorITorMPT);
    GetDlgItem(IDC_TEXT_TEMPOMODE)->ShowWindow(XMorITorMPT);
    GetDlgItem(IDC_FRAME_TEMPOMODE)->ShowWindow(XMorITorMPT);

    // Version info
    GetDlgItem(IDC_FRAME_MPTVERSION)->ShowWindow(XMorITorMPT);
    GetDlgItem(IDC_TEXT_CREATEDWITH)->ShowWindow(XMorITorMPT);
    GetDlgItem(IDC_TEXT_SAVEDWITH)->ShowWindow(XMorITorMPT);
    GetDlgItem(IDC_EDIT_CREATEDWITH)->ShowWindow(XMorITorMPT);
    GetDlgItem(IDC_EDIT_SAVEDWITH)->ShowWindow(XMorITorMPT);

    // Window height - some parts of the dialog won't be visible for all formats
    RECT rWindow;
    GetWindowRect(&rWindow);

    UINT iHeight;
    int nItemID = (XMorITorMPT) ? IDC_FRAME_MPTVERSION : IDC_FRAME_MODFLAGS;
    RECT rFrame;
    GetDlgItem(nItemID)->GetWindowRect(&rFrame);
    iHeight = rFrame.bottom - rWindow.top + 12;
    MoveWindow(rWindow.left, rWindow.top, rWindow.right - rWindow.left, iHeight);

}


void CModTypeDlg::OnCheck1()
//--------------------------
{
    if (m_CheckBox1.GetCheck())
            m_pSndFile->m_dwSongFlags |= SONG_LINEARSLIDES;
    else
            m_pSndFile->m_dwSongFlags &= ~SONG_LINEARSLIDES;
}


void CModTypeDlg::OnCheck2()
//--------------------------
{
    if (m_CheckBox2.GetCheck())
            m_pSndFile->m_dwSongFlags |= SONG_FASTVOLSLIDES;
    else
            m_pSndFile->m_dwSongFlags &= ~SONG_FASTVOLSLIDES;
}


void CModTypeDlg::OnCheck3()
//--------------------------
{
    if (m_CheckBox3.GetCheck())
            m_pSndFile->m_dwSongFlags |= SONG_ITOLDEFFECTS;
    else
            m_pSndFile->m_dwSongFlags &= ~SONG_ITOLDEFFECTS;
}


void CModTypeDlg::OnCheck4()
//--------------------------
{
    if (m_CheckBox4.GetCheck())
            m_pSndFile->m_dwSongFlags |= SONG_ITCOMPATGXX;
    else
            m_pSndFile->m_dwSongFlags &= ~SONG_ITCOMPATGXX;
}


void CModTypeDlg::OnCheck5()
//--------------------------
{
    if (m_CheckBox5.GetCheck())
            m_pSndFile->m_dwSongFlags |= SONG_EXFILTERRANGE;
    else
            m_pSndFile->m_dwSongFlags &= ~SONG_EXFILTERRANGE;
}


void CModTypeDlg::OnCheck6()
//--------------------------
{
    if (m_CheckBox6.GetCheck())
            m_pSndFile->m_dwSongFlags |= SONG_ITPEMBEDIH;
    else
            m_pSndFile->m_dwSongFlags &= ~SONG_ITPEMBEDIH;
}

void CModTypeDlg::OnCheckPT1x()
//-----------------------------
{
    if (m_CheckBoxPT1x.GetCheck())
            m_pSndFile->m_dwSongFlags |= SONG_PT1XMODE;
    else
            m_pSndFile->m_dwSongFlags &= ~SONG_PT1XMODE;
}


bool CModTypeDlg::VerifyData()
//----------------------------
{

    int temp_nRPB = GetDlgItemInt(IDC_ROWSPERBEAT);
    int temp_nRPM = GetDlgItemInt(IDC_ROWSPERMEASURE);
    if ((temp_nRPB > temp_nRPM))
    {
            ::AfxMessageBox("Error: Rows per measure must be greater than or equal rows per beat.", MB_OK|MB_ICONEXCLAMATION);
            GetDlgItem(IDC_ROWSPERMEASURE)->SetFocus();
            return false;
    }

    int sel = m_ChannelsBox.GetItemData(m_ChannelsBox.GetCurSel());
    int type = m_TypeBox.GetItemData(m_TypeBox.GetCurSel());

    modplug::tracker::chnindex_t maxChans = module_renderer::GetModSpecifications(type).channelsMax;

    if (sel > maxChans)
    {
            CString error;
            error.Format("Error: Max number of channels for this type is %d", maxChans);
            ::AfxMessageBox(error, MB_OK|MB_ICONEXCLAMATION);
            return FALSE;
    }

    if(maxChans < m_pSndFile->GetNumChannels())
    {
            if(MessageBox("New module type supports less channels than currently used, and reducing channel number is required. Continue?", "", MB_OKCANCEL) != IDOK)
                    return false;
    }

    return true;
}

void CModTypeDlg::OnOK()
//----------------------
{
    if (!VerifyData())
            return;

    int sel = m_TypeBox.GetCurSel();
    if (sel >= 0)
    {
            m_nType = m_TypeBox.GetItemData(sel);
// -> CODE#0023
// -> DESC="IT project files (.itp)"
            if(m_pSndFile->m_dwSongFlags & SONG_ITPROJECT && sel != 4)
            {
                    m_pSndFile->m_dwSongFlags &= ~SONG_ITPROJECT;
                    m_pSndFile->m_dwSongFlags &= ~SONG_ITPEMBEDIH;
            }
            if(sel == 4) m_pSndFile->m_dwSongFlags |= SONG_ITPROJECT;
// -! NEW_FEATURE#0023
    }
    sel = m_ChannelsBox.GetCurSel();
    if (sel >= 0)
    {
            m_nChannels = m_ChannelsBox.GetItemData(sel);
            //if (m_nType & MOD_TYPE_XM) m_nChannels = (m_nChannels+1) & 0xFE;
    }

    sel = m_TempoModeBox.GetCurSel();
    if (sel >= 0)
    {
            m_pSndFile->m_nTempoMode = m_TempoModeBox.GetItemData(sel);
    }

    sel = m_PlugMixBox.GetCurSel();
    if (sel >= 0)
    {
            m_pSndFile->m_nMixLevels = m_PlugMixBox.GetItemData(sel);
            m_pSndFile->m_pConfig->SetMixLevels(m_pSndFile->m_nMixLevels);
            m_pSndFile->RecalculateGainForAllPlugs();
    }

    if(m_nType & (MOD_TYPE_IT | MOD_TYPE_MPT | MOD_TYPE_XM))
    {
            m_pSndFile->SetModFlags(0);
            if(IsDlgButtonChecked(IDC_CHK_COMPATPLAY)) m_pSndFile->SetModFlag(MSF_COMPATIBLE_PLAY, true);
            if(IsDlgButtonChecked(IDC_CHK_MIDICCBUG)) m_pSndFile->SetModFlag(MSF_MIDICC_BUGEMULATION, true);
            if(IsDlgButtonChecked(IDC_CHK_OLDRANDOM)) m_pSndFile->SetModFlag(MSF_OLDVOLSWING, true);
    }

    m_pSndFile->m_nDefaultRowsPerBeat    = GetDlgItemInt(IDC_ROWSPERBEAT);
    m_pSndFile->m_nDefaultRowsPerMeasure = GetDlgItemInt(IDC_ROWSPERMEASURE);

    m_pSndFile->SetupITBidiMode();

    CDialog::OnOK();
}

void CModTypeDlg::OnCancel()
//--------------------------
{
    // Reset mod flags
    m_pSndFile->m_dwSongFlags = m_dwSongFlags;
    CDialog::OnCancel();
}


BOOL CModTypeDlg::OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult)
//-------------------------------------------------------------------------
{
    UNREFERENCED_PARAMETER(id);
    UNREFERENCED_PARAMETER(pResult);

    // need to handle both ANSI and UNICODE versions of the message
    TOOLTIPTEXTA* pTTTA = (TOOLTIPTEXTA*)pNMHDR;
    TOOLTIPTEXTW* pTTTW = (TOOLTIPTEXTW*)pNMHDR;
    CStringA strTipText = "";
    uintptr_t nID = pNMHDR->idFrom;
    if (pNMHDR->code == TTN_NEEDTEXTA && (pTTTA->uFlags & TTF_IDISHWND) ||
            pNMHDR->code == TTN_NEEDTEXTW && (pTTTW->uFlags & TTF_IDISHWND))
    {
            // idFrom is actually the HWND of the tool
            nID = ::GetDlgCtrlID((HWND)nID);
    }

    switch(nID)
    {
    case IDC_CHECK1:
            strTipText = "Note slides always slide the same amount, not depending on the sample frequency.";
            break;
    case IDC_CHECK2:
            strTipText = "Old ScreamTracker 3 volume slide behaviour (not recommended).";
            break;
    case IDC_CHECK3:
            strTipText = "Play some effects like in early versions of Impulse Tracker (not recommended).";
            break;
    case IDC_CHECK4:
            strTipText = "Gxx and Exx/Fxx won't share effect memory. Gxx resets instrument envelopes.";
            break;
    case IDC_CHECK5:
            strTipText = "The resonant filter's frequency range is increased from about 4KHz to 10KHz.";
            break;
    case IDC_CHECK6:
            strTipText = "The instrument settings of the external ITI files will be ignored.";
            break;
    case IDC_CHECK_PT1X:
            strTipText = "Ignore pan fx, use on-the-fly sample swapping, enforce Amiga frequency limits.";
            break;
    case IDC_COMBO_MIXLEVELS:
            strTipText = "Mixing method of sample and VST levels.";
            break;
    case IDC_CHK_COMPATPLAY:
            strTipText = "Play commands as the original tracker would play them (recommended)";
            break;
    case IDC_CHK_MIDICCBUG:
            strTipText = "Emulate an old bug which sent wrong volume messages to VSTis (not recommended)";
            break;
    case IDC_CHK_OLDRANDOM:
            strTipText = "Use old (buggy) random volume / panning variation algorithm (not recommended)";
            break;
    }

    if (pNMHDR->code == TTN_NEEDTEXTA)
    {
            //strncpy_s(pTTTA->szText, sizeof(pTTTA->szText), strTipText,
            //        strTipText.GetLength() + 1);
            // 80 chars bad_max?!
            strncpy(pTTTA->szText, strTipText, bad_min(strTipText.GetLength() + 1, CountOf(pTTTA->szText) - 1));
    }
    else
    {
            ::MultiByteToWideChar(CP_ACP , 0, strTipText, strTipText.GetLength() + 1,
                    pTTTW->szText, CountOf(pTTTW->szText));
    }

    return TRUE;
}


//////////////////////////////////////////////////////////////////////////////
// CShowLogDlg

BEGIN_MESSAGE_MAP(CShowLogDlg, CDialog)
    //{{AFX_MSG_MAP(CShowLogDlg)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()



void CShowLogDlg::DoDataExchange(CDataExchange* pDX)
//--------------------------------------------------
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CShowLogDlg)
    DDX_Control(pDX, IDC_EDIT_LOG,                m_EditLog);
    //}}AFX_DATA_MAP
}


BOOL CShowLogDlg::OnInitDialog()
//------------------------------
{
    CDialog::OnInitDialog();
    if (m_lpszTitle) SetWindowText(m_lpszTitle);
    m_EditLog.SetSel(0, -1);
    m_EditLog.ReplaceSel(m_lpszLog);
    m_EditLog.SetFocus();
    m_EditLog.SetSel(0, 0);
    return FALSE;
}


UINT CShowLogDlg::ShowLog(LPCSTR pszLog, LPCSTR lpszTitle)
//--------------------------------------------------------
{
    m_lpszLog = pszLog;
    m_lpszTitle = lpszTitle;
    return DoModal();
}


///////////////////////////////////////////////////////////
// CRemoveChannelsDlg

//rewbs.removeChansDlgCleanup
void CRemoveChannelsDlg::DoDataExchange(CDataExchange* pDX)
//--------------------------------------------------
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CShowLogDlg)
    DDX_Control(pDX, IDC_REMCHANSLIST,                m_RemChansList);
    //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CRemoveChannelsDlg, CDialog)
    //{{AFX_MSG_MAP(CRemoveChannelsDlg)
    ON_LBN_SELCHANGE(IDC_REMCHANSLIST,                OnChannelChanged)
    //}}AFX_MSG_MAP
//    ON_WM_SIZE()
END_MESSAGE_MAP()



BOOL CRemoveChannelsDlg::OnInitDialog()
//-------------------------------------
{
    CHAR label[bad_max(100, 20 + MAX_CHANNELNAME)];
    CDialog::OnInitDialog();
    for (UINT n = 0; n < m_nChannels; n++)
    {
            if(m_pSndFile->ChnSettings[n].szName[0] >= 0x20)
                    wsprintf(label, "Channel %d: %s", (n + 1), m_pSndFile->ChnSettings[n].szName);
            else
                    wsprintf(label, "Channel %d", n + 1);

            m_RemChansList.SetItemData(m_RemChansList.AddString(label), n);
            if (!m_bKeepMask[n]) m_RemChansList.SetSel(n);
    }

    if (m_nRemove > 0) {
            wsprintf(label, "Select %d channel%s to remove:", m_nRemove, (m_nRemove != 1) ? "s" : "");
    } else {
            wsprintf(label, "Select channels to remove (the minimum number of remaining channels is %d)", m_pSndFile->GetModSpecifications().channelsMin);
    }

    SetDlgItemText(IDC_QUESTION1, label);
    if(GetDlgItem(IDCANCEL)) GetDlgItem(IDCANCEL)->ShowWindow(m_ShowCancel);

    OnChannelChanged();
    return TRUE;
}


void CRemoveChannelsDlg::OnOK()
//-----------------------------
{
    int nCount = m_RemChansList.GetSelCount();
    CArray<int,int> aryListBoxSel;
    aryListBoxSel.SetSize(nCount);
    m_RemChansList.GetSelItems(nCount, aryListBoxSel.GetData());

    m_bKeepMask.assign(m_nChannels, true);
    for (int n = 0; n < nCount; n++)
    {
            m_bKeepMask[aryListBoxSel[n]] = false;
    }
    if ((static_cast<UINT>(nCount) == m_nRemove && nCount > 0)  || (m_nRemove == 0 && (m_pSndFile->GetNumChannels() >= nCount + m_pSndFile->GetModSpecifications().channelsMin)))
            CDialog::OnOK();
    else
            CDialog::OnCancel();
}


void CRemoveChannelsDlg::OnChannelChanged()
//-----------------------------------------
{
    UINT nr = 0;
    nr = m_RemChansList.GetSelCount();
    GetDlgItem(IDOK)->EnableWindow(((nr == m_nRemove && nr >0)  || (m_nRemove == 0 && (m_pSndFile->GetNumChannels() >= nr + m_pSndFile->GetModSpecifications().channelsMin) && nr > 0)) ? TRUE : FALSE);
}
//end rewbs.removeChansDlgCleanup


////////////////////////////////////////////////////////////////////////////////////////////
// Keyboard Control

const uint8_t whitetab[7] = {0,2,4,5,7,9,11};
const uint8_t blacktab[7] = {0xff,1,3,0xff,6,8,10};

BEGIN_MESSAGE_MAP(CKeyboardControl, CWnd)
    ON_WM_PAINT()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
END_MESSAGE_MAP()


void CKeyboardControl::OnPaint()
//------------------------------
{
    HGDIOBJ oldpen, oldbrush;
    CRect rcClient, rect;
    CPaintDC dc(this);
    HDC hdc = dc.m_hDC;
    HBRUSH brushRed;

    if (!m_nOctaves) m_nOctaves = 1;
    GetClientRect(&rcClient);
    rect = rcClient;
    oldpen = ::SelectObject(hdc, CMainFrame::penBlack);
    oldbrush = ::SelectObject(hdc, CMainFrame::brushWhite);
    brushRed = ::CreateSolidBrush(RGB(0xFF, 0, 0));
    // White notes
    for (UINT note=0; note<m_nOctaves*7; note++)
    {
            rect.right = ((note + 1) * rcClient.Width()) / (m_nOctaves * 7);
            int val = (note/7) * 12 + whitetab[note % 7];
            if (val == m_nSelection) ::SelectObject(hdc, CMainFrame::brushGray);
            dc.Rectangle(&rect);
            if (val == m_nSelection) ::SelectObject(hdc, CMainFrame::brushWhite);
            if ((val < NoteMax) && (KeyFlags[val]))
            {
                    ::SelectObject(hdc, brushRed);
                    dc.Ellipse(rect.left+2, rect.bottom - (rect.right-rect.left) + 2, rect.right-2, rect.bottom-2);
                    ::SelectObject(hdc, CMainFrame::brushWhite);
            }
            rect.left = rect.right - 1;
    }
    // Black notes
    ::SelectObject(hdc, CMainFrame::brushBlack);
    rect = rcClient;
    rect.bottom -= rcClient.Height() / 3;
    for (UINT nblack=0; nblack<m_nOctaves*7; nblack++)
    {
            switch(nblack % 7)
            {
            case 1:
            case 2:
            case 4:
            case 5:
            case 6:
                    {
                            rect.left = (nblack * rcClient.Width()) / (m_nOctaves * 7);
                            rect.right = rect.left;
                            int delta = rcClient.Width() / (m_nOctaves * 7 * 3);
                            rect.left -= delta;
                            rect.right += delta;
                            int val = (nblack/7)*12 + blacktab[nblack%7];
                            if (val == m_nSelection) ::SelectObject(hdc, CMainFrame::brushGray);
                            dc.Rectangle(&rect);
                            if (val == m_nSelection) ::SelectObject(hdc, CMainFrame::brushBlack);
                            if ((val < NoteMax) && (KeyFlags[val]))
                            {
                                    ::SelectObject(hdc, brushRed);
                                    dc.Ellipse(rect.left, rect.bottom - (rect.right-rect.left), rect.right, rect.bottom);
                                    ::SelectObject(hdc, CMainFrame::brushBlack);
                            }
                    }
                    break;
            }
    }
    if (oldpen) ::SelectObject(hdc, oldpen);
    if (oldbrush) ::SelectObject(hdc, oldbrush);
}


void CKeyboardControl::OnMouseMove(UINT, CPoint point)
//----------------------------------------------------
{
    int sel = -1, xmin, xmax;
    CRect rcClient, rect;
    if (!m_nOctaves) m_nOctaves = 1;
    GetClientRect(&rcClient);
    rect = rcClient;
    xmin = rcClient.right;
    xmax = rcClient.left;
    // White notes
    for (UINT note=0; note<m_nOctaves*7; note++)
    {
            int val = (note/7)*12 + whitetab[note % 7];
            rect.right = ((note + 1) * rcClient.Width()) / (m_nOctaves * 7);
            if (val == m_nSelection)
            {
                    if (rect.left < xmin) xmin = rect.left;
                    if (rect.right > xmax) xmax = rect.right;
            }
            if (rect.PtInRect(point))
            {
                    sel = val;
                    if (rect.left < xmin) xmin = rect.left;
                    if (rect.right > xmax) xmax = rect.right;
            }
            rect.left = rect.right - 1;
    }
    // Black notes
    rect = rcClient;
    rect.bottom -= rcClient.Height() / 3;
    for (UINT nblack=0; nblack<m_nOctaves*7; nblack++)
    {
            switch(nblack % 7)
            {
            case 1:
            case 2:
            case 4:
            case 5:
            case 6:
                    {
                            int val = (nblack/7)*12 + blacktab[nblack % 7];
                            rect.left = (nblack * rcClient.Width()) / (m_nOctaves * 7);
                            rect.right = rect.left;
                            int delta = rcClient.Width() / (m_nOctaves * 7 * 3);
                            rect.left -= delta;
                            rect.right += delta;
                            if (val == m_nSelection)
                            {
                                    if (rect.left < xmin) xmin = rect.left;
                                    if (rect.right > xmax) xmax = rect.right;
                            }
                            if (rect.PtInRect(point))
                            {
                                    sel = val;
                                    if (rect.left < xmin) xmin = rect.left;
                                    if (rect.right > xmax) xmax = rect.right;
                            }
                    }
                    break;
            }
    }
    // Check for selection change
    if (sel != m_nSelection)
    {
            m_nSelection = sel;
            rcClient.left = xmin;
            rcClient.right = xmax;
            InvalidateRect(&rcClient, FALSE);
            if ((m_bCursorNotify) && (m_hParent))
            {
                    ::PostMessage(m_hParent, WM_MOD_KBDNOTIFY, KBDNOTIFY_MOUSEMOVE, m_nSelection);
            }
    }
    if (sel >= 0)
    {
            if (!m_bCapture)
            {
                    m_bCapture = TRUE;
                    SetCapture();
            }
    } else
    {
            if (m_bCapture)
            {
                    m_bCapture = FALSE;
                    ReleaseCapture();
            }
    }
}


void CKeyboardControl::OnLButtonDown(UINT, CPoint)
//------------------------------------------------
{
    if ((m_nSelection != -1) && (m_hParent))
    {
            ::SendMessage(m_hParent, WM_MOD_KBDNOTIFY, KBDNOTIFY_LBUTTONDOWN, m_nSelection);
    }
}


void CKeyboardControl::OnLButtonUp(UINT, CPoint)
//----------------------------------------------
{
    if ((m_nSelection != -1) && (m_hParent))
    {
            ::SendMessage(m_hParent, WM_MOD_KBDNOTIFY, KBDNOTIFY_LBUTTONUP, m_nSelection);
    }
}



///////////////////////////////////////////////////////////////////////////////////////
// Messagebox with 'don't show again'-option.

//===================================
class CMsgBoxHidable : public CDialog
//===================================
{
public:
    CMsgBoxHidable(LPCTSTR strMsg, bool checkStatus = true, CWnd* pParent = NULL);
    enum { IDD = IDD_MSGBOX_HIDABLE };

    int m_nCheckStatus;
    LPCTSTR m_StrMsg;
protected:
    virtual void DoDataExchange(CDataExchange* pDX);   // DDX/DDV support
    virtual BOOL OnInitDialog();
};


struct MsgBoxHidableMessage
//=========================
{
    LPCTSTR strMsg;
    uint32_t nMask;
    bool bDefaultDontShowAgainStatus; // true for don't show again, false for show again.
};

const MsgBoxHidableMessage HidableMessages[] =
{
    {TEXT("Tip: To create ProTracker compatible MOD-files, try compatibility export from File-menu."), 1, true},
    {TEXT("Tip: To create IT-files without MPT-specific extensions included, try compatibility export from File-menu."), 1 << 1, true},
    {TEXT("Press OK to apply signed/unsigned conversion\n (note: this often significantly increases volume level)"), 1 << 2, false},
    {TEXT("Tip: To create XM-files without MPT-specific extensions included, try compatibility export from File-menu."), 1 << 1, true},
};

static_assert(CountOf(HidableMessages) == enMsgBoxHidableMessage_count,
              "the number of HidableMessages must be equal to enMsgBoxherpenderpen");

// Messagebox with 'don't show this again'-checkbox. Uses parameter 'enMsg'
// to get the needed information from message array, and updates the variable that
// controls the show/don't show-flags.
void MsgBoxHidable(enMsgBoxHidableMessage enMsg)
//----------------------------------------------
{
    // Check whether the message should be shown.
    if((CMainFrame::gnMsgBoxVisiblityFlags & HidableMessages[enMsg].nMask) == 0)
            return;

    const LPCTSTR strMsg = HidableMessages[enMsg].strMsg;
    const uint32_t mask = HidableMessages[enMsg].nMask;
    const bool defaulCheckStatus = HidableMessages[enMsg].bDefaultDontShowAgainStatus;

    // Show dialog.
    CMsgBoxHidable dlg(strMsg, defaulCheckStatus);
    dlg.DoModal();

    // Update visibility flags.
    if(dlg.m_nCheckStatus == BST_CHECKED)
            CMainFrame::gnMsgBoxVisiblityFlags &= ~mask;
    else
            CMainFrame::gnMsgBoxVisiblityFlags |= mask;
}


CMsgBoxHidable::CMsgBoxHidable(LPCTSTR strMsg, bool checkStatus, CWnd* pParent)
    :        CDialog(CMsgBoxHidable::IDD, pParent),
            m_StrMsg(strMsg),
            m_nCheckStatus((checkStatus) ? BST_CHECKED : BST_UNCHECKED)
//----------------------------------------------------------------------------
{}

BOOL CMsgBoxHidable::OnInitDialog()
//----------------------------------
{
    CDialog::OnInitDialog();
    SetDlgItemText(IDC_MESSAGETEXT, m_StrMsg);
    SetWindowText(AfxGetAppName());
    return TRUE;
}

void CMsgBoxHidable::DoDataExchange(CDataExchange* pDX)
//------------------------------------------------------
{
    CDialog::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_DONTSHOWAGAIN, m_nCheckStatus);
}


/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

const LPCTSTR szNullNote = TEXT("...");
const LPCTSTR szUnknownNote = TEXT("???");


LPCTSTR GetNoteStr(const modplug::tracker::note_t nNote)
//----------------------------------------------
{
    if(nNote == 0)
            return szNullNote;

    if(nNote >= 1 && nNote <= NoteMax)
    {
            return szDefaultNoteNames[nNote-1];
    }
    else if(nNote >= NoteMinSpecial && nNote <= NoteMaxSpecial)
    {
            return szSpecialNoteNames[nNote - NoteMinSpecial];
    }
    else
            return szUnknownNote;
}


void AppendNotesToControl(CComboBox& combobox, const modplug::tracker::note_t noteStart, const modplug::tracker::note_t noteEnd)
//------------------------------------------------------------------------------------------------------------------
{
    const modplug::tracker::note_t upperLimit = bad_min(CountOf(szDefaultNoteNames)-1, noteEnd);
    for(modplug::tracker::note_t note = noteStart; note <= upperLimit; ++note)
            combobox.SetItemData(combobox.AddString(szDefaultNoteNames[note]), note);
}


void AppendNotesToControlEx(CComboBox& combobox, const module_renderer* const pSndFile /* = nullptr*/, const modplug::tracker::instrumentindex_t nInstr/* = MAX_INSTRUMENTS*/)
//----------------------------------------------------------------------------------------------------------------------------------
{
    const modplug::tracker::note_t noteStart = (pSndFile != nullptr) ? pSndFile->GetModSpecifications().noteMin : 1;
    const modplug::tracker::note_t noteEnd = (pSndFile != nullptr) ? pSndFile->GetModSpecifications().noteMax : NoteMax;
    for(modplug::tracker::note_t nNote = noteStart; nNote <= noteEnd; nNote++)
    {
            if(pSndFile != nullptr && nInstr != MAX_INSTRUMENTS)
                    combobox.SetItemData(combobox.AddString(pSndFile->GetNoteName(nNote, nInstr).c_str()), nNote);
            else
                    combobox.SetItemData(combobox.AddString(szDefaultNoteNames[nNote-1]), nNote);
    }
    for(modplug::tracker::note_t nNote = NoteMinSpecial-1; nNote++ < NoteMaxSpecial;)
    {
            if(pSndFile == nullptr || pSndFile->GetModSpecifications().HasNote(nNote) == true)
                    combobox.SetItemData(combobox.AddString(szSpecialNoteNames[nNote-NoteMinSpecial]), nNote);
    }
}
