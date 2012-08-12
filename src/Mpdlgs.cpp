#include "stdafx.h"
#include "mptrack.h"
#include "legacy_soundlib/sndfile.h"
#include "mainfrm.h"
#include "legacy_soundlib/dlsbank.h"
#include "mpdlgs.h"
#include "moptions.h"
#include "moddoc.h"
#include ".\mpdlgs.h"

#include "pervasives/pervasives.h"
using namespace modplug::pervasives;

#define str_preampChangeNote GetStrI18N(_TEXT("Note: The Pre-Amp setting affects sample volume only. Changing it may cause undesired effects on volume balance between sample based instruments and plugin instruments.\nIn other words: Don't touch this slider unless you know what you are doing."))




////////////////////////////////////////////////////////////////////////////////
//
// EQ Globals
//

enum {
    EQPRESET_FLAT=0,
    EQPRESET_JAZZ,
    EQPRESET_POP,
    EQPRESET_ROCK,
    EQPRESET_CONCERT,
    EQPRESET_CLEAR,
};


#define EQ_MAX_FREQS    5

const UINT gEqBandFreqs[EQ_MAX_FREQS*MAX_EQ_BANDS] =
{
    100, 125, 150, 200, 250,
    300, 350, 400, 450, 500,
    600, 700, 800, 900, 1000,
    1250, 1500, 1750, 2000, 2500,
    3000, 3500, 4000, 4500, 5000,
    6000, 7000, 8000, 9000, 10000
};


const EQPRESET gEQPresets[6] =
{
    { "Flat",	{16,16,16,16,16,16}, { 125, 300, 600, 1250, 4000, 8000 } },	// Flat
    { "Jazz",	{16,16,24,20,20,14}, { 125, 300, 600, 1250, 4000, 8000 } },	// Jazz
    { "Pop",	{24,16,16,21,16,26}, { 125, 300, 600, 1250, 4000, 8000 } },	// Pop
    { "Rock",	{24,16,24,16,24,22}, { 125, 300, 600, 1250, 4000, 8000 } },	// Rock
    { "Concert",{22,18,26,16,22,16}, { 125, 300, 600, 1250, 4000, 8000 } },	// Concert
    { "Clear",	{20,16,16,22,24,26}, { 125, 300, 600, 1250, 4000, 8000 } }	// Clear
};


EQPRESET CEQSetupDlg::gUserPresets[4] =
{
    { "User1",	{16,16,16,16,16,16}, { 125, 300, 600, 1250, 4000, 8000 } },	// User1
    { "User2",	{16,16,16,16,16,16}, { 125, 300, 600, 1250, 4000, 8000 } },	// User2
    { "User3",	{16,16,16,16,16,16}, { 125, 300, 600, 1250, 4000, 8000 } },	// User3
    { "User4",	{16,16,16,16,16,16}, { 150, 500, 1000, 2500, 5000, 10000 } }	// User4
};


#define ID_EQSLIDER_BASE    41000
#define ID_EQMENU_BASE    	41100


////////////////////////////////////////////////////////////////////////////////
//
// CEQSavePresetDlg
//

//====================================
class CEQSavePresetDlg: public CDialog
//====================================
{
protected:
    PEQPRESET m_pEq;

public:
    CEQSavePresetDlg(PEQPRESET pEq, CWnd *parent=NULL):CDialog(IDD_SAVEPRESET, parent) { m_pEq = pEq; }
    BOOL OnInitDialog();
    VOID OnOK();
};


BOOL CEQSavePresetDlg::OnInitDialog()
//-----------------------------------
{
    CComboBox *pCombo = (CComboBox *)GetDlgItem(IDC_COMBO1);
    if (pCombo)
    {
        int ndx = 0;
        for (UINT i=0; i<4; i++)
        {
            int n = pCombo->AddString(CEQSetupDlg::gUserPresets[i].szName);
            pCombo->SetItemData( n, i);
            if (!lstrcmpi(CEQSetupDlg::gUserPresets[i].szName, m_pEq->szName)) ndx = n;
        }
        pCombo->SetCurSel(ndx);
    }
    SetDlgItemText(IDC_EDIT1, m_pEq->szName);
    return TRUE;
}


VOID CEQSavePresetDlg::OnOK()
//---------------------------
{
    CComboBox *pCombo = (CComboBox *)GetDlgItem(IDC_COMBO1);
    if (pCombo)
    {
        int n = pCombo->GetCurSel();
        if ((n < 0) || (n >= 4)) n = 0;
        GetDlgItemText(IDC_EDIT1, m_pEq->szName, sizeof(m_pEq->szName));
        m_pEq->szName[sizeof(m_pEq->szName)-1] = 0;
        CEQSetupDlg::gUserPresets[n] = *m_pEq;
    }
    CDialog::OnOK();
}


