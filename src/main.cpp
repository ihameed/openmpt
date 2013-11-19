#include "stdafx.h"
#include "MainFrm.h"
#include "ChildFrm.h"
#include "moddoc.h"
#include "globals.h"
#include "vstplug.h"
#include "commctrl.h"
#include "version.h"
#include "test/test.h"
#include <shlwapi.h>
#include "main.h"


#include "qmfcapp.h"

#include "pervasives/pervasives.hpp"
using namespace modplug::pervasives;

CTrackApp theApp;


class CModDocTemplate: public CMultiDocTemplate
{
public:
    CModDocTemplate(UINT nIDResource, CRuntimeClass* pDocClass, CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass):
        CMultiDocTemplate(nIDResource, pDocClass, pFrameClass, pViewClass) {}
    virtual CDocument* OpenDocumentFile(LPCTSTR, BOOL, BOOL);
};

CDocument *CModDocTemplate::OpenDocumentFile(LPCTSTR path, BOOL addToMru, BOOL makeVisible) {
    if (path) {
        TCHAR s[_MAX_EXT];
        _tsplitpath(path, NULL, NULL, NULL, s);
        if (!_tcsicmp(s, _TEXT(".dll"))) {
            CVstPluginManager *pluginManager = theApp.GetPluginManager();
            if (pluginManager) {
                pluginManager->AddPlugin(path);
                return NULL;
            }
        }
    }

    CDocument *doc = CMultiDocTemplate::OpenDocumentFile(path, addToMru, makeVisible);
    if (doc) {
        CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
        if (pMainFrm) pMainFrm->OnDocumentCreated(static_cast<CModDoc *>(doc));
    } else {
        if (path != 0) {
            if(PathFileExists(path) == FALSE) {
                CString str;
                str.Format(GetStrI18N(_TEXT("Unable to open \"%s\": file does not exist.")), path);
                AfxMessageBox(str);
            } else {
                const int nOdc = AfxGetApp()->m_pDocManager->GetOpenDocumentCount();
                CString str;
                str.Format(GetStrI18N(_TEXT("Opening \"%s\" failed. This can happen if "
                    "no more documents can be opened or if the file type was not "
                    "recognised. If the former is true, it's "
                    "recommended to close some documents as otherwise crash is likely"
                    "(currently there %s %d document%s open).")),
                    path, (nOdc == 1) ? "is" : "are", nOdc, (nOdc == 1) ? "" : "s");
                AfxMessageBox(str);
            }
        }
    }
    return doc;
}

#ifdef _DEBUG
#define DDEDEBUG
#endif


//======================================
class CModDocManager: public CDocManager
//======================================
{
public:
    CModDocManager() {}
    virtual BOOL OnDDECommand(LPTSTR lpszCommand);
};


BOOL CModDocManager::OnDDECommand(LPTSTR lpszCommand)
//---------------------------------------------------
{
    BOOL bResult, bActivate;
#ifdef DDEDEBUG
    Log("OnDDECommand: %s\n", lpszCommand);
#endif
    // Handle any DDE commands recognized by your application
    // and return TRUE.  See implementation of CWinApp::OnDDEComand
    // for example of parsing the DDE command string.
    bResult = FALSE;
    bActivate = FALSE;
    if ((lpszCommand) && (*lpszCommand) && (theApp.m_pMainWnd))
    {
        CHAR s[_MAX_PATH], *pszCmd, *pszData;
        int len;

        lstrcpyn(s, lpszCommand, CountOf(s));
        len = strlen(s) - 1;
        while ((len > 0) && (strchr("(){}[]\'\" ", s[len]))) s[len--] = 0;
        pszCmd = s;
        while (pszCmd[0] == '[') pszCmd++;
        pszData = pszCmd;
        while ((pszData[0] != '(') && (pszData[0]))
        {
            if (((uint8_t)pszData[0]) <= (uint8_t)0x20) *pszData = 0;
            pszData++;
        }
        while ((*pszData) && (strchr("(){}[]\'\" ", *pszData)))
        {
            *pszData = 0;
            pszData++;
        }
        // Edit/Open
        if ((!lstrcmpi(pszCmd, "Edit"))
         || (!lstrcmpi(pszCmd, "Open")))
        {
            if (pszData[0])
            {
                bResult = TRUE;
                bActivate = TRUE;
                OpenDocumentFile(pszData);
            }
        } else
        // New
        if (!lstrcmpi(pszCmd, "New"))
        {
            OpenDocumentFile(NULL);
            bResult = TRUE;
            bActivate = TRUE;
        }
    #ifdef DDEDEBUG
        Log("%s(%s)\n", pszCmd, pszData);
    #endif
        if ((bActivate) && (theApp.m_pMainWnd->m_hWnd))
        {
            if (theApp.m_pMainWnd->IsIconic()) theApp.m_pMainWnd->ShowWindow(SW_RESTORE);
            theApp.m_pMainWnd->SetActiveWindow();
        }
    }
    // Return FALSE for any DDE commands you do not handle.
#ifdef DDEDEBUG
    if (!bResult)
    {
        Log("WARNING: failure in CModDocManager::OnDDECommand()\n");
    }
#endif
    return bResult;
}


/////////////////////////////////////////////////////////////////////////////
// Common Tables

const LPCSTR szNoteNames[12] =
{
    "C-", "C#", "D-", "D#", "E-", "F-",
    "F#", "G-", "G#", "A-", "A#", "B-"
};

const LPCTSTR szDefaultNoteNames[modplug::tracker::NoteMax] = {
    TEXT("C-0"), TEXT("C#0"), TEXT("D-0"), TEXT("D#0"), TEXT("E-0"), TEXT("F-0"), TEXT("F#0"), TEXT("G-0"), TEXT("G#0"), TEXT("A-0"), TEXT("A#0"), TEXT("B-0"),
    TEXT("C-1"), TEXT("C#1"), TEXT("D-1"), TEXT("D#1"), TEXT("E-1"), TEXT("F-1"), TEXT("F#1"), TEXT("G-1"), TEXT("G#1"), TEXT("A-1"), TEXT("A#1"), TEXT("B-1"),
    TEXT("C-2"), TEXT("C#2"), TEXT("D-2"), TEXT("D#2"), TEXT("E-2"), TEXT("F-2"), TEXT("F#2"), TEXT("G-2"), TEXT("G#2"), TEXT("A-2"), TEXT("A#2"), TEXT("B-2"),
    TEXT("C-3"), TEXT("C#3"), TEXT("D-3"), TEXT("D#3"), TEXT("E-3"), TEXT("F-3"), TEXT("F#3"), TEXT("G-3"), TEXT("G#3"), TEXT("A-3"), TEXT("A#3"), TEXT("B-3"),
    TEXT("C-4"), TEXT("C#4"), TEXT("D-4"), TEXT("D#4"), TEXT("E-4"), TEXT("F-4"), TEXT("F#4"), TEXT("G-4"), TEXT("G#4"), TEXT("A-4"), TEXT("A#4"), TEXT("B-4"),
    TEXT("C-5"), TEXT("C#5"), TEXT("D-5"), TEXT("D#5"), TEXT("E-5"), TEXT("F-5"), TEXT("F#5"), TEXT("G-5"), TEXT("G#5"), TEXT("A-5"), TEXT("A#5"), TEXT("B-5"),
    TEXT("C-6"), TEXT("C#6"), TEXT("D-6"), TEXT("D#6"), TEXT("E-6"), TEXT("F-6"), TEXT("F#6"), TEXT("G-6"), TEXT("G#6"), TEXT("A-6"), TEXT("A#6"), TEXT("B-6"),
    TEXT("C-7"), TEXT("C#7"), TEXT("D-7"), TEXT("D#7"), TEXT("E-7"), TEXT("F-7"), TEXT("F#7"), TEXT("G-7"), TEXT("G#7"), TEXT("A-7"), TEXT("A#7"), TEXT("B-7"),
    TEXT("C-8"), TEXT("C#8"), TEXT("D-8"), TEXT("D#8"), TEXT("E-8"), TEXT("F-8"), TEXT("F#8"), TEXT("G-8"), TEXT("G#8"), TEXT("A-8"), TEXT("A#8"), TEXT("B-8"),
    TEXT("C-9"), TEXT("C#9"), TEXT("D-9"), TEXT("D#9"), TEXT("E-9"), TEXT("F-9"), TEXT("F#9"), TEXT("G-9"), TEXT("G#9"), TEXT("A-9"), TEXT("A#9"), TEXT("B-9"),
};

