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



/////////////////////////////////////////////////////////////
// CMidiSetupDlg

extern UINT gnMidiImportSpeed;
extern UINT gnMidiPatternLen;

BEGIN_MESSAGE_MAP(CMidiSetupDlg, CPropertyPage)
    ON_CBN_SELCHANGE(IDC_COMBO1,        OnSettingsChanged)
    ON_COMMAND(IDC_CHECK1,                        OnSettingsChanged)
    ON_COMMAND(IDC_CHECK2,                        OnSettingsChanged)
    ON_COMMAND(IDC_CHECK3,                        OnSettingsChanged)
    ON_COMMAND(IDC_CHECK4,                        OnSettingsChanged)
    ON_COMMAND(IDC_MIDI_TO_PLUGIN,        OnSettingsChanged)
    ON_COMMAND(IDC_MIDI_MACRO_CONTROL,        OnSettingsChanged)
    ON_COMMAND(IDC_MIDIVOL_TO_NOTEVOL,        OnSettingsChanged)
    ON_COMMAND(IDC_MIDIPLAYCONTROL,        OnSettingsChanged)
    ON_COMMAND(IDC_MIDIPLAYPATTERNONMIDIIN,        OnSettingsChanged)
END_MESSAGE_MAP()


void CMidiSetupDlg::DoDataExchange(CDataExchange* pDX)
//----------------------------------------------------
{
    CPropertyPage::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(COptionsSoundcard)
    DDX_Control(pDX, IDC_SPIN1,                m_SpinSpd);
    DDX_Control(pDX, IDC_SPIN2,                m_SpinPat);
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