////////////////////////////////////////////////////////////////////////////////
//
// CEQSetupDlg
//

VOID CEQSetupDlg::LoadEQ(HKEY key, LPCSTR pszName, PEQPRESET pEqSettings)
//-----------------------------------------------------------------------
{
    uint32_t dwType = REG_BINARY;
    uint32_t dwSize = sizeof(EQPRESET);
    registry_query_value(key, pszName, NULL, &dwType, (LPBYTE)pEqSettings, &dwSize);
    for (UINT i=0; i<MAX_EQ_BANDS; i++)
    {
        if (pEqSettings->Gains[i] > 32) pEqSettings->Gains[i] = 16;
        if ((pEqSettings->Freqs[i] < 100) || (pEqSettings->Freqs[i] > 10000)) pEqSettings->Freqs[i] = gEQPresets[0].Freqs[i];
    }
    pEqSettings->szName[sizeof(pEqSettings->szName)-1] = 0;
}


VOID CEQSetupDlg::SaveEQ(HKEY key, LPCSTR pszName, PEQPRESET pEqSettings)
//-----------------------------------------------------------------------
{
    RegSetValueEx(key, pszName, NULL, REG_BINARY, (LPBYTE)pEqSettings, sizeof(EQPRESET));
}


VOID CEQSlider::Init(UINT nID, UINT n, CWnd *parent)
//--------------------------------------------------
{
    m_nSliderNo = n;
    m_pParent = parent;
    SubclassDlgItem(nID, parent);
}


BOOL CEQSlider::PreTranslateMessage(MSG *pMsg)
//--------------------------------------------
{
    if ((pMsg) && (pMsg->message == WM_RBUTTONDOWN) && (m_pParent))
    {
        m_x = LOWORD(pMsg->lParam);
        m_y = HIWORD(pMsg->lParam);
        m_pParent->PostMessage(WM_COMMAND, ID_EQSLIDER_BASE+m_nSliderNo, 0);
    }
    return CSliderCtrl::PreTranslateMessage(pMsg);
}


// CEQSetupDlg
BEGIN_MESSAGE_MAP(CEQSetupDlg, CDialog)
    ON_WM_VSCROLL()
    ON_COMMAND(IDC_BUTTON1,	OnEqFlat)
    ON_COMMAND(IDC_BUTTON2,	OnEqJazz)
    ON_COMMAND(IDC_BUTTON5,	OnEqPop)
    ON_COMMAND(IDC_BUTTON6,	OnEqRock)
    ON_COMMAND(IDC_BUTTON7,	OnEqConcert)
    ON_COMMAND(IDC_BUTTON8,	OnEqClear)
    ON_COMMAND(IDC_BUTTON3,	OnEqUser1)
    ON_COMMAND(IDC_BUTTON4,	OnEqUser2)
    ON_COMMAND(IDC_BUTTON9,	OnEqUser3)
    ON_COMMAND(IDC_BUTTON10,OnEqUser4)
    ON_COMMAND(IDC_BUTTON13,OnSavePreset)
    ON_COMMAND_RANGE(ID_EQSLIDER_BASE, ID_EQSLIDER_BASE+MAX_EQ_BANDS, OnSliderMenu)
    ON_COMMAND_RANGE(ID_EQMENU_BASE, ID_EQMENU_BASE+EQ_MAX_FREQS, OnSliderFreq)
END_MESSAGE_MAP()


BOOL CEQSetupDlg::OnInitDialog()
//------------------------------
{
    CDialog::OnInitDialog();
    m_Sliders[0].Init(IDC_SLIDER1, 0, this);
    m_Sliders[1].Init(IDC_SLIDER3, 1, this);
    m_Sliders[2].Init(IDC_SLIDER5, 2, this);
    m_Sliders[3].Init(IDC_SLIDER7, 3, this);
    m_Sliders[4].Init(IDC_SLIDER8, 4, this);
    m_Sliders[5].Init(IDC_SLIDER9, 5, this);
    for (UINT i=0; i<MAX_EQ_BANDS; i++)
    {
        m_Sliders[i].SetRange(0, 32);
        m_Sliders[i].SetTicFreq(4);
    }
    UpdateDialog();
    return TRUE;
}