const uint8_t gEffectColors[modplug::tracker::CmdMax] =
{
    0,                                    0,                                        MODCOLOR_PITCH,                MODCOLOR_PITCH,
    MODCOLOR_PITCH,            MODCOLOR_PITCH,                MODCOLOR_VOLUME,        MODCOLOR_VOLUME,
    MODCOLOR_VOLUME,    MODCOLOR_PANNING,        0,                                        MODCOLOR_VOLUME,
    MODCOLOR_GLOBALS,    MODCOLOR_VOLUME,        MODCOLOR_GLOBALS,        0,
    MODCOLOR_GLOBALS,    MODCOLOR_GLOBALS,        0,                                        0,
    0,                                    MODCOLOR_VOLUME,        MODCOLOR_VOLUME,        MODCOLOR_GLOBALS,
    MODCOLOR_GLOBALS,    0,                                        MODCOLOR_PITCH,                MODCOLOR_PANNING,
    MODCOLOR_PITCH,            MODCOLOR_PANNING,        0,                                        0,
    0,                                    0,                                        0,                                        MODCOLOR_PITCH,
    MODCOLOR_PITCH,
};

const uint8_t gVolEffectColors[modplug::tracker::VolCmdMax] =
{
    0,                                    MODCOLOR_VOLUME,        MODCOLOR_PANNING,        MODCOLOR_VOLUME,
    MODCOLOR_VOLUME,    MODCOLOR_VOLUME,        MODCOLOR_VOLUME,        MODCOLOR_PITCH,
    MODCOLOR_PITCH,            MODCOLOR_PANNING,        MODCOLOR_PANNING,        MODCOLOR_PITCH,
    MODCOLOR_PITCH,            MODCOLOR_PITCH,                0,                                        0,
};

static void ShowChangesDialog()
//-----------------------------
{
}


TCHAR CTrackApp::m_szExePath[_MAX_PATH] = TEXT("");
bool CTrackApp::m_bPortableMode = false;


/////////////////////////////////////////////////////////////////////////////
// Midi Library

LPMIDILIBSTRUCT CTrackApp::glpMidiLibrary = NULL;

/////////////////////////////////////////////////////////////////////////////
// CTrackApp

UINT CTrackApp::m_nDefaultDocType = MOD_TYPE_IT;
MEMORYSTATUS CTrackApp::gMemStatus;

// -> CODE#0023
// -> DESC="IT project files (.itp)"
BOOL CTrackApp::m_nProject = FALSE;
// -! NEW_FEATURE#0023

BEGIN_MESSAGE_MAP(CTrackApp, CWinApp)
    //{{AFX_MSG_MAP(CTrackApp)
    ON_COMMAND(ID_FILE_NEW,            OnFileNew)
    ON_COMMAND(ID_FILE_NEWMOD,    OnFileNewMOD)
    ON_COMMAND(ID_FILE_NEWS3M,    OnFileNewS3M)
    ON_COMMAND(ID_FILE_NEWXM,    OnFileNewXM)
    ON_COMMAND(ID_FILE_NEWIT,    OnFileNewIT)
// -> CODE#0023
// -> DESC="IT project files (.itp)"
    ON_COMMAND(ID_NEW_ITPROJECT,OnFileNewITProject)
// -! NEW_FEATURE#0023
    ON_COMMAND(ID_NEW_MPT,            OnFileNewMPT)
    ON_COMMAND(ID_FILE_OPEN,    OnFileOpen)
    ON_COMMAND(ID_APP_ABOUT,    OnAppAbout)
    ON_COMMAND(ID_HELP_INDEX,    CWinApp::OnHelpIndex)
    ON_COMMAND(ID_HELP_FINDER,    CWinApp::OnHelpFinder)
    ON_COMMAND(ID_HELP_USING,    CWinApp::OnHelpUsing)
    ON_COMMAND(ID_HELP_SEARCH,    OnHelpSearch)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTrackApp construction

CTrackApp::CTrackApp()
//--------------------
{
    #if (_MSC_VER >= 1400)
        _CrtSetDebugFillThreshold(0); // Disable buffer filling in secure enhanced CRT functions.
    #endif
    m_pModTemplate = NULL;
    m_pPluginManager = NULL;
    m_bInitialized = FALSE;
    m_bLayer3Present = FALSE;
    m_bDebugMode = FALSE;
    m_hAlternateResourceHandle = NULL;
    m_szConfigFileName[0] = 0;
    // Default macro config
    MemsetZero(m_MidiCfg);
    strcpy(m_MidiCfg.szMidiGlb[MIDIOUT_START], "FF");
    strcpy(m_MidiCfg.szMidiGlb[MIDIOUT_STOP], "FC");
    strcpy(m_MidiCfg.szMidiGlb[MIDIOUT_NOTEON], "9c n v");
    strcpy(m_MidiCfg.szMidiGlb[MIDIOUT_NOTEOFF], "9c n 0");
    strcpy(m_MidiCfg.szMidiGlb[MIDIOUT_PROGRAM], "Cc p");
    strcpy(m_MidiCfg.szMidiSFXExt[0], "F0F000z");
    CModDoc::CreateZxxFromType(m_MidiCfg.szMidiZXXExt, zxx_reso4Bit);
}

/////////////////////////////////////////////////////////////////////////////
// CTrackApp initialization

void Terminate_AppThread()
//----------------------------------------------
{
    //TODO: Why does this not get called.
    AfxMessageBox("HOLY BALLS");
    exit(-1);
}

#ifdef WIN32    // Legacy stuff
// Move a config file called sFileName from the App's directory (or one of its sub directories specified by sSubDir) to
// %APPDATA%. If specified, it will be renamed to sNewFileName. Existing files are never overwritten.
// Returns true on success.
bool CTrackApp::MoveConfigFile(TCHAR sFileName[_MAX_PATH], TCHAR sSubDir[_MAX_PATH], TCHAR sNewFileName[_MAX_PATH])
//-----------------------------------------------------------------------------------------------------------------
{
    // copy a config file from the exe directory to the new config dirs
    TCHAR sOldPath[_MAX_PATH], sNewPath[_MAX_PATH];
    strcpy(sOldPath, m_szExePath);
    if(sSubDir[0])
        strcat(sOldPath, sSubDir);
    strcat(sOldPath, sFileName);

    strcpy(sNewPath, m_szConfigDirectory);
    if(sSubDir[0])
        strcat(sNewPath, sSubDir);
    if(sNewFileName[0])
        strcat(sNewPath, sNewFileName);
    else
        strcat(sNewPath, sFileName);

    if(PathFileExists(sNewPath) == 0 && PathFileExists(sOldPath) != 0)
    {
        return (MoveFile(sOldPath, sNewPath) != 0);
    }
    return false;
}
#endif    // WIN32 Legacy Stuff