static void f2s(UINT f, LPSTR s)
//------------------------------
{
    if (f < 1000)
    {
        wsprintf(s, "%dHz", f);
    } else
    {
        UINT fHi = f / 1000;
        UINT fLo = f % 1000;
        if (fLo)
        {
            wsprintf(s, "%d.%dkHz", fHi, fLo/100);
        } else
        {
            wsprintf(s, "%dkHz", fHi);
        }
    }
}


void CEQSetupDlg::UpdateDialog()
//------------------------------
{
    const USHORT uTextIds[MAX_EQ_BANDS] = {IDC_TEXT1, IDC_TEXT2, IDC_TEXT3, IDC_TEXT4, IDC_TEXT5, IDC_TEXT6};
    CHAR s[32];
    for (UINT i=0; i<MAX_EQ_BANDS; i++)
    {
        int n = 32 - m_pEqPreset->Gains[i];
        if (n < 0) n = 0;
        if (n > 32) n = 32;
        if (n != (m_Sliders[i].GetPos() & 0xFFFF)) m_Sliders[i].SetPos(n);
        f2s(m_pEqPreset->Freqs[i], s);
        SetDlgItemText(uTextIds[i], s);
        SetDlgItemText(IDC_BUTTON3,	gUserPresets[0].szName);
        SetDlgItemText(IDC_BUTTON4,	gUserPresets[1].szName);
        SetDlgItemText(IDC_BUTTON9,	gUserPresets[2].szName);
        SetDlgItemText(IDC_BUTTON10,gUserPresets[3].szName);
    }
}


void CEQSetupDlg::UpdateEQ(BOOL bReset)
//-------------------------------------
{
}


void CEQSetupDlg::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
//--------------------------------------------------------------------------
{
    CDialog::OnVScroll(nSBCode, nPos, pScrollBar);
    for (UINT i=0; i<MAX_EQ_BANDS; i++)
    {
        int n = 32 - m_Sliders[i].GetPos();
        if ((n >= 0) && (n <= 32)) m_pEqPreset->Gains[i] = n;
    }
    UpdateEQ(FALSE);
}


void CEQSetupDlg::OnEqFlat()
//--------------------------
{
    *m_pEqPreset = gEQPresets[EQPRESET_FLAT];
    UpdateEQ(TRUE);
    UpdateDialog();
}


void CEQSetupDlg::OnEqJazz()
//--------------------------
{
    *m_pEqPreset = gEQPresets[EQPRESET_JAZZ];
    UpdateEQ(TRUE);
    UpdateDialog();
}


void CEQSetupDlg::OnEqPop()
//-------------------------
{
    *m_pEqPreset = gEQPresets[EQPRESET_POP];
    UpdateEQ(TRUE);
    UpdateDialog();
}


void CEQSetupDlg::OnEqRock()
//--------------------------
{
    *m_pEqPreset = gEQPresets[EQPRESET_ROCK];
    UpdateEQ(TRUE);
    UpdateDialog();
}


void CEQSetupDlg::OnEqConcert()
//-----------------------------
{
    *m_pEqPreset = gEQPresets[EQPRESET_CONCERT];
    UpdateEQ(TRUE);
    UpdateDialog();
}


void CEQSetupDlg::OnEqClear()
//---------------------------
{
    *m_pEqPreset = gEQPresets[EQPRESET_CLEAR];
    UpdateEQ(TRUE);
    UpdateDialog();
}


void CEQSetupDlg::OnEqUser1()
//---------------------------
{
    *m_pEqPreset = gUserPresets[0];
    UpdateEQ(TRUE);
    UpdateDialog();
}


void CEQSetupDlg::OnEqUser2()
//---------------------------
{
    *m_pEqPreset = gUserPresets[1];
    UpdateEQ(TRUE);
    UpdateDialog();
}


void CEQSetupDlg::OnEqUser3()
//---------------------------
{
    *m_pEqPreset = gUserPresets[2];
    UpdateEQ(TRUE);
    UpdateDialog();
}


void CEQSetupDlg::OnEqUser4()
//---------------------------
{
    *m_pEqPreset = gUserPresets[3];
    UpdateEQ(TRUE);
    UpdateDialog();
}


void CEQSetupDlg::OnSavePreset()
//------------------------------
{
    CEQSavePresetDlg dlg(m_pEqPreset, this);
    if (dlg.DoModal() == IDOK)
    {
        UpdateDialog();
    }
}