void CTrackApp::SetupPaths()
//--------------------------
{
    if(GetModuleFileName(NULL, m_szExePath, _MAX_PATH))
    {
        TCHAR szDrive[_MAX_DRIVE] = "", szDir[_MAX_PATH] = "";
        _splitpath(m_szExePath, szDrive, szDir, NULL, NULL);
        strcpy(m_szExePath, szDrive);
        strcat(m_szExePath, szDir);
    }

    m_szConfigDirectory[0] = 0;
    // Try to find a nice directory where we should store our settings (default: %APPDATA%)
    bool bIsAppDir = false;
    if(!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, m_szConfigDirectory)))
    {
        if(!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, m_szConfigDirectory)))
        {
            strcpy(m_szConfigDirectory, m_szExePath);
            bIsAppDir = true;
        }
    }

    // Check if the user prefers to use the app's directory
    strcpy(m_szConfigFileName, m_szExePath); // config file
    strcat(m_szConfigFileName, "mptrack.ini");
    if(GetPrivateProfileInt("Paths", "UseAppDataDirectory", 1, m_szConfigFileName) == 0)
    {
        strcpy(m_szConfigDirectory, m_szExePath);
        bIsAppDir = true;
    }

    if(!bIsAppDir)
    {
        // Store our app settings in %APPDATA% or "My Files"
        strcat(m_szConfigDirectory, "\\OpenMPT\\");

        // Path doesn't exist yet, so it has to be created
        if(PathIsDirectory(m_szConfigDirectory) == 0)
        {
            CreateDirectory(m_szConfigDirectory, 0);
        }

        #ifdef WIN32    // Legacy stuff
        // Move the config files if they're still in the old place.
        MoveConfigFile("mptrack.ini");
        MoveConfigFile("plugin.cache");
        MoveConfigFile("mpt_intl.ini");
        #endif    // WIN32 Legacy Stuff
    }

    // Create tunings dir
    CString sTuningPath;
    sTuningPath.Format(TEXT("%stunings\\"), m_szConfigDirectory);
    CMainFrame::SetDefaultDirectory(sTuningPath, DIR_TUNING);

    if(PathIsDirectory(CMainFrame::GetDefaultDirectory(DIR_TUNING)) == 0)
    {
        CreateDirectory(CMainFrame::GetDefaultDirectory(DIR_TUNING), 0);
    }

    if(!bIsAppDir)
    {
        // Import old tunings
        TCHAR sOldTunings[_MAX_PATH];
        strcpy(sOldTunings, m_szExePath);
        strcat(sOldTunings, "tunings\\");

        if(PathIsDirectory(sOldTunings) != 0)
        {
            TCHAR sSearchPattern[_MAX_PATH];
            strcpy(sSearchPattern, sOldTunings);
            strcat(sSearchPattern, "*.*");
            WIN32_FIND_DATA FindFileData;
            HANDLE hFind;
            hFind = FindFirstFile(sSearchPattern, &FindFileData);
            if(hFind != INVALID_HANDLE_VALUE)
            {
                do
                {
                    MoveConfigFile(FindFileData.cFileName, "tunings\\");
                } while(FindNextFile(hFind, &FindFileData) != 0);
            }
            FindClose(hFind);
            RemoveDirectory(sOldTunings);
        }
    }

    // Set up default file locations
    strcpy(m_szConfigFileName, m_szConfigDirectory); // config file
    strcat(m_szConfigFileName, "mptrack.ini");

    strcpy(m_szStringsFileName, m_szConfigDirectory); // I18N file
    strcat(m_szStringsFileName, "mpt_intl.ini");

    strcpy(m_szPluginCacheFileName, m_szConfigDirectory); // plugin cache
    strcat(m_szPluginCacheFileName, "plugin.cache");

    m_bPortableMode = bIsAppDir;
}

BOOL CTrackApp::Run() {
    auto result = QMfcApp::run(this);
    delete qApp;
    return result;
}

BOOL CTrackApp::InitInstance()
//----------------------------
{
    QMfcApp::instance(this);

    set_terminate(Terminate_AppThread);

    // Initialize OLE MFC support
    AfxOleInit();
    // Standard initialization

    // Change the registry key under which our settings are stored.
    //SetRegistryKey(_T("Olivier Lapicque"));
    // Start loading
    BeginWaitCursor();

    //m_hAlternateResourceHandle = LoadLibrary("mpt_intl.dll");

    MemsetZero(gMemStatus);
    GlobalMemoryStatus(&gMemStatus);
#if 0
    Log("Physical: %lu\n", gMemStatus.dwTotalPhys);
    Log("Page File: %lu\n", gMemStatus.dwTotalPageFile);
    Log("Virtual: %lu\n", gMemStatus.dwTotalVirtual);
#endif
    // Allow allocations of at least 16MB
    if (gMemStatus.dwTotalPhys < 16*1024*1024) gMemStatus.dwTotalPhys = 16*1024*1024;

    CMainFrame::m_nSampleUndoMaxBuffer = gMemStatus.dwTotalPhys / 10; // set sample undo buffer size
    if(CMainFrame::m_nSampleUndoMaxBuffer < (1 << 20)) CMainFrame::m_nSampleUndoMaxBuffer = (1 << 20);

    ASSERT(NULL == m_pDocManager);
    m_pDocManager = new CModDocManager();

#ifdef _DEBUG
        ASSERT((sizeof(modplug::tracker::modchannel_t)&7) == 0);
    // Disabled by rewbs for smoothVST. Might cause minor perf issues due to increased cache misses?
#endif

    // Set up paths to store configuration in
    SetupPaths();

    //Force use of custom ini file rather than windowsDir\executableName.ini
    if (m_pszProfileName) {
        free((void *)m_pszProfileName);
    }
    m_pszProfileName = _tcsdup(m_szConfigFileName);


    LoadStdProfileSettings(10);  // Load standard INI file options (including MRU)

    // Register document templates
    m_pModTemplate = new CModDocTemplate(
        IDR_MODULETYPE,
        RUNTIME_CLASS(CModDoc),
        RUNTIME_CLASS(CChildFrame), // custom MDI child frame
        RUNTIME_CLASS(CModControlView));
    AddDocTemplate(m_pModTemplate);

    // Initialize Audio
    module_renderer::InitSysInfo();
    if (module_renderer::deprecated_global_system_info & SYSMIX_ENABLEMMX)
    {
        CMainFrame::m_nSrcMode = SRCMODE_SPLINE;
    }
    if (module_renderer::deprecated_global_system_info & SYSMIX_MMXEX)
    {
        CMainFrame::m_nSrcMode = SRCMODE_POLYPHASE;
    }

    // Load default macro configuration
    for (UINT isfx=0; isfx<16; isfx++)
    {
        CHAR s[64], snam[32];
        wsprintf(snam, "SF%X", isfx);
        GetPrivateProfileString("Zxx Macros", snam, m_MidiCfg.szMidiSFXExt[isfx], s, CountOf(s), m_szConfigFileName);
        s[MACRO_LENGTH - 1] = 0;
        memcpy(m_MidiCfg.szMidiSFXExt[isfx], s, MACRO_LENGTH);
    }
    for (UINT izxx=0; izxx<128; izxx++)
    {
        CHAR s[64], snam[32];
        wsprintf(snam, "Z%02X", izxx|0x80);
        GetPrivateProfileString("Zxx Macros", snam, m_MidiCfg.szMidiZXXExt[izxx], s, CountOf(s), m_szConfigFileName);
        s[MACRO_LENGTH - 1] = 0;
        memcpy(m_MidiCfg.szMidiZXXExt[izxx], s, MACRO_LENGTH);
    }

    int argc = __argc;
    char **argv = __argv; //__targv
    // Parse command line for standard shell commands, DDE, file open


    bool opt_debug_mode = false;
    bool opt_disable_plugins = false;
    bool opt_suppress_yet_another_helpful_modal_dialog = false;

    // create main MDI Frame window
    CMainFrame* pMainFrame = new CMainFrame();
    if (!pMainFrame->LoadFrame(IDR_MAINFRAME)) return FALSE;
    m_pMainWnd = pMainFrame;

    m_bDebugMode = opt_debug_mode;

    // Enable drag/drop open
    m_pMainWnd->DragAcceptFiles();

    // Enable DDE Execute open
    EnableShellOpen();

    // Register MOD extensions
    //RegisterExtensions();

    // Initialize DXPlugins
    if (!opt_disable_plugins) deprecated_InitializeDXPlugins();

    // Initialize localized strings
    ImportLocalizedStrings();

    // Initialize CMainFrame
    pMainFrame->Initialize();
    InitCommonControls();
    m_dwLastPluginIdleCall=0;    //rewbs.VSTCompliance

    pMainFrame->ShowWindow(m_nCmdShow);
    pMainFrame->UpdateWindow();

    m_dwTimeStarted = timeGetTime();
    m_bInitialized = TRUE;

    // Open settings if the previous execution was with an earlier version.
    if (opt_suppress_yet_another_helpful_modal_dialog && MptVersion::ToNum(CMainFrame::gcsPreviousVersion) < MptVersion::num) {
        ShowChangesDialog();
        m_pMainWnd->PostMessage(WM_COMMAND, ID_VIEW_OPTIONS);
    }

    EndWaitCursor();

    #ifdef ENABLE_TESTS
        MptTest::DoTests();
    #endif

    return TRUE;
}


int CTrackApp::ExitInstance()
//---------------------------
{
    //::MessageBox("Exiting/Crashing");
    // Save default macro configuration
    if (m_szConfigFileName[0])
    {
        for (UINT isfx=0; isfx<16; isfx++)
        {
            CHAR s[64], snam[32];
            wsprintf(snam, "SF%X", isfx);
            memcpy(s, m_MidiCfg.szMidiSFXExt[isfx], MACRO_LENGTH);
            s[31] = 0;
            if (!WritePrivateProfileString("Zxx Macros", snam, s, m_szConfigFileName)) break;
        }
        for (UINT izxx=0; izxx<128; izxx++)
        {
            CHAR s[64], snam[32];
            wsprintf(snam, "Z%02X", izxx|0x80);
            memcpy(s, m_MidiCfg.szMidiZXXExt[izxx], MACRO_LENGTH);
            s[MACRO_LENGTH - 1] = 0;
            if (!WritePrivateProfileString("Zxx Macros", snam, s, m_szConfigFileName)) break;
        }
    }
    // Uninitialize DX-Plugins
    deprecated_UninitializeDXPlugins();

    return CWinApp::ExitInstance();
}


////////////////////////////////////////////////////////////////////////////////
// Chords

void CTrackApp::LoadChords(PMPTCHORD pChords)
//-------------------------------------------
{
    if (!m_szConfigFileName[0]) return;
    for (UINT i=0; i<3*12; i++)
    {
        LONG chord;
        if ((chord = GetPrivateProfileInt("Chords", szDefaultNoteNames[i], -1, m_szConfigFileName)) >= 0)
        {
            if ((chord & 0xFFFFFFC0) || (!pChords[i].notes[0]))
            {
                pChords[i].key = (uint8_t)(chord & 0x3F);
                pChords[i].notes[0] = (uint8_t)((chord >> 6) & 0x3F);
                pChords[i].notes[1] = (uint8_t)((chord >> 12) & 0x3F);
                pChords[i].notes[2] = (uint8_t)((chord >> 18) & 0x3F);
            }
        }
    }
}


void CTrackApp::SaveChords(PMPTCHORD pChords)
//-------------------------------------------
{
    CHAR s[64];

    if (!m_szConfigFileName[0]) return;
    for (UINT i=0; i<3*12; i++)
    {
        wsprintf(s, "%d", (pChords[i].key) | (pChords[i].notes[0] << 6) | (pChords[i].notes[1] << 12) | (pChords[i].notes[2] << 18));
        if (!WritePrivateProfileString("Chords", szDefaultNoteNames[i], s, m_szConfigFileName)) break;
    }
}


////////////////////////////////////////////////////////////////////////////////
// App Messages


void CTrackApp::OnFileNew()
//-------------------------
{
    if (!m_bInitialized) return;

    // Default module type
    MODTYPE nNewType = MOD_TYPE_IT;
    bool bIsProject = false;

    // Get active document to make the new module of the same type
    CModDoc *pModDoc = CMainFrame::GetMainFrame()->GetActiveDoc();
    if(pModDoc != nullptr)
    {
        module_renderer *pSndFile = pModDoc->GetSoundFile();
        if(pSndFile != nullptr)
        {
            nNewType = pSndFile->GetBestSaveFormat();
            bIsProject = ((pSndFile->m_dwSongFlags & SONG_ITPROJECT) != 0) ? true: false;
        }
    }

    switch(nNewType)
    {
    case MOD_TYPE_MOD:
        OnFileNewMOD();
        break;
    case MOD_TYPE_S3M:
        OnFileNewS3M();
        break;
    case MOD_TYPE_XM:
        OnFileNewXM();
        break;
    case MOD_TYPE_IT:
        if(bIsProject)
            OnFileNewITProject();
        else
            OnFileNewIT();
        break;
    case MOD_TYPE_MPT:
    default:
        OnFileNewMPT();
        break;
    }
}


void CTrackApp::OnFileNewMOD()
//----------------------------
{
// -> CODE#0023
// -> DESC="IT project files (.itp)"
    SetAsProject(FALSE);
// -! NEW_FEATURE#0023

    SetDefaultDocType(MOD_TYPE_MOD);
    if (m_pModTemplate) m_pModTemplate->OpenDocumentFile(NULL);
}


void CTrackApp::OnFileNewS3M()
//----------------------------
{
// -> CODE#0023
// -> DESC="IT project files (.itp)"
    SetAsProject(FALSE);
// -! NEW_FEATURE#0023

    SetDefaultDocType(MOD_TYPE_S3M);
    if (m_pModTemplate) m_pModTemplate->OpenDocumentFile(NULL);
}


void CTrackApp::OnFileNewXM()
//---------------------------
{
// -> CODE#0023
// -> DESC="IT project files (.itp)"
    SetAsProject(FALSE);
// -! NEW_FEATURE#0023

    SetDefaultDocType(MOD_TYPE_XM);
    if (m_pModTemplate) m_pModTemplate->OpenDocumentFile(NULL);
}


void CTrackApp::OnFileNewIT()
//---------------------------
{
// -> CODE#0023
// -> DESC="IT project files (.itp)"
    SetAsProject(FALSE);
// -! NEW_FEATURE#0023

    SetDefaultDocType(MOD_TYPE_IT);
    if (m_pModTemplate) m_pModTemplate->OpenDocumentFile(NULL);
}

void CTrackApp::OnFileNewMPT()
//---------------------------
{
    SetAsProject(FALSE);
    SetDefaultDocType(MOD_TYPE_MPT);
    if (m_pModTemplate) m_pModTemplate->OpenDocumentFile(NULL);
}



// -> CODE#0023
// -> DESC="IT project files (.itp)"
void CTrackApp::OnFileNewITProject()
//----------------------------------
{
    SetAsProject(TRUE);
    SetDefaultDocType(MOD_TYPE_IT);
    if (m_pModTemplate) m_pModTemplate->OpenDocumentFile(NULL);
}
// -! NEW_FEATURE#0023


void CTrackApp::OnFileOpen()
//--------------------------
{
    static int nFilterIndex = 0;
    FileDlgResult files = ShowOpenSaveFileDialog(true, "", "",
        "All Modules|*.mod;*.nst;*.wow;*.s3m;*.stm;*.669;*.mtm;*.xm;*.it;*.itp;*.mptm;*.ult;*.mdz;*.s3z;*.xmz;*.itz;mod.*;*.far;*.mdl;*.okt;*.dmf;*.ptm;*.mdr;*.med;*.ams;*.dbm;*.dsm;*.mid;*.rmi;*.smf;*.umx;*.amf;*.psm;*.mt2;*.gdm;*.imf;*.j2b"
#ifndef NO_MO3_SUPPORT
        ";*.mo3"
#endif
        "|"
        "Compressed Modules (*.mdz;*.s3z;*.xmz;*.itz"
#ifndef NO_MO3_SUPPORT
        ";*.mo3"
#endif
        ")|*.mdz;*.s3z;*.xmz;*.itz;*.mdr;*.zip;*.rar;*.lha;*.gz"
#ifndef NO_MO3_SUPPORT
        ";*.mo3"
#endif
        "|"
        "ProTracker Modules (*.mod,*.nst)|*.mod;mod.*;*.mdz;*.nst;*.m15|"
        "ScreamTracker Modules (*.s3m,*.stm)|*.s3m;*.stm;*.s3z|"
        "FastTracker Modules (*.xm)|*.xm;*.xmz|"
        "Impulse Tracker Modules (*.it)|*.it;*.itz|"
        // -> CODE#0023
        // -> DESC="IT project files (.itp)"
        "Impulse Tracker Projects (*.itp)|*.itp;*.itpz|"
        // -! NEW_FEATURE#0023
        "OpenMPT Modules (*.mptm)|*.mptm;*.mptmz|"
        "Other Modules (mtm,okt,mdl,669,far,...)|*.mtm;*.669;*.ult;*.wow;*.far;*.mdl;*.okt;*.dmf;*.ptm;*.med;*.ams;*.dbm;*.dsm;*.umx;*.amf;*.psm;*.mt2;*.gdm;*.imf;*.j2b|"
        "Wave Files (*.wav)|*.wav|"
        "Midi Files (*.mid,*.rmi)|*.mid;*.rmi;*.smf|"
        "All Files (*.*)|*.*||",
        CMainFrame::GetWorkingDirectory(DIR_MODS),
        true,
        &nFilterIndex);
    if (files.abort) {
        return;
    }

    CMainFrame::SetWorkingDirectory(files.workingDirectory.c_str(), DIR_MODS, true);

    for (size_t counter = 0; counter < files.filenames.size(); counter++) {
        //XXXih: what is this broken shit
        OpenDocumentFile(files.filenames[counter].c_str());
    }
}