void CEQSetupDlg::OnSliderMenu(UINT nID)
//--------------------------------------
{
    UINT n = nID - ID_EQSLIDER_BASE;
    if (n < MAX_EQ_BANDS)
    {
        CHAR s[32];
        HMENU hMenu = ::CreatePopupMenu();
        m_nSliderMenu = n;
        if (!hMenu)	return;
        const UINT *pFreqs = &gEqBandFreqs[m_nSliderMenu*EQ_MAX_FREQS];
        for (UINT i=0; i<EQ_MAX_FREQS; i++)
        {
            uint32_t d = MF_STRING;
            if (m_pEqPreset->Freqs[m_nSliderMenu] == pFreqs[i]) d |= MF_CHECKED;
            f2s(pFreqs[i], s);
            ::AppendMenu(hMenu, d, ID_EQMENU_BASE+i, s);
        }
        CPoint pt(m_Sliders[m_nSliderMenu].m_x, m_Sliders[m_nSliderMenu].m_y);
        m_Sliders[m_nSliderMenu].ClientToScreen(&pt);
        ::TrackPopupMenu(hMenu, TPM_LEFTALIGN|TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hWnd, NULL);
        ::DestroyMenu(hMenu);
    }
}


void CEQSetupDlg::OnSliderFreq(UINT nID)
//--------------------------------------
{
    UINT n = nID - ID_EQMENU_BASE;
    if ((m_nSliderMenu < MAX_EQ_BANDS) && (n < EQ_MAX_FREQS))
    {
        UINT f = gEqBandFreqs[m_nSliderMenu*EQ_MAX_FREQS+n];
        if (f != m_pEqPreset->Freqs[m_nSliderMenu])
        {
            m_pEqPreset->Freqs[m_nSliderMenu] = f;
            UpdateEQ(TRUE);
            UpdateDialog();
        }
    }
}


BOOL CEQSetupDlg::OnSetActive()
//-----------------------------
{
    CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_EQ;
    SetDlgItemText(IDC_EQ_WARNING,
        "Note: This EQ, when enabled from Player tab, is applied to "
        "any and all of the modules "
        "that you load in OpenMPT; its settings are stored globally, "
        "rather than in each file. This means that you should avoid "
        "using it as part of your production process, and instead only "
        "use it to correct deficiencies in your audio hardware.");
    return CPropertyPage::OnSetActive();
}


/////////////////////////////////////////////////////////////
// CMidiSetupDlg

extern UINT gnMidiImportSpeed;
extern UINT gnMidiPatternLen;

BEGIN_MESSAGE_MAP(CMidiSetupDlg, CPropertyPage)
    ON_CBN_SELCHANGE(IDC_COMBO1,	OnSettingsChanged)
    ON_COMMAND(IDC_CHECK1,			OnSettingsChanged)
    ON_COMMAND(IDC_CHECK2,			OnSettingsChanged)
    ON_COMMAND(IDC_CHECK3,			OnSettingsChanged)
    ON_COMMAND(IDC_CHECK4,			OnSettingsChanged)
    ON_COMMAND(IDC_MIDI_TO_PLUGIN,	OnSettingsChanged)
    ON_COMMAND(IDC_MIDI_MACRO_CONTROL,	OnSettingsChanged)
    ON_COMMAND(IDC_MIDIVOL_TO_NOTEVOL,	OnSettingsChanged)
    ON_COMMAND(IDC_MIDIPLAYCONTROL,	OnSettingsChanged)
    ON_COMMAND(IDC_MIDIPLAYPATTERNONMIDIIN,	OnSettingsChanged)
END_MESSAGE_MAP()


void CMidiSetupDlg::DoDataExchange(CDataExchange* pDX)
//----------------------------------------------------
{
    CPropertyPage::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(COptionsSoundcard)
    DDX_Control(pDX, IDC_SPIN1,		m_SpinSpd);
    DDX_Control(pDX, IDC_SPIN2,		m_SpinPat);
    //}}AFX_DATA_MAP
}