void CTrackApp::RegisterExtensions()
//----------------------------------
{
    HKEY key;
    CHAR s[512] = "";
    CHAR exename[512] = "";

    GetModuleFileName(AfxGetInstanceHandle(), s, sizeof(s));
    GetShortPathName(s, exename, sizeof(exename));
    if (RegCreateKey(HKEY_CLASSES_ROOT,
                    "OpenMPTFile\\shell\\Edit\\command",
                    &key) == ERROR_SUCCESS)
    {
        strcpy(s, exename);
        strcat(s, " \"%1\"");
        RegSetValueEx(key, NULL, NULL, REG_SZ, (LPBYTE)s, strlen(s)+1);
        RegCloseKey(key);
    }
    if (RegCreateKey(HKEY_CLASSES_ROOT,
                    "OpenMPTFile\\shell\\Edit\\ddeexec",
                    &key) == ERROR_SUCCESS)
    {
        strcpy(s, "[Edit(\"%1\")]");
        RegSetValueEx(key, NULL, NULL, REG_SZ, (LPBYTE)s, strlen(s)+1);
        RegCloseKey(key);
    }
}


void CTrackApp::OnHelpSearch()
//----------------------------
{
    CHAR s[80] = "";
    WinHelp((uint32_t)&s, HELP_KEY);
}

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About


//=============================
class CAboutDlg: public CDialog
//=============================
{
public:
    CAboutDlg() {}
    ~CAboutDlg();

// Implementation
protected:
    virtual BOOL OnInitDialog();
    virtual void OnOK();
    virtual void OnCancel();
};

static CAboutDlg *gpAboutDlg = NULL;


CAboutDlg::~CAboutDlg()
//---------------------
{
    gpAboutDlg = NULL;
}

void CAboutDlg::OnOK()
//--------------------
{
    gpAboutDlg = NULL;
    DestroyWindow();
    delete this;
}


void CAboutDlg::OnCancel()
//------------------------
{
    OnOK();
}


BOOL CAboutDlg::OnInitDialog()
//----------------------------
{
    CHAR s[256];
    CDialog::OnInitDialog();
    wsprintf(s, "Build Date: %s", gszBuildDate);
    SetDlgItemText(IDC_EDIT2, s);
    SetDlgItemText(IDC_EDIT3, CString("OpenMPT ") + MptVersion::str + " (development build)");

    const char* const credits_and_authors = {
        "Find the source at: http://github.com/xaimus/openmpt/\r\n"
        "OpenMPT / ModPlug Tracker\r\n"
        "Copyright (c) 2004-2011 Contributors\r\n"
        "Copyright (c) 1997-2003 Olivier Lapicque (olivier@modplug.com)\r\n"
        "\r\n"
        "Contributors:\r\n"
        "Imran Hameed (2012)\r\n"
        "Ahti Leppanen (2005-2011)\r\n"
        "Johannes Schultz (2008-2011)\r\n"
        "Robin Fernandes (2004-2007)\r\n"
        "Sergiy Pylypenko (2007)\r\n"
        "Eric Chavanon (2004-2005)\r\n"
        "Trevor Nunes (2004)\r\n"
        "Olivier Lapicque (1997-2003)\r\n"
        "\r\n"
        "Thanks to:\r\n\r\n"
        "Konstanty for the XMMS-ModPlug resampling implementation\r\n"
        "http://modplug-xmms.sourceforge.net/\r\n"
        "Stephan M. Bernsee for pitch shifting source code\r\n"
        "http://www.dspdimension.com\r\n"
        "Olli Parviainen for SoundTouch Library (time stretching)\r\n"
        "http://www.surina.net/soundtouch/\r\n"
        "Hermann Seib for his example VST Host implementation\r\n"
        "http://www.hermannseib.com/english/vsthost.htm\r\n"
        "Ian Luck for UNMO3\r\n"
        "http://www.un4seen.com/mo3.html\r\n"
        "Jean-loup Gailly and Mark Adler for zlib\r\n"
        "http://zlib.net/\r\n"
        "coda for sample drawing code\r\n"
        "http://coda.s3m.us/\r\n"
        "Storlek for all the IT compatibility hints and testcases\r\n"
        "as well as the IMF, OKT and ULT loaders\r\n"
        "http://schismtracker.org/\r\n"
        "kode54 for the PSM and J2B loaders\r\n"
        "http://kode54.foobar2000.org/\r\n"
        "Pel K. Txnder for the scrolling credits control :)\r\n"
        "http://tinyurl.com/4yze8\r\n"
        "\r\nThe people at ModPlug forums for crucial contribution\r\n"
        "in the form of ideas, testing and support; thanks\r\n"
        "particularly to:\r\n"
        "33, Anboi, BooT-SectoR-ViruZ, Bvanoudtshoorn\r\n"
        "christofori, Diamond, Ganja, Georg, Goor00\r\n"
        "KrazyKatz, LPChip, Nofold, Rakib, Sam Zen\r\n"
        "Skaven, Skilletaudio, Snu, Squirrel Havoc, Waxhead\r\n"
        "\r\n\r\n\r\n\r\n\r\n\r\n\r\n"
        "VST PlugIn Technology by Steinberg Media Technologies GmbH\r\n"
        "ASIO Technology by Steinberg Media Technologies GmbH\r\n"
        "\r\n\r\n\r\n\r\n\r\n\r\n"
    };
    SetDlgItemText(IDC_HERPDERP, credits_and_authors);
    return TRUE;
}


// App command to run the dialog
void CTrackApp::OnAppAbout()
//--------------------------
{
    if (gpAboutDlg) return;
    ShowChangesDialog();
    gpAboutDlg = new CAboutDlg();
    gpAboutDlg->Create(IDD_ABOUTBOX, m_pMainWnd);
}




/////////////////////////////////////////////////////////////////////////////
// Idle-time processing

BOOL CTrackApp::OnIdle(LONG lCount)
//---------------------------------
{
    BOOL b = CWinApp::OnIdle(lCount);

    // Call plugins idle routine for open editor
    uint32_t curTime = timeGetTime();
    // TODO: is it worth the overhead of checking that 10ms have passed,
    //       or should we just do it on every idle message?
    if (m_pPluginManager)
    {
        //rewbs.vstCompliance: call @ 50Hz
        if (curTime - m_dwLastPluginIdleCall > 20) //20ms since last call?
        {
            m_pPluginManager->OnIdle();
            m_dwLastPluginIdleCall = curTime;
        }
    }

    return b;
}


/////////////////////////////////////////////////////////////////////////////
// DIB


RGBQUAD rgb2quad(COLORREF c)
//--------------------------
{
    RGBQUAD r;
    r.rgbBlue = GetBValue(c);
    r.rgbGreen = GetGValue(c);
    r.rgbRed = GetRValue(c);
    r.rgbReserved = 0;
    return r;
}


void DibBlt(HDC hdc, int x, int y, int sizex, int sizey, int srcx, int srcy, LPMODPLUGDIB lpdib)
//----------------------------------------------------------------------------------------------
{
    if (!lpdib) return;
    SetDIBitsToDevice(    hdc,
                        x,
                        y,
                        sizex,
                        sizey,
                        srcx,
                        lpdib->bmiHeader.biHeight - srcy - sizey,
                        0,
                        lpdib->bmiHeader.biHeight,
                        lpdib->lpDibBits,
                        (LPBITMAPINFO)lpdib,
                        DIB_RGB_COLORS);
}


LPMODPLUGDIB LoadDib(LPCSTR lpszName)
//-----------------------------------
{
    HINSTANCE hInstance = AfxGetInstanceHandle();
    HRSRC hrsrc = FindResource(hInstance, lpszName, RT_BITMAP);
    HGLOBAL hglb = LoadResource(hInstance, hrsrc);
    LPBITMAPINFO p = (LPBITMAPINFO)LockResource(hglb);
    if (p)
    {
        LPMODPLUGDIB pmd = new MODPLUGDIB;
        pmd->bmiHeader = p->bmiHeader;
        for (int i=0; i<16; i++) pmd->bmiColors[i] = p->bmiColors[i];
        LPBYTE lpDibBits = (LPBYTE)p;
        lpDibBits += p->bmiHeader.biSize + 16 * sizeof(RGBQUAD);
        pmd->lpDibBits = lpDibBits;
        return pmd;
    } else return NULL;
}


void DrawBitmapButton(HDC hdc, LPRECT lpRect, LPMODPLUGDIB lpdib, int srcx, int srcy, BOOL bPushed)
//-------------------------------------------------------------------------------------------------
{
    RECT rect;
    int x = (lpRect->right + lpRect->left) / 2 - 8;
    int y = (lpRect->top + lpRect->bottom) / 2 - 8;
    HGDIOBJ oldpen = SelectObject(hdc, CMainFrame::penBlack);
    rect.left = lpRect->left + 1;
    rect.top = lpRect->top + 1;
    rect.right = lpRect->right - 1;
    rect.bottom = lpRect->bottom - 1;
    if (bPushed)
    {
        ::MoveToEx(hdc, lpRect->left, lpRect->bottom-1, NULL);
        ::LineTo(hdc, lpRect->left, lpRect->top);
        ::LineTo(hdc, lpRect->right-1, lpRect->top);
        ::SelectObject(hdc, CMainFrame::penLightGray);
        ::LineTo(hdc, lpRect->right-1, lpRect->bottom-1);
        ::LineTo(hdc, lpRect->left, lpRect->bottom-1);
        ::MoveToEx(hdc, lpRect->left+1, lpRect->bottom-2, NULL);
        ::SelectObject(hdc, CMainFrame::penDarkGray);
        ::LineTo(hdc, lpRect->left+1, lpRect->top+1);
        ::LineTo(hdc, lpRect->right-2, lpRect->top+1);
        rect.left++;
        rect.top++;
        x++;
        y++;
    } else
    {
        ::MoveToEx(hdc, lpRect->right-1, lpRect->top, NULL);
        ::LineTo(hdc, lpRect->right-1, lpRect->bottom-1);
        ::LineTo(hdc, lpRect->left, lpRect->bottom-1);
        ::SelectObject(hdc, CMainFrame::penLightGray);
        ::LineTo(hdc, lpRect->left, lpRect->top);
        ::LineTo(hdc, lpRect->right-1, lpRect->top);
        ::SelectObject(hdc, CMainFrame::penDarkGray);
        ::MoveToEx(hdc, lpRect->right-2, lpRect->top+1, NULL);
        ::LineTo(hdc, lpRect->right-2, lpRect->bottom-2);
        ::LineTo(hdc, lpRect->left+1, lpRect->bottom-2);
        rect.right--;
        rect.bottom--;
    }
    ::FillRect(hdc, &rect, CMainFrame::brushGray);
    ::SelectObject(hdc, oldpen);
    DibBlt(hdc, x, y, 16, 15, srcx, srcy, lpdib);
}


void DrawButtonRect(HDC hdc, LPRECT lpRect, LPCSTR lpszText, BOOL bDisabled, BOOL bPushed, uint32_t dwFlags)
//-------------------------------------------------------------------------------------------------------
{
    RECT rect;
    HGDIOBJ oldpen = ::SelectObject(hdc, (bPushed) ? CMainFrame::penDarkGray : CMainFrame::penLightGray);
    ::MoveToEx(hdc, lpRect->left, lpRect->bottom-1, NULL);
    ::LineTo(hdc, lpRect->left, lpRect->top);
    ::LineTo(hdc, lpRect->right-1, lpRect->top);
    ::SelectObject(hdc, (bPushed) ? CMainFrame::penLightGray : CMainFrame::penDarkGray);
    ::LineTo(hdc, lpRect->right-1, lpRect->bottom-1);
    ::LineTo(hdc, lpRect->left, lpRect->bottom-1);
    rect.left = lpRect->left + 1;
    rect.top = lpRect->top + 1;
    rect.right = lpRect->right - 1;
    rect.bottom = lpRect->bottom - 1;
    ::FillRect(hdc, &rect, CMainFrame::brushGray);
    ::SelectObject(hdc, oldpen);
    if ((lpszText) && (lpszText[0]))
    {
        if (bPushed)
        {
            rect.top++;
            rect.left++;
        }
        ::SetTextColor(hdc, GetSysColor((bDisabled) ? COLOR_GRAYTEXT : COLOR_BTNTEXT));
        ::SetBkMode(hdc, TRANSPARENT);
        HGDIOBJ oldfont = ::SelectObject(hdc, CMainFrame::GetGUIFont());
        ::DrawText(hdc, lpszText, -1, &rect, dwFlags | DT_SINGLELINE | DT_NOPREFIX);
        ::SelectObject(hdc, oldfont);
    }
}


/////////////////////////////////////////////////////////////////////////////////////
// CButtonEx: button with custom bitmap

BEGIN_MESSAGE_MAP(CButtonEx, CButton)
    //{{AFX_MSG_MAP(CButtonEx)
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDBLCLK()
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()


BOOL CButtonEx::Init(const LPMODPLUGDIB pDib, COLORREF colorkey)
//--------------------------------------------------------------
{
    COLORREF btnface = GetSysColor(COLOR_BTNFACE);
    if (!pDib) return FALSE;
    m_Dib = *pDib;
    m_srcRect.left = 0;
    m_srcRect.top = 0;
    m_srcRect.right = 16;
    m_srcRect.bottom = 15;
    for (UINT i=0; i<16; i++)
    {
        COLORREF rgb = RGB(m_Dib.bmiColors[i].rgbRed, m_Dib.bmiColors[i].rgbGreen, m_Dib.bmiColors[i].rgbBlue);
        if (rgb == colorkey)
        {
            m_Dib.bmiColors[i].rgbRed = GetRValue(btnface);
            m_Dib.bmiColors[i].rgbGreen = GetGValue(btnface);
            m_Dib.bmiColors[i].rgbBlue = GetBValue(btnface);
        }
    }
    return TRUE;
}


void CButtonEx::SetPushState(BOOL bPushed)
//----------------------------------------
{
    m_bPushed = bPushed;
}


BOOL CButtonEx::SetSourcePos(int x, int y, int cx, int cy)
//--------------------------------------------------------
{
    m_srcRect.left = x;
    m_srcRect.top = y;
    m_srcRect.right = cx;
    m_srcRect.bottom = cy;
    return TRUE;
}


BOOL CButtonEx::AlignButton(HWND hwndPrev, int dx)
//------------------------------------------------
{
    HWND hwndParent = ::GetParent(m_hWnd);
    if (!hwndParent) return FALSE;
    if (hwndPrev)
    {
        POINT pt;
        RECT rect;
        SIZE sz;
        ::GetWindowRect(hwndPrev, &rect);
        pt.x = rect.left;
        pt.y = rect.top;
        sz.cx = rect.right - rect.left;
        sz.cy = rect.bottom - rect.top;
        ::ScreenToClient(hwndParent, &pt);
        SetWindowPos(NULL, pt.x + sz.cx + dx, pt.y, 0,0, SWP_NOZORDER|SWP_NOSIZE|SWP_NOACTIVATE);
    }
    return FALSE;
}


BOOL CButtonEx::AlignButton(UINT nIdPrev, int dx)
//-----------------------------------------------
{
    return AlignButton(::GetDlgItem(::GetParent(m_hWnd), nIdPrev), dx);
}


void CButtonEx::DrawItem(LPDRAWITEMSTRUCT lpdis)
//----------------------------------------------
{
    DrawBitmapButton(lpdis->hDC, &lpdis->rcItem, &m_Dib, m_srcRect.left, m_srcRect.top,
                        ((lpdis->itemState & ODS_SELECTED) || (m_bPushed)) ? TRUE : FALSE);
}


void CButtonEx::OnLButtonDblClk(UINT nFlags, CPoint point)
//--------------------------------------------------------
{
    PostMessage(WM_LBUTTONDOWN, nFlags, MAKELPARAM(point.x, point.y));
}


//////////////////////////////////////////////////////////////////////////////////
// Misc functions


UINT MsgBox(UINT nStringID, CWnd *p, LPCSTR lpszTitle, UINT n)
//------------------------------------------------------------
{
    CString str;
    str.LoadString(nStringID);
    HWND hwnd = (p) ? p->m_hWnd : NULL;
    return ::MessageBox(hwnd, str, lpszTitle, n);
}


void ErrorBox(UINT nStringID, CWnd*p)
//-----------------------------------
{
    MsgBox(nStringID, p, "Error!", MB_OK|MB_ICONSTOP);
}


////////////////////////////////////////////////////////////////////////////////
// CFastBitmap 8-bit output / 4-bit input
// useful for lots of small blits with color mapping
// combined in one big blit