BOOL CMidiSetupDlg::OnInitDialog()
//--------------------------------
{
    MIDIINCAPS mic;
    CComboBox *combo;

    CPropertyPage::OnInitDialog();
    // Flags
    if (m_dwMidiSetup & MIDISETUP_RECORDVELOCITY) CheckDlgButton(IDC_CHECK1, MF_CHECKED);
    if (m_dwMidiSetup & MIDISETUP_RECORDNOTEOFF) CheckDlgButton(IDC_CHECK2, MF_CHECKED);
    if (m_dwMidiSetup & MIDISETUP_AMPLIFYVELOCITY) CheckDlgButton(IDC_CHECK3, MF_CHECKED);
    if (m_dwMidiSetup & MIDISETUP_TRANSPOSEKEYBOARD) CheckDlgButton(IDC_CHECK4, MF_CHECKED);
    if (m_dwMidiSetup & MIDISETUP_MIDITOPLUG) CheckDlgButton(IDC_MIDI_TO_PLUGIN, MF_CHECKED);
    if (m_dwMidiSetup & MIDISETUP_MIDIMACROCONTROL) CheckDlgButton(IDC_MIDI_MACRO_CONTROL, MF_CHECKED);
    if (m_dwMidiSetup & MIDISETUP_MIDIVOL_TO_NOTEVOL) CheckDlgButton(IDC_MIDIVOL_TO_NOTEVOL, MF_CHECKED);
    if (m_dwMidiSetup & MIDISETUP_RESPONDTOPLAYCONTROLMSGS) CheckDlgButton(IDC_MIDIPLAYCONTROL, MF_CHECKED);
    if (m_dwMidiSetup & MIDISETUP_PLAYPATTERNONMIDIIN) CheckDlgButton(IDC_MIDIPLAYPATTERNONMIDIIN, MF_CHECKED);
    // Midi In Device
    if ((combo = (CComboBox *)GetDlgItem(IDC_COMBO1)) != NULL)
    {
        UINT ndevs = midiInGetNumDevs();
        for (UINT i=0; i<ndevs; i++)
        {
            mic.szPname[0] = 0;
            if (midiInGetDevCaps(i, &mic, sizeof(mic)) == MMSYSERR_NOERROR)
                combo->SetItemData(combo->AddString(mic.szPname), i);
        }
        combo->SetCurSel((m_nMidiDevice == MIDI_MAPPER) ? 0 : m_nMidiDevice);
    }
    // Midi Import settings
    SetDlgItemInt(IDC_EDIT1, gnMidiImportSpeed);
    SetDlgItemInt(IDC_EDIT2, gnMidiPatternLen);
    m_SpinSpd.SetRange(2, 6);
    m_SpinPat.SetRange(64, 256);
    return TRUE;
}


void CMidiSetupDlg::OnOK()
//------------------------
{
    CComboBox *combo;
    CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
    m_dwMidiSetup = 0;
    m_nMidiDevice = MIDI_MAPPER;
    if (IsDlgButtonChecked(IDC_CHECK1)) m_dwMidiSetup |= MIDISETUP_RECORDVELOCITY;
    if (IsDlgButtonChecked(IDC_CHECK2)) m_dwMidiSetup |= MIDISETUP_RECORDNOTEOFF;
    if (IsDlgButtonChecked(IDC_CHECK3)) m_dwMidiSetup |= MIDISETUP_AMPLIFYVELOCITY;
    if (IsDlgButtonChecked(IDC_CHECK4)) m_dwMidiSetup |= MIDISETUP_TRANSPOSEKEYBOARD;
    if (IsDlgButtonChecked(IDC_MIDI_TO_PLUGIN)) m_dwMidiSetup |= MIDISETUP_MIDITOPLUG;
    if (IsDlgButtonChecked(IDC_MIDI_MACRO_CONTROL)) m_dwMidiSetup |= MIDISETUP_MIDIMACROCONTROL;
    if (IsDlgButtonChecked(IDC_MIDIVOL_TO_NOTEVOL)) m_dwMidiSetup |= MIDISETUP_MIDIVOL_TO_NOTEVOL;
    if (IsDlgButtonChecked(IDC_MIDIPLAYCONTROL)) m_dwMidiSetup |= MIDISETUP_RESPONDTOPLAYCONTROLMSGS;
    if (IsDlgButtonChecked(IDC_MIDIPLAYPATTERNONMIDIIN)) m_dwMidiSetup |= MIDISETUP_PLAYPATTERNONMIDIIN;

    if ((combo = (CComboBox *)GetDlgItem(IDC_COMBO1)) != NULL)
    {
        int n = combo->GetCurSel();
        if (n >= 0) m_nMidiDevice = combo->GetItemData(n);
    }
    gnMidiImportSpeed = GetDlgItemInt(IDC_EDIT1);
    gnMidiPatternLen = GetDlgItemInt(IDC_EDIT2);
    if (pMainFrm) pMainFrm->SetupMidi(m_dwMidiSetup, m_nMidiDevice);
    CPropertyPage::OnOK();
}


BOOL CMidiSetupDlg::OnSetActive()
//-------------------------------
{
    CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_MIDI;
    return CPropertyPage::OnSetActive();
}