void CFastBitmap::Init(LPMODPLUGDIB lpTextDib)
//--------------------------------------------
{
    m_nBlendOffset = 0;                    // rewbs.buildfix for pattern display bug in debug builds
                                // & release builds when ran directly from vs.net

    m_pTextDib = lpTextDib;
    MemsetZero(m_Dib);
    m_nTextColor = 0;
    m_nBkColor = 1;
    m_Dib.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    m_Dib.bmiHeader.biWidth = FASTBMP_MAXWIDTH;
    m_Dib.bmiHeader.biHeight = FASTBMP_MAXHEIGHT;
    m_Dib.bmiHeader.biPlanes = 1;
    m_Dib.bmiHeader.biBitCount = 8;
    m_Dib.bmiHeader.biCompression = BI_RGB;
    m_Dib.bmiHeader.biSizeImage = 0;
    m_Dib.bmiHeader.biXPelsPerMeter = 96;
    m_Dib.bmiHeader.biYPelsPerMeter = 96;
    m_Dib.bmiHeader.biClrUsed = 0;
    m_Dib.bmiHeader.biClrImportant = 256; // MAX_MODPALETTECOLORS;
    m_n4BitPalette[0] = (uint8_t)m_nTextColor;
    m_n4BitPalette[4] = MODCOLOR_SEPSHADOW;
    m_n4BitPalette[12] = MODCOLOR_SEPFACE;
    m_n4BitPalette[14] = MODCOLOR_SEPHILITE;
    m_n4BitPalette[15] = (uint8_t)m_nBkColor;
}


void CFastBitmap::Blit(HDC hdc, int x, int y, int cx, int cy)
//-----------------------------------------------------------
{
    SetDIBitsToDevice(    hdc,
                        x,
                        y,
                        cx,
                        cy,
                        0,
                        FASTBMP_MAXHEIGHT - cy,
                        0,
                        FASTBMP_MAXHEIGHT,
                        m_Dib.DibBits,
                        (LPBITMAPINFO)&m_Dib,
                        DIB_RGB_COLORS);
}


void CFastBitmap::SetColor(UINT nIndex, COLORREF cr)
//--------------------------------------------------
{
    if (nIndex < 256)
    {
        m_Dib.bmiColors[nIndex].rgbRed = GetRValue(cr);
        m_Dib.bmiColors[nIndex].rgbGreen = GetGValue(cr);
        m_Dib.bmiColors[nIndex].rgbBlue = GetBValue(cr);
    }
}


void CFastBitmap::SetAllColors(UINT nBaseIndex, UINT nColors, COLORREF *pcr)
//--------------------------------------------------------------------------
{
    for (UINT i=0; i<nColors; i++)
    {
        SetColor(nBaseIndex+i, pcr[i]);
    }
}


void CFastBitmap::SetBlendColor(COLORREF cr)
//------------------------------------------
{
    UINT r = GetRValue(cr);
    UINT g = GetGValue(cr);
    UINT b = GetBValue(cr);
    for (UINT i=0; i<0x80; i++)
    {
        UINT m = (m_Dib.bmiColors[i].rgbRed >> 2)
                + (m_Dib.bmiColors[i].rgbGreen >> 1)
                + (m_Dib.bmiColors[i].rgbBlue >> 2);
        m_Dib.bmiColors[i|0x80].rgbRed = static_cast<uint8_t>((m + r)>>1);
        m_Dib.bmiColors[i|0x80].rgbGreen = static_cast<uint8_t>((m + g)>>1);
        m_Dib.bmiColors[i|0x80].rgbBlue = static_cast<uint8_t>((m + b)>>1);
    }
}


// Monochrome 4-bit bitmap (0=text, !0 = back)
void CFastBitmap::TextBlt(int x, int y, int cx, int cy, int srcx, int srcy, LPMODPLUGDIB lpdib)
//---------------------------------------------------------------------------------------------
{
    const uint8_t *psrc;
    uint8_t *pdest;
    UINT x1, x2;
    int srcwidth, srcinc;

    m_n4BitPalette[0] = (uint8_t)m_nTextColor;
    m_n4BitPalette[15] = (uint8_t)m_nBkColor;
    if (x < 0)
    {
        cx += x;
        x = 0;
    }
    if (y < 0)
    {
        cy += y;
        y = 0;
    }
    if ((x >= FASTBMP_MAXWIDTH) || (y >= FASTBMP_MAXHEIGHT)) return;
    if (x+cx >= FASTBMP_MAXWIDTH) cx = FASTBMP_MAXWIDTH - x;
    if (y+cy >= FASTBMP_MAXHEIGHT) cy = FASTBMP_MAXHEIGHT - y;
    if (!lpdib) lpdib = m_pTextDib;
    if ((cx <= 0) || (cy <= 0) || (!lpdib)) return;
    srcwidth = (lpdib->bmiHeader.biWidth+1) >> 1;
    srcinc = srcwidth;
    if (((int)lpdib->bmiHeader.biHeight) > 0)
    {
        srcy = lpdib->bmiHeader.biHeight - 1 - srcy;
        srcinc = -srcinc;
    }
    x1 = srcx & 1;
    x2 = x1 + cx;
    pdest = m_Dib.DibBits + ((FASTBMP_MAXHEIGHT - 1 - y) << FASTBMP_XSHIFT) + x;
    psrc = lpdib->lpDibBits + (srcx >> 1) + (srcy * srcwidth);
    for (int iy=0; iy<cy; iy++)
    {
        LPBYTE p = pdest;
        UINT ix = x1;
        if (ix&1)
        {
            UINT b = psrc[ix >> 1];
            *p++ = m_n4BitPalette[b & 0x0F]+m_nBlendOffset;
            ix++;
        }
        while (ix+1 < x2)
        {
            UINT b = psrc[ix >> 1];
            p[0] = m_n4BitPalette[b >> 4]+m_nBlendOffset;
            p[1] = m_n4BitPalette[b & 0x0F]+m_nBlendOffset;
            ix+=2;
            p+=2;
        }
        if (x2&1)
        {
            UINT b = psrc[ix >> 1];
            *p++ = m_n4BitPalette[b >> 4]+m_nBlendOffset;
        }
        pdest -= FASTBMP_MAXWIDTH;
        psrc += srcinc;
    }
}


/////////////////////////////////////////////////////////////////////////////////////
// CMappedFile

CMappedFile::CMappedFile()
//------------------------
{
    m_hFMap = NULL;
    m_lpData = NULL;
}


CMappedFile::~CMappedFile()
//-------------------------
{
}


BOOL CMappedFile::Open(LPCSTR lpszFileName)
//-----------------------------------------
{
    return m_File.Open(lpszFileName, CFile::modeRead|CFile::typeBinary);
}


void CMappedFile::Close()
//-----------------------
{
    if (m_lpData) Unlock();
    m_File.Close();
}


uint32_t CMappedFile::GetLength()
//----------------------------
{
    return static_cast<uint32_t>(m_File.GetLength());
}


LPBYTE CMappedFile::Lock(uint32_t dwMaxLen)
//--------------------------------------
{
    uint32_t dwLen = GetLength();
    LPBYTE lpStream;

    if (!dwLen) return NULL;
    if ((dwMaxLen) && (dwLen > dwMaxLen)) dwLen = dwMaxLen;
    HANDLE hmf = CreateFileMapping(
                            (HANDLE)m_File.m_hFile,
                            NULL,
                            PAGE_READONLY,
                            0, 0,
                            NULL
                            );
    if (hmf)
    {
        lpStream = (LPBYTE)MapViewOfFile(
                                hmf,
                                FILE_MAP_READ,
                                0, 0,
                                0
                            );
        if (lpStream)
        {
            m_hFMap = hmf;
            m_lpData = lpStream;
            return lpStream;
        }
        CloseHandle(hmf);
    }
    if (dwLen > CTrackApp::gMemStatus.dwTotalPhys) return NULL;
    if ((lpStream = (LPBYTE)GlobalAllocPtr(GHND, dwLen)) == NULL) return NULL;
    m_File.Read(lpStream, dwLen);
    m_lpData = lpStream;
    return lpStream;
}


BOOL CMappedFile::Unlock()
//------------------------
{
    if (m_hFMap)
    {
        if (m_lpData)
        {
            UnmapViewOfFile(m_lpData);
            m_lpData = NULL;
        }
        CloseHandle(m_hFMap);
        m_hFMap = NULL;
    }
    if (m_lpData)
    {
        GlobalFreePtr(m_lpData);
        m_lpData = NULL;
    }
    return TRUE;
}


///////////////////////////////////////////////////////////////////////////////////
//
// DirectX Plugins
//

BOOL CTrackApp::deprecated_InitializeDXPlugins()
//-----------------------------------
{
    TCHAR s[_MAX_PATH], tmp[32];
    LONG nPlugins;

    m_pPluginManager = new CVstPluginManager;
    if (!m_pPluginManager) return FALSE;
    nPlugins = GetPrivateProfileInt("VST Plugins", "NumPlugins", 0, m_szConfigFileName);

    #ifndef NO_VST
        char buffer[64];
        GetPrivateProfileString("VST Plugins", "HostProductString", CVstPluginManager::s_szHostProductString, buffer, CountOf(buffer), m_szConfigFileName);
        strcpy(CVstPluginManager::s_szHostProductString, buffer);
        GetPrivateProfileString("VST Plugins", "HostVendorString", CVstPluginManager::s_szHostVendorString, buffer, CountOf(buffer), m_szConfigFileName);
        strcpy(CVstPluginManager::s_szHostVendorString, buffer);
        CVstPluginManager::s_nHostVendorVersion = GetPrivateProfileInt("VST Plugins", "HostVendorVersion", CVstPluginManager::s_nHostVendorVersion, m_szConfigFileName);
    #endif


    CString nonFoundPlugs;
    for (LONG iPlug=0; iPlug<nPlugins; iPlug++)
    {
        s[0] = 0;
        wsprintf(tmp, "Plugin%d", iPlug);
        GetPrivateProfileString("VST Plugins", tmp, "", s, sizeof(s), m_szConfigFileName);
        if (s[0])
        {
            CMainFrame::RelativePathToAbsolute(s);
            m_pPluginManager->AddPlugin(s, TRUE, true, &nonFoundPlugs);
        }
    }
    if(nonFoundPlugs.GetLength() > 0)
    {
        nonFoundPlugs.Insert(0, "Problems were encountered with plugins:\n");
        MessageBox(NULL, nonFoundPlugs, "", MB_OK);
    }
    return FALSE;
}


BOOL CTrackApp::deprecated_UninitializeDXPlugins()
//-------------------------------------
{
    TCHAR s[_MAX_PATH], tmp[32];
    PVSTPLUGINLIB pPlug;
    UINT iPlug;

    if (!m_pPluginManager) return FALSE;
    pPlug = m_pPluginManager->GetFirstPlugin();
    iPlug = 0;
    while (pPlug)
    {
        s[0] = 0;
        wsprintf(tmp, "Plugin%d", iPlug);
        strcpy(s, pPlug->szDllPath);
        if(theApp.IsPortableMode())
        {
            CMainFrame::AbsolutePathToRelative(s);
        }
        WritePrivateProfileString("VST Plugins", tmp, s, m_szConfigFileName);
        iPlug++;
        pPlug = pPlug->pNext;
    }
    wsprintf(s, "%d", iPlug);
    WritePrivateProfileString("VST Plugins", "NumPlugins", s, m_szConfigFileName);

    #ifndef NO_VST
        WritePrivateProfileString("VST Plugins", "HostProductString", CVstPluginManager::s_szHostProductString, m_szConfigFileName);
        WritePrivateProfileString("VST Plugins", "HostVendorString", CVstPluginManager::s_szHostVendorString, m_szConfigFileName);
        CMainFrame::WritePrivateProfileLong("VST Plugins", "HostVendorVersion", CVstPluginManager::s_nHostVendorVersion, m_szConfigFileName);
    #endif


    if (m_pPluginManager)
    {
        delete m_pPluginManager;
        m_pPluginManager = NULL;
    }
    return TRUE;
}


///////////////////////////////////////////////////////////////////////////////////
// Debug

void Log(LPCSTR format, ...) {
#ifdef _DEBUG
    va_list arglist;
    va_start(arglist, format);
    modplug::pervasives::vdebug_log(format, arglist);
    va_end(arglist);
#endif
}


//////////////////////////////////////////////////////////////////////////////////
// Localized strings

VOID CTrackApp::ImportLocalizedStrings()
//--------------------------------------
{
    //uint32_t dwLangId = ((uint32_t)GetUserDefaultLangID()) & 0xfff;
    // TODO: look up [Strings.lcid], [Strings.(lcid&0xff)] & [Strings] in mpt_intl.ini
}


BOOL CTrackApp::GetLocalizedString(LPCSTR pszName, LPSTR pszStr, UINT cbSize)
//---------------------------------------------------------------------------
{
    CHAR s[32];
    uint32_t dwLangId = ((uint32_t)GetUserDefaultLangID()) & 0xffff;

    pszStr[0] = 0;
    if (!m_szStringsFileName[0]) return FALSE;
    wsprintf(s, "Strings.%04X", dwLangId);
    GetPrivateProfileString(s, pszName, "", pszStr, cbSize, m_szStringsFileName);
    if (pszStr[0]) return TRUE;
    wsprintf(s, "Strings.%04X", dwLangId&0xff);
    GetPrivateProfileString(s, pszName, "", pszStr, cbSize, m_szStringsFileName);
    if (pszStr[0]) return TRUE;
    wsprintf(s, "Strings", dwLangId&0xff);
    GetPrivateProfileString(s, pszName, "", pszStr, cbSize, m_szStringsFileName);
    if (pszStr[0]) return TRUE;
    return FALSE;
}

//rewbs.crashHandler
LRESULT CTrackApp::ProcessWndProcException(CException* e, const MSG* pMsg)
//-----------------------------------------------------------------------
{
    // TODO: Add your specialized code here and/or call the base class
    Log("Unhandled Exception\n");
    Log("Attempting to close sound device\n");

    return CWinApp::ProcessWndProcException(e, pMsg);
}
//end rewbs.crashHandler


/* Open or save one or multiple files using the system's file dialog
 * Parameter list:
 * - load: true: load dialog. false: save dialog.
 * - defaultExtension: dialog should use this as the default extension for the file(s)
 * - defaultFilename: dialog should use this as the default filename
 * - extFilter: list of possible extensions. format: "description|extensions|...|description|extensions||"
 * - workingDirectory: default directory of the dialog
 * - allowMultiSelect: allow the user to select multiple files? (will be ignored if load == false)
 * - filterIndex: pointer to a variable holding the index of the last extension filter used.
 */
FileDlgResult CTrackApp::ShowOpenSaveFileDialog(const bool load, const std::string defaultExtension, const std::string defaultFilename, const std::string extFilter, const std::string workingDirectory, const bool allowMultiSelect, int *filterIndex)
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
{
    FileDlgResult result;
    result.workingDirectory = workingDirectory;
    result.first_file = "";
    result.filenames.clear();
    result.extension = defaultExtension;
    result.abort = true;

    // we can't save multiple files.
    const bool multiSelect = allowMultiSelect && load;

    // First, set up the dialog...
    CFileDialog dlg(load ? TRUE : FALSE,
        defaultExtension.empty() ? NULL : defaultExtension.c_str(),
        defaultFilename.empty() ? NULL : defaultFilename.c_str(),
        load ? (OFN_HIDEREADONLY | OFN_ENABLESIZING | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | (multiSelect ? OFN_ALLOWMULTISELECT : 0))
             : (OFN_HIDEREADONLY | OFN_ENABLESIZING | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOREADONLYRETURN),
        extFilter.empty() ? NULL : extFilter.c_str(),
        theApp.m_pMainWnd);
    if(!workingDirectory.empty())
        dlg.m_ofn.lpstrInitialDir = workingDirectory.c_str();
    if(filterIndex != nullptr)
        dlg.m_ofn.nFilterIndex = (uint32_t)(*filterIndex);

    vector<TCHAR> filenameBuffer;
    if(multiSelect)
    {
        const size_t bufferSize = 2048; // Note: This is possibly the maximum buffer size in MFC 7(this note was written November 2006).
        filenameBuffer.resize(bufferSize, 0);
        dlg.GetOFN().lpstrFile = &filenameBuffer[0];
        dlg.GetOFN().nMaxFile = bufferSize;
    }

    if(dlg.DoModal() != IDOK) {
        return result;
    }

    // Retrieve variables
    if(filterIndex != nullptr)
        *filterIndex = dlg.m_ofn.nFilterIndex;

    if(multiSelect)
    {
        // multiple files might have been selected
        POSITION pos = dlg.GetStartPosition();
        while(pos != NULL)
        {
            std::string filename = dlg.GetNextPathName(pos);
            result.filenames.push_back(filename);
        }

    } else
    {
        // only one file
        std::string filename = dlg.GetPathName();
        result.filenames.push_back(filename);
    }

    if(!result.filenames.empty())
    {
        // some file has been selected.
        result.workingDirectory = result.filenames.back();
        result.first_file = result.filenames.front();
        result.abort = false;
    }

    result.extension = dlg.GetFileExt();

    return result;
}
