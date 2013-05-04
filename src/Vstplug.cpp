#include "stdafx.h"
#include <uuids.h>
#include <dmoreg.h>
#include <shlwapi.h>
#include <medparam.h>
#include "mainfrm.h"
#include "vstplug.h"
#include "moddoc.h"
#include "legacy_soundlib/sndfile.h"
#include "fxp.h"                                    //rewbs.VSTpresets
#include "AbstractVstEditor.h"            //rewbs.defaultPlugGUI
#include "VstEditor.h"                            //rewbs.defaultPlugGUI
#include "defaultvsteditor.h"            //rewbs.defaultPlugGUI
#include "legacy_soundlib/midi.h"
#include "version.h"
#include "midimappingdialog.h"

#include "pervasives/pervasives.h"
using namespace modplug::pervasives;
using namespace modplug::tracker;

#ifdef VST_USE_ALTERNATIVE_MAGIC    //Pelya's plugin ID fix. Breaks fx presets, so let's avoid it for now.
#define ZLIB_WINAPI
#include "../zlib/zlib.h"                    //For CRC32 calculation (to detect plugins with same UID)
#endif // VST_USE_ALTERNATIVE_MAGIC

#define new DEBUG_NEW

#ifndef NO_VST

char CVstPluginManager::s_szHostProductString[64] = "OpenMPT";
char CVstPluginManager::s_szHostVendorString[64] = "OpenMPT project";
VstIntPtr CVstPluginManager::s_nHostVendorVersion = MptVersion::num;

//#define VST_LOG

#ifdef VST_USE_ALTERNATIVE_MAGIC
UINT32 CalculateCRC32fromFilename(const char * s)
//-----------------------------------------------
{
    char fn[_MAX_PATH];
    strncpy(fn, s, sizeof(fn));
    fn[sizeof(fn)-1] = 0;
    int f;
    for(f = 0; fn[f] != 0; f++) fn[f] = toupper(fn[f]);
    return crc32(0, (uint8_t *)fn, f);

}
#endif // VST_USE_ALTERNATIVE_MAGIC

VstIntPtr VSTCALLBACK CVstPluginManager::MasterCallBack(AEffect *effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void *ptr, float opt)
//----------------------------------------------------------------------------------------------------------------------------------------------
{
    CVstPluginManager *that = theApp.GetPluginManager();
    if (that)
    {
        return that->VstCallback(effect, opcode, index, value, ptr, opt);
    }
    return 0;
}


BOOL CVstPluginManager::CreateMixPluginProc(PSNDMIXPLUGIN pMixPlugin, module_renderer* pSndFile)
//-------------------------------------------------------------------------------------
{
    CVstPluginManager *that = theApp.GetPluginManager();
    if (that)
    {
        return that->CreateMixPlugin(pMixPlugin, pSndFile);
    }
    return FALSE;
}


CVstPluginManager::CVstPluginManager()
//------------------------------------
{
    m_pVstHead = NULL;
    //m_bNeedIdle = FALSE; //rewbs.VSTCompliance - now member of plugin class
    module_renderer::gpMixPluginCreateProc = CreateMixPluginProc;
}


CVstPluginManager::~CVstPluginManager()
//-------------------------------------
{
    module_renderer::gpMixPluginCreateProc = NULL;
    while (m_pVstHead)
    {
        PVSTPLUGINLIB p = m_pVstHead;
        m_pVstHead = m_pVstHead->pNext;
        if (m_pVstHead) m_pVstHead->pPrev = NULL;
        p->pPrev = p->pNext = NULL;
        while ((volatile void *)(p->pPluginsList) != NULL)
        {
            p->pPluginsList->Release();
        }
        delete p;
    }
}


BOOL CVstPluginManager::IsValidPlugin(const VSTPLUGINLIB *pLib)
//-------------------------------------------------------------
{
    PVSTPLUGINLIB p = m_pVstHead;
    while (p)
    {
        if (p == pLib) return TRUE;
        p = p->pNext;
    }
    return FALSE;
}


//
// PluginCache format:
// LibraryName = ID100000ID200000
// ID100000ID200000 = FullDllPath
// ID100000ID200000.Flags = Plugin Flags (for now, just isInstrument).



PVSTPLUGINLIB CVstPluginManager::AddPlugin(LPCSTR pszDllPath, BOOL bCache, const bool checkFileExistence, CString* const errStr)
//------------------------------------------------------------------------------------------------------------------------------
{
    TCHAR szPath[_MAX_PATH];

    if(checkFileExistence && (PathFileExists(pszDllPath) == FALSE))
    {
        if(errStr)
        {
            *errStr += "\nUnable to find ";
            *errStr += pszDllPath;
        }
    }

    PVSTPLUGINLIB pDup = m_pVstHead;
    while (pDup)
    {
        if (!lstrcmpi(pszDllPath, pDup->szDllPath)) return pDup;
        pDup = pDup->pNext;
    }
    // Look if the plugin info is stored in the PluginCache
    if (bCache)
    {
        const CString cacheSection = "PluginCache";
        const CString cacheFile = theApp.GetPluginCacheFileName();
        char fileName[_MAX_PATH];
        _splitpath(pszDllPath, NULL, NULL, fileName, NULL);

        CString IDs = CMainFrame::GetPrivateProfileCString(cacheSection, fileName, "", cacheFile);

        if (IDs.GetLength() >= 16)
        {
            // Get path from cache file
            GetPrivateProfileString(cacheSection, IDs, "", szPath, CountOf(szPath) - 1, cacheFile);
            SetNullTerminator(szPath);
            CMainFrame::RelativePathToAbsolute(szPath);

            if ((szPath[0]) && (!lstrcmpi(szPath, pszDllPath)))
            {
                PVSTPLUGINLIB p = new VSTPLUGINLIB;
                if (!p) return NULL;
                p->dwPluginId1 = 0;
                p->dwPluginId2 = 0;
                p->bIsInstrument = FALSE;
                p->pPluginsList = NULL;
                lstrcpyn(p->szDllPath, pszDllPath, sizeof(p->szDllPath));
                _splitpath(pszDllPath, NULL, NULL, p->szLibraryName, NULL);
                p->szLibraryName[63] = 0;
                p->pNext = m_pVstHead;
                p->pPrev = NULL;
                if (m_pVstHead) m_pVstHead->pPrev = p;
                m_pVstHead = p;
                // Extract plugin Ids
                for (UINT i=0; i<16; i++)
                {
                    UINT n = ((LPCTSTR)IDs)[i] - '0';
                    if (n > 9) n = ((LPCTSTR)IDs)[i] + 10 - 'A';
                    n &= 0x0f;
                    if (i < 8)
                    {
                        p->dwPluginId1 = (p->dwPluginId1<<4) | n;
                    } else
                    {
                        p->dwPluginId2 = (p->dwPluginId2<<4) | n;
                    }
                }
                CString flagKey;
                flagKey.Format("%s.Flags", IDs);
                int infoex = CMainFrame::GetPrivateProfileLong(cacheSection, flagKey, 0, cacheFile);
                if (infoex&1) p->bIsInstrument = TRUE;
            #ifdef VST_USE_ALTERNATIVE_MAGIC
                if( p->dwPluginId1 == kEffectMagic )
                {
                    p->dwPluginId1 = CalculateCRC32fromFilename(p->szLibraryName); // Make Plugin ID unique for sure (for VSTs with same UID)
                };
            #endif // VST_USE_ALTERNATIVE_MAGIC
            #ifdef VST_LOG
                Log("Plugin \"%s\" found in PluginCache\n", p->szLibraryName);
            #endif // VST_LOG
                return p;
            } else
            {
            #ifdef VST_LOG
                Log("Plugin \"%s\" mismatch in PluginCache: \"%s\" [%s]=\"%s\"\n", s, pszDllPath, (LPCTSTR)IDs, (LPCTSTR)strFullPath);
            #endif // VST_LOG
            }
        }
    }

    HINSTANCE hLib = NULL;


    try
    {
            hLib = LoadLibrary(pszDllPath);
    //rewbs.VSTcompliance
#ifdef _DEBUG
            uint32_t dw = GetLastError();
            if (!hLib && dw != ERROR_MOD_NOT_FOUND)    // "File not found errors" are annoying.
            {
                TCHAR szBuf[256];
                wsprintf(szBuf, "Warning: encountered problem when loading plugin dll. Error %d: %s", dw, GetErrorMessage(dw));
                MessageBox(NULL, szBuf, "DEBUG: Error when loading plugin dll", MB_OK);
            }
#endif //_DEBUG
    //end rewbs.VSTcompliance
    } catch(...)
    {
        CVstPluginManager::ReportPlugException("Exception caught in LoadLibrary (%s)", pszDllPath);
    }
    if ((hLib) && (hLib != INVALID_HANDLE_VALUE))
    {
        BOOL bOk = FALSE;
        PVSTPLUGENTRY pMainProc = (PVSTPLUGENTRY)GetProcAddress(hLib, "main");
        if (pMainProc)
        {
            PVSTPLUGINLIB p = new VSTPLUGINLIB;
            if (!p) return NULL;
            p->dwPluginId1 = 0;
            p->dwPluginId2 = 0;
            p->bIsInstrument = FALSE;
            p->pPluginsList = NULL;
            lstrcpyn(p->szDllPath, pszDllPath, sizeof(p->szDllPath));
            _splitpath(pszDllPath, NULL, NULL, p->szLibraryName, NULL);
            p->szLibraryName[63] = 0;
            p->pNext = m_pVstHead;
            p->pPrev = NULL;
            if (m_pVstHead) m_pVstHead->pPrev = p;
            m_pVstHead = p;

            try {
                AEffect *pEffect = pMainProc(MasterCallBack);
                if ((pEffect) && (pEffect->magic == kEffectMagic)
                 && (pEffect->dispatcher))
                {
                    pEffect->dispatcher(pEffect, effOpen, 0,0,0,0);
                #ifdef VST_USE_ALTERNATIVE_MAGIC
                    p->dwPluginId1 = CalculateCRC32fromFilename(p->szLibraryName); // Make Plugin ID unique for sure
                #else
                    p->dwPluginId1 = pEffect->magic;
                #endif // VST_USE_ALTERNATIVE_MAGIC
                    p->dwPluginId2 = pEffect->uniqueID;
                    if ((pEffect->flags & effFlagsIsSynth) || (!pEffect->numInputs)) p->bIsInstrument = TRUE;
                #ifdef VST_LOG
                    int nver = pEffect->dispatcher(pEffect, effGetVstVersion, 0,0, NULL, 0);
                    if (!nver) nver = pEffect->version;
                    Log("%-20s: v%d.0, %d in, %d out, %2d programs, %2d params, flags=0x%04X realQ=%d offQ=%d\n",
                        p->szLibraryName, nver,
                        pEffect->numInputs, pEffect->numOutputs,
                        pEffect->numPrograms, pEffect->numParams,
                        pEffect->flags, pEffect->realQualities, pEffect->offQualities);
                #endif // VST_LOG
                    pEffect->dispatcher(pEffect, effClose, 0,0,0,0);
                    bOk = TRUE;
                }
            } catch(...) {
                CVstPluginManager::ReportPlugException("Exception while trying to load plugin \"%s\"!\n", p->szLibraryName);
            }

            // If OK, write the information in PluginCache
            if (bOk)
            {
                CString cacheSection = "PluginCache";
                CString cacheFile = theApp.GetPluginCacheFileName();
                CString IDs, flagsKey;
                IDs.Format("%08X%08X", p->dwPluginId1, p->dwPluginId2);
                flagsKey.Format("%s.Flags", IDs);

                _tcsncpy(szPath, pszDllPath, CountOf(szPath) - 1);
                if(theApp.IsPortableMode())
                {
                    CMainFrame::AbsolutePathToRelative(szPath);
                }
                SetNullTerminator(szPath);

                WritePrivateProfileString(cacheSection, IDs, szPath, cacheFile);
                CMainFrame::WritePrivateProfileCString(cacheSection, IDs, pszDllPath, cacheFile);
                CMainFrame::WritePrivateProfileCString(cacheSection, p->szLibraryName, IDs, cacheFile);
                CMainFrame::WritePrivateProfileLong(cacheSection, flagsKey, p->bIsInstrument, cacheFile);
            }
        } else {
        #ifdef VST_LOG
            Log("Entry point not found!\n");
        #endif // VST_LOG
        }

        try {
            FreeLibrary(hLib);
        } catch (...) {
            CVstPluginManager::ReportPlugException("Exception in FreeLibrary(\"%s\")!\n", pszDllPath);
        }

        return (bOk) ? m_pVstHead : NULL;
    } else
    {
    #ifdef VST_LOG
        Log("LoadLibrary(%s) failed!\n", pszDllPath);
    #endif // VST_LOG
    }
    return NULL;
}


BOOL CVstPluginManager::RemovePlugin(PVSTPLUGINLIB pFactory)
//----------------------------------------------------------
{
    PVSTPLUGINLIB p = m_pVstHead;

    while (p)
    {
        if (p == pFactory)
        {
            if (p == m_pVstHead) m_pVstHead = p->pNext;
            if (p->pPrev) p->pPrev->pNext = p->pNext;
            if (p->pNext) p->pNext->pPrev = p->pPrev;
            p->pPrev = p->pNext = NULL;
            BEGIN_CRITICAL();

            try {
                while ((volatile void *)(p->pPluginsList) != NULL)
                {
                    p->pPluginsList->Release();
                }
                delete p;
            } catch (...) {
                CVstPluginManager::ReportPlugException("Exception while trying to release plugin \"%s\"!\n", pFactory->szLibraryName);
            }

            END_CRITICAL();
            return TRUE;
        }
        p = p->pNext;
    }
    return FALSE;
}


BOOL CVstPluginManager::CreateMixPlugin(PSNDMIXPLUGIN pMixPlugin, module_renderer* pSndFile)
//-------------------------------------------------------------------------------------
{
    UINT nMatch=0;
    PVSTPLUGINLIB pFound = NULL;

    if (pMixPlugin)
    {
        PVSTPLUGINLIB p = m_pVstHead;

        while (p)
        {
            if (pMixPlugin)
            {
                bool b1 = false, b2 = false;
                if ((p->dwPluginId1 == pMixPlugin->Info.dwPluginId1)
                 && (p->dwPluginId2 == pMixPlugin->Info.dwPluginId2))
                {
                    b1 = true;
                }
                if (!_strnicmp(p->szLibraryName, pMixPlugin->Info.szLibraryName, 64))
                {
                    b2 = true;
                }
                if ((b1) && (b2))
                {
                    nMatch = 3;
                    pFound = p;
                } else
                if ((b1) && (nMatch < 2))
                {
                    nMatch = 2;
                    pFound = p;
                } else
                if ((b2) && (nMatch < 1))
                {
                    nMatch = 1;
                    pFound = p;
                }
            }
            p = p->pNext;
        }
    }
    if ((!pFound) && (pMixPlugin->Info.szLibraryName[0]))
    {
        CHAR s[_MAX_PATH], dir[_MAX_PATH];
        _splitpath(theApp.GetConfigFileName(), s, dir, NULL, NULL);
        strcat(s, dir);
        int len = strlen(s);
        if ((len > 0) && (s[len-1] != '\\')) strcat(s, "\\");
        strncat(s, "plugins\\", _MAX_PATH-1);
        strncat(s, pMixPlugin->Info.szLibraryName, _MAX_PATH-1);
        strncat(s, ".dll", _MAX_PATH-1);
        pFound = AddPlugin(s);
        if (!pFound)
        {
            CString cacheSection = "PluginCache";
            CString cacheFile = theApp.GetPluginCacheFileName();
            CString IDs = CMainFrame::GetPrivateProfileCString(cacheSection, pMixPlugin->Info.szLibraryName, "", cacheFile);
            if (IDs.GetLength() >= 16)
            {
                CString strFullPath = CMainFrame::GetPrivateProfileCString(cacheSection, IDs, "", cacheFile);
                if ((strFullPath) && (strFullPath[0])) pFound = AddPlugin(strFullPath);
            }
        }
    }
    if (pFound)
    {
        HINSTANCE hLibrary = NULL;
        BOOL bOk = FALSE;

        BEGIN_CRITICAL();

        try {
            hLibrary = LoadLibrary(pFound->szDllPath);
            if (hLibrary != NULL)
            {
                AEffect *pEffect = NULL;
                PVSTPLUGENTRY pEntryPoint = (PVSTPLUGENTRY)GetProcAddress(hLibrary, "main");
                if (pEntryPoint)
                {
                    pEffect = pEntryPoint(MasterCallBack);
                } else
                {
                #ifdef VST_LOG
                    Log("Entry point not found! (handle=%08X)\n", hLibrary);
                #endif
                }
                if (pEffect && pEffect->dispatcher && (pEffect->magic == kEffectMagic))
                {
                    bOk = TRUE;
                    if ((pEffect->flags & effFlagsIsSynth) || (!pEffect->numInputs))
                    {
                        if (!pFound->bIsInstrument)
                        {
                            CString cacheSection = "PluginCache";
                            CString cacheFile = theApp.GetPluginCacheFileName();
                            //LPCSTR pszSection = "PluginCache";
                            pFound->bIsInstrument = TRUE;
                            CString flagsKey;
                            flagsKey.Format("%08X%08X.Flags", pFound->dwPluginId1, pFound->dwPluginId2);
                            CMainFrame::WritePrivateProfileLong(cacheSection, flagsKey, 1, cacheFile);
                        }
                    }
                    CVstPlugin *pVstPlug = new CVstPlugin(hLibrary, pFound, pMixPlugin, pEffect);
                    if (pVstPlug) pVstPlug->Initialize(pSndFile);
                }
            } else
            {
            #ifdef VST_LOG
                Log("LoadLibrary(\"%s\") failed!\n", pFound->szDllPath);
            #endif
            }
            if ((!bOk) && (hLibrary)) FreeLibrary(hLibrary);
        } catch(...) {
            CVstPluginManager::ReportPlugException("Exception while trying to create plugin \"%s\"!\n", pFound->szLibraryName);
        }

        END_CRITICAL();
        return bOk;
    } else
    {
        // "plug not found" notification code MOVED to CSoundFile::Create
        #ifdef VST_LOG
            Log("Unknown plugin\n");
        #endif
    }
    return FALSE;
}


VOID CVstPluginManager::OnIdle()
//------------------------------
{
    PVSTPLUGINLIB pFactory = m_pVstHead;

    while (pFactory)
    {
        CVstPlugin *p = pFactory->pPluginsList;
        while (p)
        {
            //rewbs. VSTCompliance: A specific plug has requested indefinite periodic processing time.
            if (p->m_bNeedIdle)
            {
                if (!(p->Dispatch(effIdle, 0,0, NULL, 0)))
                    p->m_bNeedIdle=false;
            }
            //We need to update all open editors
            if ((p->m_pEditor) && (p->m_pEditor->m_hWnd)) {
                p->m_pEditor->UpdateParamDisplays();
            }
            //end rewbs. VSTCompliance:

            p = p->m_pNext;
        }
        pFactory = pFactory->pNext;
    }

}

//rewbs.VSTCompliance: Added support for lots of opcodes
VstIntPtr CVstPluginManager::VstCallback(AEffect *effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void *ptr, float /*opt*/)
//-----------------------------------------------------------------------------------------------------------------------------------
{
    #ifdef VST_LOG
    Log("VST plugin to host: Eff: 0x%.8X, Opcode = %d, Index = %d, Value = %d, PTR = %.8X, OPT = %.3f\n",(int)effect, opcode,index,value,(int)ptr,opt);
    #endif

    enum enmHostCanDo
    {
        HostDoNotKnow    = 0,
        HostCanDo            = 1,
        HostCanNotDo    = -1
    };

    switch(opcode)
    {
    // Called when plugin param is changed via gui
    case audioMasterAutomate:
        if (effect && effect->resvd1)
        {
            CVstPlugin *pVstPlugin = ((CVstPlugin*)effect->resvd1);

            //Mark track modified
            CModDoc* pModDoc = pVstPlugin->GetModDoc();
            if (pModDoc) {
                CAbstractVstEditor *pVstEditor = pVstPlugin->GetEditor();
                if (pVstEditor && pVstEditor->m_hWnd)    // Check GUI is open
                {
                    if(pModDoc->GetSoundFile() && pModDoc->GetSoundFile()->GetModSpecifications().supportsPlugins)
                        CMainFrame::GetMainFrame()->ThreadSafeSetModified(pModDoc);
                }
                // Could be used to update general tab in real time, but causes flickers in treeview
                // Better idea: add an update hint just for plugin params?
                //pModDoc->UpdateAllViews(NULL, HINT_MIXPLUGINS, NULL);
            }

            //Record param change
            if (pVstPlugin->m_bRecordAutomation) {
                pModDoc->RecordParamChange(pVstPlugin->GetSlot(), index);
            }

            // Learn macro
            CAbstractVstEditor *pVstEditor = pVstPlugin->GetEditor();
            if (pVstEditor) {
                int macroToLearn = pVstEditor->GetLearnMacro();
                if (macroToLearn>-1) {
                    pModDoc->LearnMacro(macroToLearn, index);
                    pVstEditor->SetLearnMacro(-1);
                }
                //Commenting this out to see if it fixes http://www.modplug.com/forum/viewtopic.php?t=3710
                //pVstPlugin->Dispatch(effEditIdle, 0,0, NULL, 0);
            }
        }
        return 0;
    // Called when plugin asks for VST version supported by host
    case audioMasterVersion:    return kVstVersion;
    // Returns the unique id of a plug that's currently loading
    // (not sure what this is actually for - we return *effect's UID, cos Herman Seib does something similar :)
    //  Let's see what happens...)
    case audioMasterCurrentId:
        return (effect != nullptr) ? effect->uniqueID : 0;
    // Call application idle routine (this will call effEditIdle for all open editors too)
    case audioMasterIdle:
        OnIdle();
        return 0;

    // Inquire if an input or output is beeing connected; index enumerates input or output counting from zero,
    // value is 0 for input and != 0 otherwise. note: the return value is 0 for <true> such that older versions
    // will always return true.
    case audioMasterPinConnected:
        //if (effect)
        //{
        //    OnIdle();
        //    if (value)
        //            return ((index < effect->numOutputs) && (index < 2)) ? 0 : 1;
        //    else
        //            return ((index < effect->numInputs) && (index < 2)) ? 0 : 1;
        //}
        //Olivier seemed to do some superfluous stuff (above). Time will tell if it was necessary.
        if (value)    //input:
            return (index < 2) ? 0 : 1;            //we only support up to 2 inputs. Remember: 0 means yes.
        else            //output:
            return (index < 2) ? 0 : 1;            //2 outputs bad_max too

    //---from here VST 2.0 extension opcodes------------------------------------------------------

    // <value> is a filter which is currently ignored - DEPRECATED in VST 2.4
    // Herman Seib only processes Midi events for plugs that call this. Keep in mind.
    case audioMasterWantMidi:    return 1;
    // returns const VstTimeInfo* (or 0 if not supported)
    // <value> should contain a mask indicating which fields are required
    case audioMasterGetTime: {

        CVstPlugin* pVstPlugin = (CVstPlugin*)effect->resvd1;
        MemsetZero(timeInfo);
        timeInfo.sampleRate = CMainFrame::GetMainFrame()->GetSampleRate();

        if (pVstPlugin)
        {
            module_renderer* pSndFile = pVstPlugin->GetSoundFile();

            if (pVstPlugin->IsSongPlaying())
            {
                timeInfo.flags |= kVstTransportPlaying;
                timeInfo.samplePos = CMainFrame::GetMainFrame()->GetTotalSampleCount();
                if (timeInfo.samplePos == 0) //samplePos=0 means we just started playing
                {
                    timeInfo.flags |= kVstTransportChanged;
                }
            } else
            {
                timeInfo.flags |= kVstTransportChanged; //just stopped.
                timeInfo.samplePos = 0;
            }
            if ((value & kVstNanosValid))
            {
                timeInfo.flags |= kVstNanosValid;
                timeInfo.nanoSeconds = timeGetTime() * 1000000;
            }
            if ((value & kVstPpqPosValid) && pSndFile)
            {
                timeInfo.flags |= kVstPpqPosValid;
                if (timeInfo.flags & kVstTransportPlaying)
                {
                    timeInfo.ppqPos = (timeInfo.samplePos/timeInfo.sampleRate)*(pSndFile->GetCurrentBPM()/60.0);
                } else
                {
                    timeInfo.ppqPos = 0;
                }
            }
            if ((value & kVstTempoValid) && pSndFile)
            {
                timeInfo.tempo = pSndFile->GetCurrentBPM();
                if (timeInfo.tempo)
                {
                    timeInfo.flags |= kVstTempoValid;
                }
            }
            if ((value & kVstTimeSigValid) && pSndFile)
            {
                timeInfo.flags |= kVstTimeSigValid;

                // Time signature. numerator = rows per beats / rows pear measure (should sound somewhat logical to you).
                // the denominator is a bit more tricky, since it cannot be set explicitely. so we just assume quarters for now.
                timeInfo.timeSigNumerator = pSndFile->m_nCurrentRowsPerMeasure / bad_max(pSndFile->m_nCurrentRowsPerBeat, 1);
                timeInfo.timeSigDenominator = 4; //gcd(pSndFile->m_nCurrentRowsPerMeasure, pSndFile->m_nCurrentRowsPerBeat);
            }
        }
        return ToVstPtr(&timeInfo);
    }
    // VstEvents* in <ptr>
    // We don't support plugs that send VSTEvents to the host
    case audioMasterProcessEvents:
        Log("VST plugin to host: Process Events\n");
        break;
    // DEPRECATED in VST 2.4
    case audioMasterSetTime:
        Log("VST plugin to host: Set Time\n");
        break;
    // returns tempo (in bpm * 10000) at sample frame location passed in <value> - DEPRECATED in VST 2.4
    case audioMasterTempoAt:
        //Screw it! Let's just return the tempo at this point in time (might be a bit wrong).
        if (effect->resvd1)
        {
            module_renderer *pSndFile = ((CVstPlugin*)effect->resvd1)->GetSoundFile();
            if (pSndFile)
            {
                return (VstInt32)(pSndFile->GetCurrentBPM() * 10000);
            } else
            {
                return (VstInt32)(125 * 10000);
            }
        }
        return 125*10000;
    // parameters - DEPRECATED in VST 2.4
    case audioMasterGetNumAutomatableParameters:
        Log("VST plugin to host: Get Num Automatable Parameters\n");
        break;
    // Apparently, this one is broken in VST SDK anyway. - DEPRECATED in VST 2.4
    case audioMasterGetParameterQuantization:
        Log("VST plugin to host: Audio Master Get Parameter Quantization\n");
        break;
    // numInputs and/or numOutputs has changed
    case audioMasterIOChanged:
        Log("VST plugin to host: IOchanged\n");
        break;
    // plug needs idle calls (outside its editor window) - DEPRECATED in VST 2.4
    case audioMasterNeedIdle:
        if (effect && effect->resvd1)
        {
            CVstPlugin* pVstPlugin = (CVstPlugin*)effect->resvd1;
            pVstPlugin->m_bNeedIdle=true;
        }

        return 1;
    // index: width, value: height
    case audioMasterSizeWindow:
        if (effect->resvd1)
        {
            CVstPlugin *pVstPlugin = ((CVstPlugin*)effect->resvd1);
            CAbstractVstEditor *pVstEditor = pVstPlugin->GetEditor();
            if (pVstEditor)
            {
            CRect rcWnd, rcClient;
            pVstEditor->GetWindowRect(rcWnd);
            pVstEditor->GetClientRect(rcClient);

            rcWnd.right  = rcWnd.left + (rcWnd.right-rcWnd.left) - (rcClient.right-rcClient.left) + index;
            rcWnd.bottom = rcWnd.top + (rcWnd.bottom-rcWnd.top) - (rcClient.bottom-rcClient.top) + value;

            pVstEditor->SetWindowPos(NULL, rcWnd.left, rcWnd.top, rcWnd.Width(), rcWnd.Height(), SWP_NOMOVE | SWP_NOZORDER);
            }
        }
        Log("VST plugin to host: Size Window\n");
        return 1;
    case audioMasterGetSampleRate:
        return CMainFrame::GetMainFrame()->GetSampleRate();
    case audioMasterGetBlockSize:
        return modplug::mixgraph::MIX_BUFFER_SIZE;
    case audioMasterGetInputLatency:
        Log("VST plugin to host: Get Input Latency\n");
        break;
    case audioMasterGetOutputLatency:
        {
            auto &stream_settings = CMainFrame::GetMainFrame()->global_config.audio_settings();
            VstIntPtr latency =
                stream_settings.buffer_length * (CMainFrame::GetMainFrame()->GetSampleRate()/1000L);
            return latency;
        }
    // input pin in <value> (-1: first to come), returns cEffect* - DEPRECATED in VST 2.4
    case audioMasterGetPreviousPlug:
        Log("VST plugin to host: Get Previous Plug\n");
        break;
    // output pin in <value> (-1: first to come), returns cEffect* - DEPRECATED in VST 2.4
    case audioMasterGetNextPlug:
        Log("VST plugin to host: Get Next Plug\n");
        break;
    // realtime info
    // returns: 0: not supported, 1: replace, 2: accumulate - DEPRECATED in VST 2.4 (replace is default)
    case audioMasterWillReplaceOrAccumulate:
        return 1; //we replace.
    case audioMasterGetCurrentProcessLevel:
        //Log("VST plugin to host: Get Current Process Level\n");
        //TODO: Support offline processing
/*            if (CMainFrame::GetMainFrame()->IsRendering()) {
            return 4;   //Offline
        } else {
            return 2;   //Unknown.
        }
*/
        break;
    // returns 0: not supported, 1: off, 2:read, 3:write, 4:read/write
    case audioMasterGetAutomationState:
        // Not entirely sure what this means. We can write automation TO the plug.
        // Is that "read" in this context?
        //Log("VST plugin to host: Get Automation State\n");
        return kVstAutomationRead;

    case audioMasterOfflineStart:
        Log("VST plugin to host: Offlinestart\n");
        break;
    case audioMasterOfflineRead:
        Log("VST plugin to host: Offlineread\n");
        break;
    case audioMasterOfflineWrite:
        Log("VST plugin to host: Offlinewrite\n");
        break;
    case audioMasterOfflineGetCurrentPass:
        Log("VST plugin to host: OfflineGetcurrentpass\n");
        break;
    case audioMasterOfflineGetCurrentMetaPass:
        Log("VST plugin to host: OfflineGetCurrentMetapass\n");
        break;
    // for variable i/o, sample rate in <opt> - DEPRECATED in VST 2.4
    case audioMasterSetOutputSampleRate:
        Log("VST plugin to host: Set Output Sample Rate\n");
        break;
    // result in ret - DEPRECATED in VST 2.4
    case audioMasterGetOutputSpeakerArrangement:
        Log("VST plugin to host: Get Output Speaker Arrangement\n");
        break;
    case audioMasterGetVendorString:
        strcpy((char *) ptr, s_szHostVendorString);
        //strcpy((char*)ptr,"Steinberg");
        //return 0;
        return true;
    case audioMasterGetVendorVersion:
        return s_nHostVendorVersion;
        //return 7000;
    case audioMasterGetProductString:
        strcpy((char *) ptr, s_szHostProductString);
        //strcpy((char*)ptr,"Cubase VST");
        //return 0;
        return true;
    case audioMasterVendorSpecific:
        return 0;
    // void* in <ptr>, format not defined yet - DEPRECATED in VST 2.4
    case audioMasterSetIcon:
        Log("VST plugin to host: Set Icon\n");
        break;
    // string in ptr, see below
    case audioMasterCanDo:
        //Other possible Can Do strings are:
        //"receiveVstEvents",
        //"receiveVstMidiEvent",
        //"receiveVstTimeInfo",
        //"reportConnectionChanges",
        //"acceptIOChanges",
        //"asyncProcessing",
        //"offline",
        //"supportShell"
        //"editFile"
        //"startStopProcess"
        if ((strcmp((char*)ptr,"sendVstEvents") == 0 ||
                strcmp((char*)ptr,"sendVstMidiEvent") == 0 ||
                strcmp((char*)ptr,"sendVstTimeInfo") == 0 ||
                strcmp((char*)ptr,"supplyIdle") == 0 ||
                strcmp((char*)ptr,"sizeWindow") == 0 ||
                strcmp((char*)ptr,"openFileSelector") == 0 ||
                strcmp((char*)ptr,"closeFileSelector") == 0
            ))
            return HostCanDo;
        else
            return HostCanNotDo;
    //
    case audioMasterGetLanguage:
        return kVstLangEnglish;
    // returns platform specific ptr - DEPRECATED in VST 2.4
    case audioMasterOpenWindow:
        Log("VST plugin to host: Open Window\n");
        break;
    // close window, platform specific handle in <ptr> - DEPRECATED in VST 2.4
    case audioMasterCloseWindow:
        Log("VST plugin to host: Close Window\n");
        break;
    // get plug directory, FSSpec on MAC, else char*
    case audioMasterGetDirectory:
        //Log("VST plugin to host: Get Directory\n");
        return ToVstPtr(CMainFrame::GetDefaultDirectory(DIR_PLUGINS));
    // something has changed, update 'multi-fx' display
    case audioMasterUpdateDisplay:
        if (effect && effect->resvd1)
        {
//                    CVstPlugin *pVstPlugin = ((CVstPlugin*)effect->resvd1);
//            pVstPlugin->GetModDoc()->UpdateAllViews(NULL, HINT_MIXPLUGINS, NULL); //No Need.

/*                    CAbstractVstEditor *pVstEditor = pVstPlugin->GetEditor();
            if (pVstEditor && ::IsWindow(pVstEditor->m_hWnd))
            {
                //pVstEditor->SetupMenu();
            }
*/
//                    effect->dispatcher(effect, effEditIdle, 0, 0, NULL, 0.0f);
//                    effect->dispatcher(effect, effEditTop, 0,0, NULL, 0);
        }
        return 0;

    //---from here VST 2.1 extension opcodes------------------------------------------------------

    // begin of automation session (when mouse down), parameter index in <index>
    case audioMasterBeginEdit:
        Log("VST plugin to host: Begin Edit\n");
        break;
    // end of automation session (when mouse up),     parameter index in <index>
    case audioMasterEndEdit:
        Log("VST plugin to host: End Edit\n");
        break;
    // open a fileselector window with VstFileSelect* in <ptr>
    case audioMasterOpenFileSelector:
    //---from here VST 2.2 extension opcodes------------------------------------------------------

    // close a fileselector operation with VstFileSelect* in <ptr>: Must be always called after an open !
    case audioMasterCloseFileSelector:
        return VstFileSelector(opcode == audioMasterCloseFileSelector, (VstFileSelect *)ptr, effect);

    // open an editor for audio (defined by XML text in ptr) - DEPRECATED in VST 2.4
    case audioMasterEditFile:
        Log("VST plugin to host: Edit File\n");
        break;
    // get the native path of currently loading bank or project
    // (called from writeChunk) void* in <ptr> (char[2048], or sizeof(FSSpec)) - DEPRECATED in VST 2.4
    case audioMasterGetChunkFile:
        Log("VST plugin to host: Get Chunk File\n");
        break;

    //---from here VST 2.3 extension opcodes------------------------------------------------------

    // result a VstSpeakerArrangement in ret - DEPRECATED in VST 2.4
    case audioMasterGetInputSpeakerArrangement:
        Log("VST plugin to host: Get Input Speaker Arrangement\n");
        break;

    //---from here VST 2.4 extension opcodes------------------------------------------------------

    // Floating point processing precision
    case effSetProcessPrecision:
        return kVstProcessPrecision32;

    }

    // Unknown codes:

    return 0;
}
//end rewbs. VSTCompliance:


// Helper function for file selection dialog stuff.
VstIntPtr CVstPluginManager::VstFileSelector(const bool destructor, VstFileSelect *pFileSel, const AEffect *effect)
//-----------------------------------------------------------------------------------------------------------------
{
    if(pFileSel == nullptr)
    {
        return 0;
    }

    if(!destructor)
    {
        pFileSel->nbReturnPath = 0;
        pFileSel->reserved = 0;

        if(pFileSel->command != kVstDirectorySelect)
        {
            // Plugin wants to load or save a file.
            std::string extensions, workingDir;
            for(size_t i = 0; i < pFileSel->nbFileTypes; i++)
            {
                VstFileType *pType = &(pFileSel->fileTypes[i]);
                extensions += pType->name;
                extensions += "|";
#if (defined(WIN32) || (defined(WINDOWS) && WINDOWS == 1))
                extensions += "*.";
                extensions += pType->dosType;
#elif defined(MAC) && MAC == 1
                extensions += "*";
                extensions += pType->macType;
#elif defined(UNIX) && UNIX == 1
                extensions += "*.";
                extensions += pType->unixType;
#else
#error Platform-specific code missing
#endif
                extensions += "|";
            }

            if(pFileSel->initialPath != nullptr)
            {
                workingDir = pFileSel->initialPath;
            } else
            {
                // Plugins are probably looking for presets...?
                workingDir = ""; //CMainFrame::GetWorkingDirectory(DIR_PLUGINPRESETS);
            }

            FileDlgResult files = CTrackApp::ShowOpenSaveFileDialog(
                (pFileSel->command == kVstFileSave ? false : true),
                "", "", extensions, workingDir,
                (pFileSel->command == kVstMultipleFilesLoad ? true : false)
                );

            if(files.abort)
            {
                return 0;
            }

            if(pFileSel->command == kVstMultipleFilesLoad)
            {
                // Multiple paths
                pFileSel->nbReturnPath = files.filenames.size();
                pFileSel->returnMultiplePaths = new char *[pFileSel->nbReturnPath];
                for(size_t i = 0; i < files.filenames.size(); i++)
                {
                    char *fname = new char[files.filenames[i].length() + 1];
                    strcpy(fname, files.filenames[i].c_str());
                    pFileSel->returnMultiplePaths[i] = fname;
                }
                return 1;
            } else
            {
                // Single path

                // VOPM doesn't initialize required information properly (it doesn't memset the struct to 0)...
                if(!memcmp(&(effect->uniqueID), "MPOV", 4))
                {
                    pFileSel->sizeReturnPath = _MAX_PATH;
                }

                if(pFileSel->returnPath == nullptr || pFileSel->sizeReturnPath == 0)
                {

                    // Provide some memory for the return path.
                    pFileSel->sizeReturnPath = files.first_file.length() + 1;
                    pFileSel->returnPath = new char[pFileSel->sizeReturnPath];
                    if(pFileSel->returnPath == nullptr)
                    {
                        return 0;
                    }
                    pFileSel->returnPath[pFileSel->sizeReturnPath - 1] = '\0';
                    pFileSel->reserved = 1;
                } else
                {
                    pFileSel->reserved = 0;
                }
                strncpy(pFileSel->returnPath, files.first_file.c_str(), pFileSel->sizeReturnPath - 1);
                pFileSel->nbReturnPath = 1;
                pFileSel->returnMultiplePaths = nullptr;
            }
            return 1;

        } else
        {
            // Plugin wants a directory

            char szInitPath[_MAX_PATH];
            MemsetZero(szInitPath);
            if(pFileSel->initialPath)
            {
                strncpy(szInitPath, pFileSel->initialPath, _MAX_PATH - 1);
            }

            char szBuffer[_MAX_PATH];
            MemsetZero(szBuffer);

            BROWSEINFO bi;
            MemsetZero(bi);
            bi.hwndOwner = CMainFrame::GetMainFrame()->m_hWnd;
            bi.lpszTitle = pFileSel->title;
            bi.pszDisplayName = szInitPath;
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;
            LPITEMIDLIST pid = SHBrowseForFolder(&bi);
            if(pid != NULL && SHGetPathFromIDList(pid, szBuffer))
            {
                if(!memcmp(&(effect->uniqueID), "rTSV", 4) && pFileSel->returnPath != nullptr && pFileSel->sizeReturnPath == 0)
                {
                    // old versions of reViSiT (which still relied on the host's file selection code) seem to be dodgy.
                    // They report a path size of 0, but when using an own buffer, they will crash.
                    // So we'll just assume that reViSiT can handle long enough (_MAX_PATH) paths here.
                    pFileSel->sizeReturnPath = strlen(szBuffer) + 1;
                    pFileSel->returnPath[pFileSel->sizeReturnPath - 1] = '\0';
                }
                if(pFileSel->returnPath == nullptr || pFileSel->sizeReturnPath == 0)
                {
                    // Provide some memory for the return path.
                    pFileSel->sizeReturnPath = strlen(szBuffer) + 1;
                    pFileSel->returnPath = new char[pFileSel->sizeReturnPath];
                    if(pFileSel->returnPath == nullptr)
                    {
                        return 0;
                    }
                    pFileSel->returnPath[pFileSel->sizeReturnPath - 1] = '\0';
                    pFileSel->reserved = 1;
                } else
                {
                    pFileSel->reserved = 0;
                }
                strncpy(pFileSel->returnPath, szBuffer, pFileSel->sizeReturnPath - 1);
                pFileSel->nbReturnPath = 1;
                return 1;
            } else
            {
                return 0;
            }
        }
    } else
    {
        // Close file selector - delete allocated strings.
        if(pFileSel->command == kVstMultipleFilesLoad && pFileSel->returnMultiplePaths != nullptr)
        {
            for(size_t i = 0; i < pFileSel->nbReturnPath; i++)
            {
                if(pFileSel->returnMultiplePaths[i] != nullptr)
                {
                    delete[] pFileSel->returnMultiplePaths[i];
                }
            }
            delete[] pFileSel->returnMultiplePaths;
        } else
        {
            if(pFileSel->reserved == 1 && pFileSel->returnPath != nullptr)
            {
                delete[] pFileSel->returnPath;
                pFileSel->returnPath = nullptr;
            }
        }
        return 1;
    }
}


void CVstPluginManager::ReportPlugException(LPCSTR format,...)
//------------------------------------------------------------
{
    CHAR cBuf[1024];
    va_list va;
    va_start(va, format);
    wvsprintf(cBuf, format, va);
    AfxMessageBox(cBuf);
#ifdef VST_LOG
    Log(cBuf);
#endif

    //Stop song - TODO: figure out why this causes a kernel hang from IASIO->release();
/*    END_CRITICAL();
    CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
        if (pMainFrm) pMainFrm->StopMod();
*/
    va_end(va);
}

/////////////////////////////////////////////////////////////////////////////////
//
// CSelectPluginDlg
//

BEGIN_MESSAGE_MAP(CSelectPluginDlg, CDialog)
    ON_NOTIFY(TVN_SELCHANGED,     IDC_TREE1, OnSelChanged)
    ON_NOTIFY(NM_DBLCLK,             IDC_TREE1, OnSelDblClk)
    ON_COMMAND(IDC_BUTTON1,             OnAddPlugin)
    ON_COMMAND(IDC_BUTTON2,             OnRemovePlugin)
    ON_EN_CHANGE(IDC_NAMEFILTER, OnNameFilterChanged)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
END_MESSAGE_MAP()

void CSelectPluginDlg::DoDataExchange(CDataExchange* pDX)
//-------------------------------------------------------
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_TREE1, m_treePlugins);
    DDX_Text(pDX, IDC_NAMEFILTER, m_sNameFilter);
}


CSelectPluginDlg::CSelectPluginDlg(CModDoc *pModDoc, int nPlugSlot, CWnd *parent):CDialog(IDD_SELECTMIXPLUGIN, parent)
//----------------------------------------------------------------------------------------------------------
{
    m_pPlugin = NULL;
    m_pModDoc = pModDoc;
    m_nPlugSlot = nPlugSlot;

     if (m_pModDoc) {
        module_renderer* pSndFile = pModDoc->GetSoundFile();
        if (pSndFile && (0<=m_nPlugSlot && m_nPlugSlot<MAX_MIXPLUGINS)) {
            m_pPlugin = &pSndFile->m_MixPlugins[m_nPlugSlot];
        }
     }
}


BOOL CSelectPluginDlg::OnInitDialog()
//-----------------------------------
{
    uint32_t dwRemove = TVS_EDITLABELS|TVS_SINGLEEXPAND;
    uint32_t dwAdd = TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS;

    CDialog::OnInitDialog();
    m_treePlugins.ModifyStyle(dwRemove, dwAdd);
    m_treePlugins.SetImageList(CMainFrame::GetMainFrame()->GetImageList(), TVSIL_NORMAL);

    if (m_pPlugin) {
        CString targetSlot;
        targetSlot.Format("Put in FX%02d", m_nPlugSlot+1);
        SetDlgItemText(IDOK, targetSlot);
        ::EnableWindow(::GetDlgItem(m_hWnd, IDOK), TRUE);
    } else {
        ::EnableWindow(::GetDlgItem(m_hWnd, IDOK), FALSE);
    }

    MoveWindow(CMainFrame::GetMainFrame()->gnPlugWindowX,
               CMainFrame::GetMainFrame()->gnPlugWindowY,
               CMainFrame::GetMainFrame()->gnPlugWindowWidth,
               CMainFrame::GetMainFrame()->gnPlugWindowHeight);

    UpdatePluginsList();
    OnSelChanged(NULL, NULL);
    return TRUE;
}


typedef struct _PROBLEMATIC_PLUG
{
    uint32_t id1;
    uint32_t id2;
    uint32_t version;
    LPCSTR name;
    LPCSTR problem;
} _PROBLEMATIC_PLUG, *PPROBLEMATIC_PLUG;

//TODO: Check whether the list is still valid.
#define NUM_PROBLEMPLUGS 2
static _PROBLEMATIC_PLUG gProblemPlugs[NUM_PROBLEMPLUGS] =
{
    {kEffectMagic, CCONST('N', 'i', '4', 'S'), 1, "Native Instruments B4", "*  v1.1.1 hangs on playback. Do not proceed unless you have v1.1.5.  *"},
    {kEffectMagic, CCONST('m', 'd', 'a', 'C'), 1, "MDA Degrade", "*  This plugin can cause OpenMPT to behave erratically.\r\nYou should try SoundHack's Decimate, ConcreteFX's Lowbit or Subtek's LoFi Plus instead.  *"},
};

bool CSelectPluginDlg::VerifyPlug(PVSTPLUGINLIB plug)
//---------------------------------------------------
{
    CString s;
    for (int p=0; p<NUM_PROBLEMPLUGS; p++)
    {
        if ( (gProblemPlugs[p].id2 == plug->dwPluginId2)
            /*&& (gProblemPlugs[p].id1 == plug->dwPluginId1)*/)
        {
            s.Format("WARNING: This plugin has been identified as %s,\r\nwhich is known to have the following problem with OpenMPT:\r\n\r\n%s\r\n\r\nWould you like to continue to load this plugin?", gProblemPlugs[p].name, gProblemPlugs[p].problem);
            return (AfxMessageBox(s, MB_YESNO)  == IDYES);
        }
    }

    return true;
}

VOID CSelectPluginDlg::OnOK()
//---------------------------
{
// -> CODE#0002
// -> DESC="list box to choose VST plugin presets (programs)"
    if(m_pPlugin==NULL) { CDialog::OnOK(); return; }
// -! NEW_FEATURE#0002

    BOOL bChanged = FALSE;
    CVstPluginManager *pManager = theApp.GetPluginManager();
    PVSTPLUGINLIB pNewPlug = (PVSTPLUGINLIB)m_treePlugins.GetItemData(m_treePlugins.GetSelectedItem());
    PVSTPLUGINLIB pFactory = NULL;
    CVstPlugin *pCurrentPlugin = NULL;
    if (m_pPlugin) pCurrentPlugin = (CVstPlugin *)m_pPlugin->pMixPlugin;
    if ((pManager) && (pManager->IsValidPlugin(pNewPlug))) pFactory = pNewPlug;
    // Plugin selected
    if (pFactory)
    {
        if (!VerifyPlug(pFactory))
        {
            CDialog::OnCancel();
            return;
        }

        if ((!pCurrentPlugin) || (pCurrentPlugin->GetPluginFactory() != pFactory))
        {
            BEGIN_CRITICAL();
            if (pCurrentPlugin) pCurrentPlugin->Release();
            // Just in case...
            m_pPlugin->pMixPlugin = NULL;
            m_pPlugin->pMixState = NULL;
            // Remove old state
            m_pPlugin->nPluginDataSize = 0;
            if (m_pPlugin->pPluginData) delete[] m_pPlugin->pPluginData;
            m_pPlugin->pPluginData = NULL;
            // Initialize plugin info
            MemsetZero(m_pPlugin->Info);
            m_pPlugin->Info.dwPluginId1 = pFactory->dwPluginId1;
            m_pPlugin->Info.dwPluginId2 = pFactory->dwPluginId2;

            switch(m_pPlugin->Info.dwPluginId2)
            {
            // Enable drymix by default for these known plugins
            case CCONST('S', 'c', 'o', 'p'):
                m_pPlugin->Info.dwInputRouting |= MIXPLUG_INPUTF_WETMIX;
                break;
            }

            lstrcpyn(m_pPlugin->Info.szName, pFactory->szLibraryName, 32);
            lstrcpyn(m_pPlugin->Info.szLibraryName, pFactory->szLibraryName, 64);
            END_CRITICAL();
            // Now, create the new plugin
            if (pManager)
            {
                pManager->CreateMixPlugin(m_pPlugin, (m_pModDoc) ? m_pModDoc->GetSoundFile() : 0);
                if (m_pPlugin->pMixPlugin)
                {
                    CHAR s[128];
                    CVstPlugin *p = (CVstPlugin *)m_pPlugin->pMixPlugin;
                    s[0] = 0;
                    if ((p->GetDefaultEffectName(s)) && (s[0]))
                    {
                        s[31] = 0;
                        lstrcpyn(m_pPlugin->Info.szName, s, 32);
                    }
                }
            }
            bChanged = TRUE;
        }
    } else
    // No effect
    {
        BEGIN_CRITICAL();
        if (pCurrentPlugin)
        {
            pCurrentPlugin->Release();
            bChanged = TRUE;
        }
        // Just in case...
        m_pPlugin->pMixPlugin = NULL;
        m_pPlugin->pMixState = NULL;
        // Remove old state
        m_pPlugin->nPluginDataSize = 0;
        if (m_pPlugin->pPluginData) delete[] m_pPlugin->pPluginData;
        m_pPlugin->pPluginData = NULL;
        // Clear plugin info
        MemsetZero(m_pPlugin->Info);
        END_CRITICAL();
    }

    //remember window size:
    RECT rect;
    GetWindowRect(&rect);
    CMainFrame::GetMainFrame()->gnPlugWindowX = rect.left;
    CMainFrame::GetMainFrame()->gnPlugWindowY = rect.top;
    CMainFrame::GetMainFrame()->gnPlugWindowWidth  = rect.right - rect.left;
    CMainFrame::GetMainFrame()->gnPlugWindowHeight = rect.bottom - rect.top;

    if (bChanged) {
        CMainFrame::GetMainFrame()->gnPlugWindowLast = m_pPlugin->Info.dwPluginId2;
        CDialog::OnOK();
    }
    else {
        CDialog::OnCancel();
    }
}

VOID CSelectPluginDlg::OnCancel()
//---------------------------
{
    //remember window size:
    RECT rect;
    GetWindowRect(&rect);
    CMainFrame::GetMainFrame()->gnPlugWindowX = rect.left;
    CMainFrame::GetMainFrame()->gnPlugWindowY = rect.top;
    CMainFrame::GetMainFrame()->gnPlugWindowWidth  = rect.right - rect.left;
    CMainFrame::GetMainFrame()->gnPlugWindowHeight = rect.bottom - rect.top;

    CDialog::OnCancel();
}

void CSelectPluginDlg::OnNameFilterChanged()
//-------------------------------------
{
    GetDlgItem(IDC_NAMEFILTER)->GetWindowText(m_sNameFilter);
    m_sNameFilter = m_sNameFilter.MakeLower();
    UpdatePluginsList();
}

VOID CSelectPluginDlg::UpdatePluginsList(uint32_t forceSelect/*=0*/)
//---------------------------------------------------------------
{
    CVstPluginManager *pManager = theApp.GetPluginManager();
    HTREEITEM cursel, hDmo, hVst, hSynth;

    m_treePlugins.SetRedraw(FALSE);
    m_treePlugins.DeleteAllItems();

    hSynth = AddTreeItem("VST Instruments", IMAGE_FOLDER, false);
    hDmo = AddTreeItem("DirectX Media Audio Effects", IMAGE_FOLDER, false);
    hVst = AddTreeItem("VST Audio Effects", IMAGE_FOLDER, false);
    cursel = AddTreeItem("No plugin (empty slot)", IMAGE_NOPLUGIN, false);

    if (pManager)
    {
        PVSTPLUGINLIB pCurrent = NULL;
        PVSTPLUGINLIB p = pManager->GetFirstPlugin();
        while (p)
        {
            // Apply name filter
            if (m_sNameFilter != "") {
                CString displayName = p->szLibraryName;
                if (displayName.MakeLower().Find(m_sNameFilter) == -1) {
                    p = p->pNext;
                    continue;
                }
            }

            HTREEITEM hParent;
            hParent = (p->bIsInstrument) ? hSynth : hVst;

            HTREEITEM h = AddTreeItem(p->szLibraryName, p->bIsInstrument ? IMAGE_PLUGININSTRUMENT : IMAGE_EFFECTPLUGIN, true, hParent, (LPARAM)p);

            //If filter is active, expand nodes.
            if (m_sNameFilter != "") m_treePlugins.EnsureVisible(h);

            //Which plugin should be selected?
            if (m_pPlugin)
            {

                //forced selection (e.g. just after add plugin)
                if (forceSelect != 0)
                {
                    if (p->dwPluginId2 == forceSelect)
                    {
                        pCurrent = p;
                    }
                }

                //Current slot's plugin
                else if (m_pPlugin->pMixPlugin)
                {
                    CVstPlugin *pVstPlug = (CVstPlugin *)m_pPlugin->pMixPlugin;
                    if (pVstPlug->GetPluginFactory() == p) pCurrent = p;
                }

                //Plugin with matching ID to current slot's plug
                else if (/* (!pCurrent) && */ m_pPlugin->Info.dwPluginId1 !=0 || m_pPlugin->Info.dwPluginId2 != 0)
                {
                    if ((p->dwPluginId1 == m_pPlugin->Info.dwPluginId1)
                     && (p->dwPluginId2 == m_pPlugin->Info.dwPluginId2)) {
                        pCurrent = p;
                    }
                }

                //Last selected plugin
                else
                {
                    if (p->dwPluginId2 == CMainFrame::GetMainFrame()->gnPlugWindowLast) {
                        pCurrent = p;
                    }
                }
            }
            if (pCurrent == p) cursel = h;
            p = p->pNext;
        }
    }
    m_treePlugins.SetRedraw(TRUE);
    if (cursel) {
        m_treePlugins.SelectItem(cursel);
        m_treePlugins.SetItemState(cursel, TVIS_BOLD, TVIS_BOLD);
        m_treePlugins.EnsureVisible(cursel);
    }
}

HTREEITEM CSelectPluginDlg::AddTreeItem(LPSTR szTitle, int iImage, bool bSort, HTREEITEM hParent, LPARAM lParam)
//--------------------------------------------------------------------------------------------------------------
{
    TVINSERTSTRUCT tvis;
    MemsetZero(tvis);

    tvis.hParent = hParent;
    tvis.hInsertAfter = (bSort) ? TVI_SORT : TVI_FIRST;
    tvis.item.mask = TVIF_IMAGE | TVIF_PARAM | TVIF_TEXT | TVIF_SELECTEDIMAGE | TVIF_TEXT;
    tvis.item.pszText = szTitle;
    tvis.item.iImage = tvis.item.iSelectedImage = iImage;
    tvis.item.lParam = lParam;
    return m_treePlugins.InsertItem(&tvis);
}


VOID CSelectPluginDlg::OnSelDblClk(NMHDR *, LRESULT *result)
//----------------------------------------------------------
{
// -> CODE#0002
// -> DESC="list box to choose VST plugin presets (programs)"
    if(m_pPlugin == NULL) return;
// -! NEW_FEATURE#0002

    HTREEITEM hSel = m_treePlugins.GetSelectedItem();
    int nImage, nSelectedImage;
    m_treePlugins.GetItemImage(hSel, nImage, nSelectedImage);

    if ((hSel) && (nImage != IMAGE_FOLDER)) OnOK();
    if (result) *result = 0;
}


VOID CSelectPluginDlg::OnSelChanged(NMHDR *, LRESULT *result)
//-----------------------------------------------------------
{
    CVstPluginManager *pManager = theApp.GetPluginManager();
    PVSTPLUGINLIB pPlug = (PVSTPLUGINLIB)m_treePlugins.GetItemData(m_treePlugins.GetSelectedItem());
    if ((pManager) && (pManager->IsValidPlugin(pPlug)))
    {
        SetDlgItemText(IDC_TEXT_CURRENT_VSTPLUG, pPlug->szDllPath);
    } else
    {
        SetDlgItemText(IDC_TEXT_CURRENT_VSTPLUG, "");
    }
    if (result) *result = 0;
}

VOID CSelectPluginDlg::OnAddPlugin()
//----------------------------------
{
    FileDlgResult files = CTrackApp::ShowOpenSaveFileDialog(true, "dll", "",
        "VST Plugins (*.dll)|*.dll||",
        CMainFrame::GetWorkingDirectory(DIR_PLUGINS),
        true);
    if(files.abort) return;

    CMainFrame::SetWorkingDirectory(files.workingDirectory.c_str(), DIR_PLUGINS, true);

    CVstPluginManager *pManager = theApp.GetPluginManager();
    bool bOk = false;

    PVSTPLUGINLIB plugLib = NULL;
    for(size_t counter = 0; counter < files.filenames.size(); counter++)
    {

        CString sFilename = files.filenames[counter].c_str();

        if (pManager) {
            plugLib = pManager->AddPlugin(sFilename, FALSE);
            if (plugLib) bOk = true;
        }
    }
    if (bOk) {
        UpdatePluginsList(plugLib->dwPluginId2);    //force selection to last added plug.
    } else {
        MessageBox("At least one selected file was not a valid VST-Plugin", NULL, MB_ICONERROR|MB_OK);
    }
}


VOID CSelectPluginDlg::OnRemovePlugin()
//-------------------------------------
{
    CVstPluginManager *pManager = theApp.GetPluginManager();
    PVSTPLUGINLIB pPlug = (PVSTPLUGINLIB)m_treePlugins.GetItemData(m_treePlugins.GetSelectedItem());
    if ((pManager) && (pPlug))
    {
        pManager->RemovePlugin(pPlug);
        UpdatePluginsList();
    }
}


void CSelectPluginDlg::OnSize(UINT nType, int cx, int cy)
//-------------------------------------------------------
{
    CDialog::OnSize(nType, cx, cy);

    if (m_treePlugins) {
        m_treePlugins.MoveWindow(8, 36, cx - 104, cy - 63, FALSE);

        ::MoveWindow(GetDlgItem(IDC_STATIC_VSTNAMEFILTER)->m_hWnd, 8, 11, 40, 21, FALSE);
        ::MoveWindow(GetDlgItem(IDC_NAMEFILTER)->m_hWnd, 40, 8, cx - 136, 21, FALSE);

        ::MoveWindow(GetDlgItem(IDC_TEXT_CURRENT_VSTPLUG)->m_hWnd, 8, cy - 20, cx - 22, 25, FALSE);
        ::MoveWindow(GetDlgItem(IDOK)->m_hWnd,                    cx-85,        8,    75, 23, FALSE);
        ::MoveWindow(GetDlgItem(IDCANCEL)->m_hWnd,            cx-85,        39,    75, 23, FALSE);
        ::MoveWindow(GetDlgItem(IDC_BUTTON1)->m_hWnd ,    cx-85,        cy-80, 75, 23, FALSE);
        ::MoveWindow(GetDlgItem(IDC_BUTTON2)->m_hWnd,    cx-85,        cy-52, 75, 23, FALSE);
        Invalidate();
    }
}

void CSelectPluginDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    lpMMI->ptMinTrackSize.x=300;
    lpMMI->ptMinTrackSize.y=270;
    CDialog::OnGetMinMaxInfo(lpMMI);
}



//////////////////////////////////////////////////////////////////////////////
//
// CVstPlugin
//

CVstPlugin::CVstPlugin(HMODULE hLibrary, PVSTPLUGINLIB pFactory, PSNDMIXPLUGIN pMixStruct, AEffect *pEffect)
//----------------------------------------------------------------------------------------------------------
{
    m_hLibrary = hLibrary;
    m_nRefCount = 1;
    m_pPrev = NULL;
    m_pNext = NULL;
    m_pFactory = pFactory;
    m_pMixStruct = pMixStruct;
    m_pEffect = pEffect;
    m_pInputs = NULL;
    m_pOutputs = NULL;
    m_pEditor = NULL;
    m_nInputs = m_nOutputs = 0;
    m_nEditorX = m_nEditorY = -1;
    m_pEvList = NULL;
    m_pModDoc = NULL; //rewbs.plugDocAware
    m_nPreviousMidiChan = -1; //rewbs.VSTCompliance
    m_pProcessFP = NULL;

    // Insert ourselves in the beginning of the list
    if (m_pFactory)
    {
        m_pNext = m_pFactory->pPluginsList;
        if (m_pFactory->pPluginsList)
        {
            m_pFactory->pPluginsList->m_pPrev = this;
        }
        m_pFactory->pPluginsList = this;
    }
    m_MixState.dwFlags = 0;
    m_MixState.nVolDecayL = 0;
    m_MixState.nVolDecayR = 0;
    m_MixState.pMixBuffer = (int *)((((uint32_t)m_MixBuffer)+7)&~7);
    m_MixState.pOutBufferL = (float *)((((uint32_t)&m_FloatBuffer[0])+7)&~7);
    m_MixState.pOutBufferR = (float *)((((uint32_t)&m_FloatBuffer[modplug::mixgraph::MIX_BUFFER_SIZE])+7)&~7);
    //XXXih: awful!
    memset(dummyBuffer_, 0, sizeof(float) * (modplug::mixgraph::MIX_BUFFER_SIZE + 2));

    //rewbs.dryRatio: we now initialise this in CVstPlugin::Initialize().
    //m_pTempBuffer = (float *)((((uint32_t)&m_FloatBuffer[modplug::mixgraph::MIX_BUFFER_SIZE*2])+7)&~7);

    m_bSongPlaying = false; //rewbs.VSTCompliance
    m_bPlugResumed = false;
    m_bModified = false;
    m_dwTimeAtStartOfProcess=0;
    m_nSampleRate = -1; //rewbs.VSTCompliance: gets set on Resume()
    memset(m_MidiCh, 0, sizeof(m_MidiCh));

    for (int ch=0; ch<16; ch++) {
        m_nMidiPitchBendPos[ch]=MIDI_PitchBend_Centre; //centre pitch bend on all channels
    }

    processCalled = CreateEvent(NULL,FALSE,FALSE,NULL);
}


void CVstPlugin::Initialize(module_renderer* pSndFile)
//-----------------------------------------------
{
    if (!m_pEvList)
    {
        m_pEvList = (VstEvents *)new char[sizeof(VstEvents)+sizeof(VstEvent*)*VSTEVENT_QUEUE_LEN];
    }
    if (m_pEvList)
    {
        m_pEvList->numEvents = 0;
        m_pEvList->reserved = 0;
    }
    m_bNeedIdle=false;
    m_bRecordAutomation=false;
    m_bPassKeypressesToPlug=false;

    memset(m_MidiCh, 0, sizeof(m_MidiCh));

    //rewbs.VSTcompliance
    //Store a pointer so we can get the CVstPlugin object from the basic VST effect object.
    m_pEffect->resvd1=ToVstPtr(this);
    //rewbs.plugDocAware
    m_pSndFile = pSndFile;
    m_pModDoc = pSndFile->GetpModDoc();
    m_nSlot = FindSlot();
    //end rewbs.plugDocAware

    Dispatch(effOpen, 0, 0, NULL, 0);
    // VST 2.0 plugins return 2 here, VST 2.4 plugins return 2400... Great!
    m_bIsVst2 = (CVstPlugin::Dispatch(effGetVstVersion, 0,0, NULL, 0) >= 2) ? true : false;
    if (m_bIsVst2)
    {
        // Set VST speaker in/out setup to Stereo. Required for some plugins (possibly all VST 2.4+ plugins?)
        // All this might get more interesting when adding sidechaining support...
        VstSpeakerArrangement sa;
        MemsetZero(sa);
        sa.numChannels = 2;
        sa.type = kSpeakerArrStereo;
        for(int i = 0; i < CountOf(sa.speakers); i++)
        {
            sa.speakers[i].azimuth = 0.0f;
            sa.speakers[i].elevation = 0.0f;
            sa.speakers[i].radius = 0.0f;
            sa.speakers[i].reserved = 0.0f;
            // For now, only left and right speaker are used.
            switch(i)
            {
            case 0:
                sa.speakers[i].type = kSpeakerL;
                vst_strncpy(sa.speakers[i].name, "Left", kVstMaxNameLen - 1);
                break;
            case 1:
                sa.speakers[i].type = kSpeakerR;
                vst_strncpy(sa.speakers[i].name, "Right", kVstMaxNameLen - 1);
                break;
            default:
                sa.speakers[i].type = kSpeakerUndefined;
                break;
            }
        }
        // For now, input setup = output setup.
        Dispatch(effSetSpeakerArrangement, 0, ToVstPtr(&sa), &sa, 0.0f);

        // Dummy pin properties collection.
        // We don't use them but some plugs might do inits in here.
        VstPinProperties tempPinProperties;
        Dispatch(effGetInputProperties, 0, 0, &tempPinProperties, 0);
        Dispatch(effGetOutputProperties, 0, 0, &tempPinProperties, 0);

        Dispatch(effConnectInput, 0, 1, NULL, 0);
        if (m_pEffect->numInputs > 1) Dispatch(effConnectInput, 1, 1, NULL, 0.0f);
        Dispatch(effConnectOutput, 0, 1, NULL, 0);
        if (m_pEffect->numOutputs > 1) Dispatch(effConnectOutput, 1, 1, NULL, 0.0f);
        //rewbs.VSTCompliance: disable all inputs and outputs beyond stereo left and right:
        for (int i=2; i<m_pEffect->numInputs; i++)
            Dispatch(effConnectInput, i, 0, NULL, 0.0f);
        for (int i=2; i<m_pEffect->numOutputs; i++)
            Dispatch(effConnectOutput, i, 0, NULL, 0.0f);
        //end rewbs.VSTCompliance

    }

    m_nSampleRate=module_renderer::deprecated_global_mixing_freq;
    Dispatch(effSetSampleRate, 0, 0, NULL, module_renderer::deprecated_global_mixing_freq);
    Dispatch(effSetBlockSize, 0, modplug::mixgraph::MIX_BUFFER_SIZE, NULL, 0.0f);
    if (m_pEffect->numPrograms > 0)    {
        Dispatch(effSetProgram, 0, 0, NULL, 0);
    }
    Dispatch(effMainsChanged, 0, 1, NULL, 0.0f);

    m_nInputs = m_pEffect->numInputs;
    m_nOutputs = m_pEffect->numOutputs;

    //32 is the maximum output count due to the size of m_FloatBuffer.
    //TODO: How to handle properly plugs with numOutputs > 32?
    if(m_nOutputs > 32)
    {
        m_nOutputs = 32;
        CString str;
        str.Format("Plugin has unsupported number(=%d) of outputs; plugin may malfunction.", m_pEffect->numOutputs);
        MessageBox(NULL, str, "Warning", MB_ICONWARNING);
    }

    //input pointer array size must be >=2 for now - the input buffer assignment might write to non allocated mem. otherwise
    m_pInputs = (m_nInputs >= 2) ? new (float *[m_nInputs]) : new (float*[2]);
    m_pInputs[0] = m_MixState.pOutBufferL;
    m_pInputs[1] = m_MixState.pOutBufferR;
    for (size_t i = 2; i < m_nInputs; ++i) {
        m_pInputs[i] = dummyBuffer_;
    }

    m_pOutputs = new (float *[m_nOutputs]);
    m_pTempBuffer = new (float *[m_nOutputs]);    //rewbs.dryRatio

    for (UINT iOut=0; iOut<m_nOutputs; iOut++)
    {
        m_pTempBuffer[iOut]=(float *)((((DWORD_PTR)&m_FloatBuffer[modplug::mixgraph::MIX_BUFFER_SIZE*(2+iOut)])+7)&~7); //rewbs.dryRatio
    }

#ifdef VST_LOG
    Log("%s: vst ver %d.0, flags=%04X, %d programs, %d parameters\n",
        m_pFactory->szLibraryName, (m_bIsVst2) ? 2 : 1, m_pEffect->flags,
        m_pEffect->numPrograms, m_pEffect->numParams);
#endif
    // Update Mix structure
    if (m_pMixStruct)
    {
        m_pMixStruct->pMixPlugin = this;
        m_pMixStruct->pMixState = &m_MixState;
    }

    //rewbs.VSTcompliance
    m_bIsInstrument = isInstrument();
    RecalculateGain();
    m_pProcessFP = (m_pEffect->flags & effFlagsCanReplacing) ?  m_pEffect->processReplacing : m_pEffect->process;

 // issue samplerate again here, cos some plugs like it before the block size, other like it right at the end.
    Dispatch(effSetSampleRate, 0, 0, NULL, module_renderer::deprecated_global_mixing_freq);

}


CVstPlugin::~CVstPlugin()
//-----------------------
{
#ifdef VST_LOG
    Log("~CVstPlugin: m_nRefCount=%d\n", m_nRefCount);
#endif
    // First thing to do, if we don't want to hang in a loop
    if ((m_pFactory) && (m_pFactory->pPluginsList == this)) m_pFactory->pPluginsList = m_pNext;
    if (m_pMixStruct)
    {
        m_pMixStruct->pMixPlugin = NULL;
        m_pMixStruct->pMixState = NULL;
        m_pMixStruct = NULL;
    }
    if (m_pNext) m_pNext->m_pPrev = m_pPrev;
    if (m_pPrev) m_pPrev->m_pNext = m_pNext;
    m_pPrev = NULL;
    m_pNext = NULL;
    if (m_pEditor)
    {
        if (m_pEditor->m_hWnd) m_pEditor->OnClose();
        if ((volatile void *)m_pEditor) delete m_pEditor;
        m_pEditor = NULL;
    }
    if (m_bIsVst2)
    {
        Dispatch(effConnectInput, 0, 0, NULL, 0);
        if (m_pEffect->numInputs > 1) Dispatch(effConnectInput, 1, 0, NULL, 0);
        Dispatch(effConnectOutput, 0, 0, NULL, 0);
        if (m_pEffect->numOutputs > 1) Dispatch(effConnectOutput, 1, 0, NULL, 0);
    }
    CVstPlugin::Dispatch(effMainsChanged, 0,0, NULL, 0);
    CVstPlugin::Dispatch(effClose, 0, 0, NULL, 0);
    m_pFactory = NULL;
    if (m_hLibrary)
    {
        FreeLibrary(m_hLibrary);
        m_hLibrary = NULL;
    }

    delete[] m_pTempBuffer;
    m_pTempBuffer = NULL;

    delete[] m_pInputs;
    m_pInputs = NULL;

    delete[] m_pOutputs;
    m_pOutputs = NULL;

    delete[] (char *)m_pEvList;
    m_pEvList = NULL;

    CloseHandle(processCalled);
}


int CVstPlugin::Release()
//-----------------------
{
    if (!(--m_nRefCount))
    {
        try {
            delete this;
        } catch (...) {
            CVstPluginManager::ReportPlugException("Exception while destroying plugin!\n");
        }

        return 0;
    }
    return m_nRefCount;
}


VOID CVstPlugin::GetPluginType(LPSTR pszType)
//-------------------------------------------
{
    pszType[0] = 0;
    if (m_pEffect)
    {
        if (m_pEffect->numInputs < 1) strcpy(pszType, "No input"); else
        if (m_pEffect->numInputs == 1) strcpy(pszType, "Mono-In"); else
        strcpy(pszType, "Stereo-In");
        strcat(pszType, ", ");
        if (m_pEffect->numOutputs < 1) strcat(pszType, "No output"); else
        if (m_pEffect->numInputs == 1) strcat(pszType, "Mono-Out"); else
        strcat(pszType, "Stereo-Out");
    }
}


BOOL CVstPlugin::HasEditor()
//--------------------------
{
    if ((m_pEffect) && (m_pEffect->flags & effFlagsHasEditor))
    {
        return TRUE;
    }
    return FALSE;
}

//rewbs.VSTcompliance: changed from BOOL to long
long CVstPlugin::GetNumPrograms()
//-------------------------------
{
    if ((m_pEffect) && (m_pEffect->numPrograms > 0))
    {
        return m_pEffect->numPrograms;
    }
    return 0;
}

//rewbs.VSTcompliance: changed from BOOL to long
PlugParamIndex CVstPlugin::GetNumParameters()
//-------------------------------------------
{
    if ((m_pEffect) && (m_pEffect->numParams > 0))
    {
        return m_pEffect->numParams;
    }
    return 0;
}

//rewbs.VSTpresets
VstInt32 CVstPlugin::GetUID()
//---------------------------
{
    if (!(m_pEffect))
        return 0;
    return m_pEffect->uniqueID;
}

VstInt32 CVstPlugin::GetVersion()
//-------------------------------
{
    if (!(m_pEffect))
        return 0;

    return m_pEffect->version;
}

bool CVstPlugin::GetParams(float *param, VstInt32 bad_min, VstInt32 bad_max)
//------------------------------------------------------------------
{
    if (!(m_pEffect))
        return false;

    if (bad_max>m_pEffect->numParams)
        bad_max = m_pEffect->numParams;

    for (VstInt32 p = bad_min; p < bad_max; p++)
        param[p-bad_min]=GetParameter(p);

    return true;

}

bool CVstPlugin::RandomizeParams(VstInt32 minParam, VstInt32 maxParam)
//--------------------------------------------------------------------
{
    if (!(m_pEffect))
        return false;

    if (minParam==0 && maxParam==0)
    {
        minParam=0;
        maxParam=m_pEffect->numParams;

    }
    else if (maxParam>m_pEffect->numParams)
    {
        maxParam=m_pEffect->numParams;
    }

    for (VstInt32 p = minParam; p < maxParam; p++)
        SetParameter(p, (rand() / float(RAND_MAX)));

    return true;
}

bool CVstPlugin::SaveProgram(CString fileName)
//--------------------------------------------
{
    if (!(m_pEffect))
        return false;

    bool success;
    //Collect required data
    long ID = GetUID();
    long plugVersion = GetVersion();

    Cfxp* fxp = nullptr;

    //Construct & save fxp

    // try chunk-based preset:
    if(m_pEffect->flags & effFlagsProgramChunks)
    {
        void *chunk = NULL;
        long chunkSize = Dispatch(effGetChunk, 1,0, &chunk, 0);
        if ((chunkSize > 8) && (chunk))    //If chunk is less than 8 bytes, the plug must be kidding. :) (e.g.: imageline sytrus)
            fxp = new Cfxp(ID, plugVersion, 1, chunkSize, chunk);
    }
    // fall back on parameter based preset:
    if (fxp == nullptr)
    {
        //Collect required data
        PlugParamIndex numParams = GetNumParameters();
        float *params = new float[numParams];
        GetParams(params, 0, numParams);

        fxp = new Cfxp(ID, plugVersion, numParams, params);

        delete[] params;
    }

    success = fxp->Save(fileName);
    if (fxp)
        delete fxp;

    return success;

}

bool CVstPlugin::LoadProgram(CString fileName)
//--------------------------------------------
{
    if (!(m_pEffect))
        return false;

    Cfxp fxp(fileName);    //load from file

    //Verify
    if (m_pEffect->uniqueID != fxp.fxID)
        return false;

    if (fxp.fxMagic == fMagic) //Load preset based fxp
    {
        if (m_pEffect->numParams != fxp.numParams)
            return false;
        for (int p=0; p<fxp.numParams; p++)
            SetParameter(p, fxp.params[p]);
    }
    else if (fxp.fxMagic == chunkPresetMagic)
    {
        Dispatch(effSetChunk, 1, fxp.chunkSize, (uint8_t*)fxp.chunk, 0);
    }

    return true;
}
//end rewbs.VSTpresets

VstIntPtr CVstPlugin::Dispatch(VstInt32 opCode, VstInt32 index, VstIntPtr value, void *ptr, float opt)
//----------------------------------------------------------------------------------------------------
{
    long lresult = 0;

    try {
        if ((m_pEffect) && (m_pEffect->dispatcher))
        {
            #ifdef VST_LOG
            Log("About to Dispatch(%d) (Plugin=\"%s\"), index: %d, value: %d, value: %h, value: %f!\n", opCode, m_pFactory->szLibraryName, index, value, ptr, opt);
            #endif
            lresult = m_pEffect->dispatcher(m_pEffect, opCode, index, value, ptr, opt);
        }
    } catch (...) {
        CVstPluginManager::ReportPlugException("Exception in Dispatch(%d) (Plugin=\"%s\")!\n", opCode, m_pFactory->szLibraryName);
    }

    return lresult;
}


long CVstPlugin::GetCurrentProgram()
//----------------------------------
{
    if ((m_pEffect) && (m_pEffect->numPrograms > 0))
    {
        return Dispatch(effGetProgram, 0, 0, NULL, 0);
    }
    return 0;
}

bool CVstPlugin::GetProgramNameIndexed(long index, long category, char *text)
//---------------------------------------------------------------------------
{
    if ((m_pEffect) && (m_pEffect->numPrograms > 0))
    {
        return (Dispatch(effGetProgramNameIndexed, index, category, text, 0) == 1);
    }
    return false;
}


VOID CVstPlugin::SetCurrentProgram(UINT nIndex)
//---------------------------------------------
{
    if ((m_pEffect) && (m_pEffect->numPrograms > 0))
    {
        if (nIndex < (UINT)m_pEffect->numPrograms)
        {
            Dispatch(effSetProgram, 0, nIndex, NULL, 0);
        }
    }
}


PlugParamValue CVstPlugin::GetParameter(PlugParamIndex nIndex)
//------------------------------------------------------------
{
    FLOAT fResult = 0;
    if ((m_pEffect) && (nIndex < m_pEffect->numParams) && (m_pEffect->getParameter))
    {
        try {
            fResult = m_pEffect->getParameter(m_pEffect, nIndex);
        } catch (...) {
            //CVstPluginManager::ReportPlugException("Exception in getParameter (Plugin=\"%s\")!\n", m_pFactory->szLibraryName);
        }
    }
    //rewbs.VSTcompliance
    if (fResult<0.0f)
        return 0.0f;
    else if (fResult>1.0f)
        return 1.0f;
    else
    //end rewbs.VSTcompliance
        return fResult;
}


VOID CVstPlugin::SetParameter(PlugParamIndex nIndex, PlugParamValue fValue)
//-------------------------------------------------------------------------
{
    try {
        if ((m_pEffect) && (nIndex < m_pEffect->numParams) && (m_pEffect->setParameter))
        {
            if ((fValue >= 0.0f) && (fValue <= 1.0f))
                m_pEffect->setParameter(m_pEffect, nIndex, fValue);
        }
    } catch (...) {
        CVstPluginManager::ReportPlugException("Exception in SetParameter(%d, 0.%03d) (Plugin=%s)\n", nIndex, (int)(fValue*1000), m_pFactory->szLibraryName);
    }
}


VOID CVstPlugin::GetParamName(UINT nIndex, LPSTR pszName, UINT cbSize)
//--------------------------------------------------------------------
{
    if ((!pszName) || (!cbSize)) return;
    pszName[0] = 0;
    if ((m_pEffect) && (m_pEffect->numParams > 0) && (nIndex < (UINT)m_pEffect->numParams))
    {
        CHAR s[64]; // Increased to 64 bytes since 32 bytes doesn't seem to suffice for all plugs. Kind of ridiculous if you consider that kVstMaxParamStrLen = 8...
        s[0] = 0;
        Dispatch(effGetParamName, nIndex, 0, s, 0);
        s[bad_min(CountOf(s) - 1, cbSize - 1)] = 0;
        lstrcpyn(pszName, s, bad_min(cbSize, sizeof(s)));
    }
}


VOID CVstPlugin::GetParamLabel(UINT nIndex, LPSTR pszLabel)
//---------------------------------------------------------
{
    pszLabel[0] = 0;
    if ((m_pEffect) && (m_pEffect->numParams > 0) && (nIndex < (UINT)m_pEffect->numParams))
    {
        Dispatch(effGetParamLabel, nIndex, 0, pszLabel, 0);
    }
}


VOID CVstPlugin::GetParamDisplay(UINT nIndex, LPSTR pszDisplay)
//-------------------------------------------------------------
{
    pszDisplay[0] = 0;
    if ((m_pEffect) && (m_pEffect->numParams > 0) && (nIndex < (UINT)m_pEffect->numParams))
    {
        Dispatch(effGetParamDisplay, nIndex, 0, pszDisplay, 0);
    }
}


BOOL CVstPlugin::GetDefaultEffectName(LPSTR pszName)
//--------------------------------------------------
{
    pszName[0] = 0;
    if (m_bIsVst2)
    {
        Dispatch(effGetEffectName, 0,0, pszName, 0);
        return TRUE;
    }
    return FALSE;
}

void CVstPlugin::Init(unsigned long /*nFreq*/, int /*bReset*/)
//------------------------------------------------------------
{

}

void CVstPlugin::Resume()
//-----------------------
{
    long sampleRate = module_renderer::deprecated_global_mixing_freq;

    try {
        //reset some stuf
        m_MixState.nVolDecayL = 0;
        m_MixState.nVolDecayR = 0;
        Dispatch(effStopProcess, 0, 0, NULL, 0.0f);
        Dispatch(effMainsChanged, 0, 0, NULL, 0.0f);    // calls plugin's suspend
        if (sampleRate != m_nSampleRate) {
            m_nSampleRate=sampleRate;
            Dispatch(effSetSampleRate, 0, 0, NULL, m_nSampleRate);
        }
        Dispatch(effSetBlockSize, 0, modplug::mixgraph::MIX_BUFFER_SIZE, NULL, 0);
        //start off some stuff
        Dispatch(effMainsChanged, 0, 1, NULL, 0.0f);    // calls plugin's resume
        Dispatch(effStartProcess, 0, 0, NULL, 0.0f);
        m_bPlugResumed = true;
    } catch (...) {
        CVstPluginManager::ReportPlugException("Exception in Resume() (Plugin=%s)\n", m_pFactory->szLibraryName);
    }


}

void CVstPlugin::Suspend()
//------------------------
{
    try {
        Dispatch(effStopProcess, 0, 0, NULL, 0.0f);
        Dispatch(effMainsChanged, 0, 0, NULL, 0.0f); // calls plugin's suspend
        m_bPlugResumed=false;
    } catch (...) {
        CVstPluginManager::ReportPlugException("Exception in Suspend() (Plugin=%s)\n", m_pFactory->szLibraryName);
    }
}


void CVstPlugin::ProcessVSTEvents()
//---------------------------------
{
    // Process VST events
    if ((m_pEffect) && (m_pEffect->dispatcher) && (m_pEvList) && (m_pEvList->numEvents > 0))
    {
        try {
            m_pEffect->dispatcher(m_pEffect, effProcessEvents, 0, 0, m_pEvList, 0);
        } catch (...) {
            CVstPluginManager::ReportPlugException("Exception in ProcessVSTEvents() (Plugin=%s, pEventList:%p, numEvents:%d, MidicodeCh:%d, MidicodeEv:%d, MidicodeNote:%d, MidiCodeVel:%d)\n", m_pFactory->szLibraryName, m_pEvList, m_pEvList->numEvents,
(((VstMidiEvent *)(m_pEvList->events[0]))->midiData[0])&0x0f, (((VstMidiEvent *)(m_pEvList->events[0]))->midiData[0])&0xf0, (((VstMidiEvent *)(m_pEvList->events[0]))->midiData[1])&0xff, (((VstMidiEvent *)(m_pEvList->events[0]))->midiData[2])&0xff);
        }
    }

}

void CVstPlugin::ClearVSTEvents()
//-------------------------------
{
    // Clear VST events
    if ((m_pEvList) && (m_pEvList->numEvents > 0)) {
        m_pEvList->numEvents = 0;
    }
}

void CVstPlugin::RecalculateGain()
//--------------------------------
{
    float gain = 0.1f * (float)( m_pMixStruct ? (m_pMixStruct->Info.dwInputRouting>>16) & 0xff : 10 );
    if(gain < 0.1f) gain = 1.0f;

    if (m_bIsInstrument && m_pSndFile) {
        gain /= m_pSndFile->m_pConfig->getVSTiAttenuation();
        gain *= (m_pSndFile->m_nVSTiVolume / m_pSndFile->m_pConfig->getNormalVSTiVol());
    }
    m_fGain = gain;
}


void CVstPlugin::SetDryRatio(UINT param)
//--------------------------------------
{
    param = bad_min(param, 127);
    m_pMixStruct->fDryRatio = 1.0-(static_cast<float>(param)/127.0f);
}

/*
void CVstPlugin::Process(float **pOutputs, unsigned long nSamples)
//----------------------------------------------------------------
{
    float wetRatio, dryRatio;
    wetRatio *= m_fGain;
    dryRatio *= m_fGain;

    ProcessVSTEvents();
    if ((m_pEffect) && (m_pProcessFP) && (m_pInputs) && (m_pOutputs) && (m_pMixStruct)) {

        //Merge stereo input before sending to the plug if the plug can only handle one input.
        if (m_pEffect->numInputs == 1) {
            for (UINT i=0; i<nSamples; i++)    {
                m_pInputs[0][i] = 0.5f*m_pInputs[0][i] + 0.5f*m_pInputs[1][i];
            }
        }

        //Clear the buffers that will be receiving the plugin's output.
        for (UINT iOut=0; iOut<m_nOutputs; iOut++) {
            memset(m_pTempBuffer[iOut], 0, nSamples*sizeof(float));
            m_pOutputs[iOut] = m_pTempBuffer[iOut];
        }

        //Do the VST processing magic
        m_dwTimeAtStartOfProcess = timeGetTime();
        try {
            ASSERT(nSamples<=modplug::mixer::MIX_BUFFER_SIZE);
            m_pProcessFP(m_pEffect, m_pInputs, m_pOutputs, nSamples);
        } catch (char * str) {
            m_pMixStruct->Info.dwInputRouting |= MIXPLUG_INPUTF_BYPASS;
            CString processMethod = (m_pEffect->flags & effFlagsCanReplacing) ? "processReplacing" : "process";
            CVstPluginManager::ReportPlugException("The plugin %s threw an exception in %s: %s. It has automatically been set to \"Bypass\".", m_pMixStruct->Info.szName, processMethod, str);
            ClearVSTEvents();
            SetEvent(processCalled);
        }

        for(UINT i=0; i<nSamples; i++) {
            pOutL[stream][i] += m_pTempBuffer[stream][i]*wetRatio + m_pInputs[stream%2][i]*dryRatio;
        }

    }
}
*/
void CVstPlugin::Process(float *pOutL, float *pOutR, unsigned long nSamples)
//--------------------------------------------------------------------------
{
    float wetRatio, dryRatio; // rewbs.dryRatio [20040123]

    ProcessVSTEvents();

// -> DESC="effect plugin mixing mode combo"
// -> mixop == 0 : normal processing
// -> mixop == 1 : MIX += DRY - WET * wetRatio
// -> mixop == 2 : MIX += WET - DRY * wetRatio
// -> mixop == 3 : MIX -= WET - DRY * wetRatio
// -> mixop == 4 : MIX -= middle - WET * wetRatio + middle - DRY
// -> mixop == 5 : MIX_L += wetRatio * (WET_L - DRY_L) + dryRatio * (DRY_R - WET_R)
//                               MIX_R += dryRatio * (WET_L - DRY_L) + wetRatio * (DRY_R - WET_R)

    //If the plug is found & ok, continue
    if ((m_pEffect) && (m_pProcessFP) && (m_pInputs) && (m_pOutputs) && (m_pMixStruct))
    {
        int mixop;
        if (m_bIsInstrument) {
            mixop = 0; //force normal mix mode for instruments
        } else {
            mixop = m_pMixStruct ? (m_pMixStruct->Info.dwInputRouting>>8) & 0xff : 0;
        }

        //RecalculateGain();

        //Merge stereo input before sending to the plug if the plug can only handle one input.
        if (m_pEffect->numInputs == 1)
        {
            for (UINT i=0; i<nSamples; i++)    {
                m_MixState.pOutBufferL[i] = 0.5f*m_MixState.pOutBufferL[i] + 0.5f*m_MixState.pOutBufferR[i];
            }
        }

        for (UINT iOut=0; iOut<m_nOutputs; iOut++)
        {
            memset(m_pTempBuffer[iOut], 0, nSamples*sizeof(float));
            m_pOutputs[iOut] = m_pTempBuffer[iOut];
        }

        m_dwTimeAtStartOfProcess = timeGetTime();
        //Do the VST processing magic
        try {
            ASSERT(nSamples<=modplug::mixgraph::MIX_BUFFER_SIZE);
            m_pProcessFP(m_pEffect, m_pInputs, m_pOutputs, nSamples);
        } catch (...) {
            m_pMixStruct->Info.dwInputRouting |= MIXPLUG_INPUTF_BYPASS;
            CString processMethod = (m_pEffect->flags & effFlagsCanReplacing) ? "processReplacing" : "process";
            CVstPluginManager::ReportPlugException("The plugin %s threw an exception in %s. It has automatically been set to \"Bypass\".", m_pMixStruct->Info.szName, processMethod);
            ClearVSTEvents();
//                    SetEvent(processCalled);
        }

        //mix outputs of multi-output VSTs:
        if(m_nOutputs>2){
            // first, mix extra outputs on a stereo basis
            UINT nOuts = m_nOutputs;
            // so if nOuts is not even, let process the last output later
            if((nOuts % 2) == 1) nOuts--;

            // mix extra stereo outputs
            for(UINT iOut=2; iOut<nOuts; iOut++){
                for(UINT i=0; i<nSamples; i++)
                    m_pTempBuffer[iOut%2][i] += m_pTempBuffer[iOut][i]; //assumed stereo.
            }

            // if m_nOutputs is odd, mix half the signal of last output to each channel
            if(nOuts != m_nOutputs){
                // trick : if we are here, nOuts = m_nOutputs-1 !!!
                for(UINT i=0; i<nSamples; i++){
                    float v = 0.5f * m_pTempBuffer[nOuts][i];
                    m_pTempBuffer[0][i] += v;
                    m_pTempBuffer[1][i] += v;
                }
            }
        }

        // If there was just the one plugin output we copy it into our 2 outputs
        if(m_pEffect->numOutputs == 1)
        {
            wetRatio = 1-m_pMixStruct->fDryRatio;  //rewbs.dryRatio
            dryRatio = m_bIsInstrument ? 1 : m_pMixStruct->fDryRatio; //always mix full dry if this is an instrument

// -> CODE#0028
// -> DESC="effect plugin mixing mode combo"
/*
            for (UINT i=0; i<nSamples; i++)
            {
                //rewbs.wetratio - added the factors. [20040123]
                pOutL[i] += m_pTempBuffer[0][i]*wetRatio + m_MixState.pOutBufferL[i]*dryRatio;
                pOutR[i] += m_pTempBuffer[0][i]*wetRatio + m_MixState.pOutBufferR[i]*dryRatio;

                //If dry mix is ticked we add the unprocessed buffer,
                //except if this is an instrument since this it already been done:
                if ((m_pMixStruct->Info.dwInputRouting & MIXPLUG_INPUTF_WETMIX) && !isInstrument)
                {
                    pOutL[i] += m_MixState.pOutBufferL[i];
                    pOutR[i] += m_MixState.pOutBufferR[i];
                }
            }
*/
            // Wet/Dry range expansion [0,1] -> [-1,1]    update#02
            if(m_pEffect->numInputs > 0 && m_pMixStruct->Info.dwInputRouting & MIXPLUG_INPUTF_MIXEXPAND){
                wetRatio = 2.0f * wetRatio - 1.0f;
                dryRatio = -wetRatio;
            }

            wetRatio *= m_fGain;    // update#02
            dryRatio *= m_fGain;    // update#02

            // Mix operation
            switch(mixop){

                // Default mix (update#02)
                case 0:
                    for(UINT i=0; i<nSamples; i++){
                        //rewbs.wetratio - added the factors. [20040123]
                        pOutL[i] += m_pTempBuffer[0][i]*wetRatio + m_MixState.pOutBufferL[i]*dryRatio;
                        pOutR[i] += m_pTempBuffer[0][i]*wetRatio + m_MixState.pOutBufferR[i]*dryRatio;
                    }
                    break;

                // Wet subtract
                case 1:
                    for(UINT i=0; i<nSamples; i++){
                        pOutL[i] += m_MixState.pOutBufferL[i] - m_pTempBuffer[0][i]*wetRatio;
                        pOutR[i] += m_MixState.pOutBufferR[i] - m_pTempBuffer[0][i]*wetRatio;
                    }
                    break;

                // Dry subtract
                case 2:
                    for(UINT i=0; i<nSamples; i++){
                        pOutL[i] += m_pTempBuffer[0][i] - m_MixState.pOutBufferL[i]*dryRatio;
                        pOutR[i] += m_pTempBuffer[0][i] - m_MixState.pOutBufferR[i]*dryRatio;
                    }
                    break;

                // Mix subtract
                case 3:
                    for(UINT i=0; i<nSamples; i++){
                        pOutL[i] -= m_pTempBuffer[0][i] - m_MixState.pOutBufferL[i]*wetRatio;
                        pOutR[i] -= m_pTempBuffer[0][i] - m_MixState.pOutBufferR[i]*wetRatio;
                    }
                    break;

                // Middle subtract
                case 4:
                    for(UINT i=0; i<nSamples; i++){
                        float middle = ( pOutL[i] + m_MixState.pOutBufferL[i] + pOutR[i] + m_MixState.pOutBufferR[i] )/2.0f;
                        pOutL[i] -= middle - m_pTempBuffer[0][i]*wetRatio + middle - m_MixState.pOutBufferL[i];
                        pOutR[i] -= middle - m_pTempBuffer[0][i]*wetRatio + middle - m_MixState.pOutBufferR[i];
                    }
                    break;

                // Left/Right balance
                case 5:
                    if(m_pMixStruct->Info.dwInputRouting & MIXPLUG_INPUTF_MIXEXPAND){
                        wetRatio /= 2.0f;
                        dryRatio /= 2.0f;
                    }
                    for(UINT i=0; i<nSamples; i++){
                        pOutL[i] += wetRatio * (m_pTempBuffer[0][i] - m_MixState.pOutBufferL[i]) + dryRatio * (m_MixState.pOutBufferR[i] - m_pTempBuffer[0][i]);
                        pOutR[i] += dryRatio * (m_pTempBuffer[0][i] - m_MixState.pOutBufferL[i]) + wetRatio * (m_MixState.pOutBufferR[i] - m_pTempBuffer[0][i]);
                    }
                    break;
            }

            //If dry mix is ticked we add the unprocessed buffer,
            //except if this is an instrument since this it has already been done:
            if((m_pMixStruct->Info.dwInputRouting & MIXPLUG_INPUTF_WETMIX) && !m_bIsInstrument){
                for (UINT i=0; i<nSamples; i++){
                    pOutL[i] += m_MixState.pOutBufferL[i];
                    pOutR[i] += m_MixState.pOutBufferR[i];
                }
            }
// -! BEHAVIOUR_CHANGE#0028

        }
        //Otherwise we actually only cater for two outputs bad_max.
        else if (m_pEffect->numOutputs > 1)
        {
            wetRatio = 1-m_pMixStruct->fDryRatio;  //rewbs.dryRatio
            dryRatio = m_bIsInstrument ? 1 : m_pMixStruct->fDryRatio; //always mix full dry if this is an instrument

// -> CODE#0028
// -> DESC="effect plugin mixing mode combo"
/*
            for (UINT i=0; i<nSamples; i++)
            {

                //rewbs.wetratio - added the factors. [20040123]

                pOutL[i] += m_pTempBuffer[0][i]*wetRatio + m_MixState.pOutBufferL[i]*dryRatio;
                pOutR[i] += m_pTempBuffer[1][i]*wetRatio + m_MixState.pOutBufferR[i]*dryRatio;

                //If dry mix is ticked, we add the unprocessed buffer,
                //except if this is an instrument since it has already been done:
                if ((m_pMixStruct->Info.dwInputRouting & MIXPLUG_INPUTF_WETMIX) && !isInstrument)
                {
                    pOutL[i] += m_MixState.pOutBufferL[i];
                    pOutR[i] += m_MixState.pOutBufferR[i];
                }
            }
*/
            // Wet/Dry range expansion [0,1] -> [-1,1]    update#02
            if(m_pEffect->numInputs > 0 && m_pMixStruct->Info.dwInputRouting & MIXPLUG_INPUTF_MIXEXPAND){
                wetRatio = 2.0f * wetRatio - 1.0f;
                dryRatio = -wetRatio;
            }

            wetRatio *= m_fGain;    // update#02
            dryRatio *= m_fGain;    // update#02

            // Mix operation
            switch(mixop){

                // Default mix (update#02)
                case 0:
                    for(UINT i=0; i<nSamples; i++){
                        //rewbs.wetratio - added the factors. [20040123]
                        pOutL[i] += m_pTempBuffer[0][i]*wetRatio + m_MixState.pOutBufferL[i]*dryRatio;
                        pOutR[i] += m_pTempBuffer[1][i]*wetRatio + m_MixState.pOutBufferR[i]*dryRatio;
                    }
                    break;

                // Wet subtract
                case 1:
                    for(UINT i=0; i<nSamples; i++){
                        pOutL[i] += m_MixState.pOutBufferL[i] - m_pTempBuffer[0][i]*wetRatio;
                        pOutR[i] += m_MixState.pOutBufferR[i] - m_pTempBuffer[1][i]*wetRatio;
                    }
                    break;

                // Dry subtract
                case 2:
                    for(UINT i=0; i<nSamples; i++){
                        pOutL[i] += m_pTempBuffer[0][i] - m_MixState.pOutBufferL[i]*dryRatio;
                        pOutR[i] += m_pTempBuffer[1][i] - m_MixState.pOutBufferR[i]*dryRatio;
                    }
                    break;

                // Mix subtract
                case 3:
                    for(UINT i=0; i<nSamples; i++){
                        pOutL[i] -= m_pTempBuffer[0][i] - m_MixState.pOutBufferL[i]*wetRatio;
                        pOutR[i] -= m_pTempBuffer[1][i] - m_MixState.pOutBufferR[i]*wetRatio;
                    }
                    break;

                // Middle subtract
                case 4:
                    for(UINT i=0; i<nSamples; i++){
                        float middle = ( pOutL[i] + m_MixState.pOutBufferL[i] + pOutR[i] + m_MixState.pOutBufferR[i] )/2.0f;
                        pOutL[i] -= middle - m_pTempBuffer[0][i]*wetRatio + middle - m_MixState.pOutBufferL[i];
                        pOutR[i] -= middle - m_pTempBuffer[1][i]*wetRatio + middle - m_MixState.pOutBufferR[i];
                    }
                    break;

                // Left/Right balance
                case 5:
                    if(m_pMixStruct->Info.dwInputRouting & MIXPLUG_INPUTF_MIXEXPAND){
                        wetRatio /= 2.0f;
                        dryRatio /= 2.0f;
                    }
                    for(UINT i=0; i<nSamples; i++){
                        pOutL[i] += wetRatio * (m_pTempBuffer[0][i] - m_MixState.pOutBufferL[i]) + dryRatio * (m_MixState.pOutBufferR[i] - m_pTempBuffer[1][i]);
                        pOutR[i] += dryRatio * (m_pTempBuffer[0][i] - m_MixState.pOutBufferL[i]) + wetRatio * (m_MixState.pOutBufferR[i] - m_pTempBuffer[1][i]);
                    }
                    break;
            }

            //If dry mix is ticked we add the unprocessed buffer,
            //except if this is an instrument since this it has already been done:
            if((m_pMixStruct->Info.dwInputRouting & MIXPLUG_INPUTF_WETMIX) && !m_bIsInstrument){
                for (UINT i=0; i<nSamples; i++){
                    pOutL[i] += m_MixState.pOutBufferL[i];
                    pOutR[i] += m_MixState.pOutBufferR[i];
                }
            }
// -! BEHAVIOUR_CHANGE#0028
        }

    }

    ClearVSTEvents();
    //SetEvent(processCalled);
}


bool CVstPlugin::MidiSend(uint32_t dwMidiCode)
//-----------------------------------------
{
    if ((m_pEvList) && (m_pEvList->numEvents < VSTEVENT_QUEUE_LEN-1))
    {
        int insertPos;
        if ((dwMidiCode & 0xF0) == 0x80) {    //noteoffs go at the start of the queue.
            if (m_pEvList->numEvents) {
                for (int i=m_pEvList->numEvents; i>=1; i--){
                    m_pEvList->events[i] = m_pEvList->events[i-1];
                }
            }
            insertPos=0;
        } else {
            insertPos=m_pEvList->numEvents;
        }

        VstMidiEvent *pev = &m_ev_queue[m_pEvList->numEvents];
        m_pEvList->events[insertPos] = (VstEvent *)pev;
        pev->type = kVstMidiType;
        pev->byteSize = 24;
        pev->deltaFrames = 0;
        pev->flags = 0;
        pev->noteLength = 0;
        pev->noteOffset = 0;
        pev->detune = 0;
        pev->noteOffVelocity = 0;
        pev->reserved1 = 0;
        pev->reserved2 = 0;
        *(uint32_t *)pev->midiData = dwMidiCode;
        m_pEvList->numEvents++;
    #ifdef VST_LOG
        Log("Sending Midi %02X.%02X.%02X\n", pev->midiData[0]&0xff, pev->midiData[1]&0xff, pev->midiData[2]&0xff);
    #endif

        return true; //rewbs.instroVST
    }
    else
    {
        Log("VST Event queue overflow!\n");
        m_pEvList->numEvents = VSTEVENT_QUEUE_LEN-1;

        return false; //rewbs.instroVST
    }
}

//rewbs.VSTiNoteHoldonStopFix
void CVstPlugin::HardAllNotesOff()
//--------------------------------
{
    bool overflow;
    float in[2][modplug::mixgraph::SCRATCH_BUFFER_SIZE], out[2][modplug::mixgraph::SCRATCH_BUFFER_SIZE]; // scratch buffers

    // Relies on a wait on processCalled, which will never get set
    // if plug is bypassed.
    //if (IsBypassed())
    //    return;

    do {
        overflow=false;
        for (int mc=0; mc<16; mc++)            //all midi chans
        {
            UINT nCh = mc & 0x0f;
            uint32_t dwMidiCode = 0x80|nCh; //command|channel|velocity
            PVSTINSTCH pCh = &m_MidiCh[nCh];

            MidiPitchBend(mc, MIDI_PitchBend_Centre); // centre pitch bend
            MidiSend(0xB0|mc|(0x79<<8));                      // reset all controllers
            MidiSend(0xB0|mc|(0x7b<<8));                      // all notes off
            MidiSend(0xB0|mc|(0x78<<8));                      // all sounds off

            for (UINT i=0; i<128; i++)    //all notes
            {
                for (UINT c=0; c<MAX_VIRTUAL_CHANNELS; c++)
                {
                    while (pCh->uNoteOnMap[i][c] && !overflow)
                    {
                        overflow=!(MidiSend(dwMidiCode|(i<<8)));
                        pCh->uNoteOnMap[i][c]--;
                    }
                    if (overflow) break;    //yuck
                }
                if (overflow) break;    //yuck
            }
            if (overflow) break;    //yuck
        }
        // let plug process events
        // TODO: wait for notification from audio thread that process has been called,
        //       rather than re-call it here
        //ResetEvent(processCalled);                             // Unset processCalled.
        //WaitForSingleObject(processCalled, 10000); // Will not return until processCalled is set again,
        //                                           // i.e. until processReplacing() has been called.
        Process((float*)in, (float*)out, modplug::mixgraph::SCRATCH_BUFFER_SIZE);

    // If we had hit an overflow, we need to loop around and start again.
    } while (overflow);

}
//end rewbs.VSTiNoteHoldonStopFix

void CVstPlugin::MidiCC(UINT nMidiCh, UINT nController, UINT nParam, UINT /*trackChannel*/)
//------------------------------------------------------------------------------------------
{
    //Error checking
    if (--nMidiCh>16) { // Decrement midi chan cos we recieve a value in [1,17]; we want [0,16].
        nMidiCh=16;
    }
    if (nController>127) {
        nController=127;
    }
    if (nParam>127) {
        nParam=127;
    }

    if(m_pSndFile && m_pSndFile->GetModFlag(MSF_MIDICC_BUGEMULATION))
        MidiSend(nController<<16 | nParam<<8 | 0xB0|nMidiCh);
    else
        MidiSend(nParam<<16 | nController<<8 | 0xB0|nMidiCh);
}

short CVstPlugin::getMIDI14bitValueFromShort(short value)
//---------------------------------------------------------------------------------
{
    //http://www.srm.com/qtma/davidsmidispec.html:
    // The two bytes of the pitch bend message form a 14 bit number, 0 to 16383.
    // The value 8192 (sent, LSB first, as 0x00 0x40), is centered, or "no pitch bend."
    // The value 0 (0x00 0x00) means, "bend as low as possible," and,
    // similarly, 16383 (0x7F 0x7F) is to "bend as high as possible."
    // The exact range of the pitch bend is specific to the synthesizer.

    // pre: 0 <= value <= 16383

    uint8_t byte1 = value >> 7;                    // get last   7 bytes only
    uint8_t byte2 = value & 0x7F;                    // get first  7 bytes only
    short converted = byte1<<8 | byte2; // merge

    return converted;

}

//Bend midi pitch for given midi channel using tracker param (0x00-0xFF)
void CVstPlugin::MidiPitchBend(UINT nMidiCh, int nParam, UINT /*trackChannel*/)
//-------------------------------------------------------------------------
{
    nMidiCh--;            // move from 1-17 range to 0-16 range

    short increment = nParam * 0x2000/0xFF;
    short newPitchBendPos = m_nMidiPitchBendPos[nMidiCh] + increment;
    newPitchBendPos = bad_max(MIDI_PitchBend_Min, bad_min(MIDI_PitchBend_Max, newPitchBendPos)); // cap

    MidiPitchBend(nMidiCh, newPitchBendPos);
}

//Set midi pitch for given midi channel using uncoverted midi value (0-16383)
void CVstPlugin::MidiPitchBend(UINT nMidiCh, short newPitchBendPos) {
//----------------------------------------------------------------
    m_nMidiPitchBendPos[nMidiCh] = newPitchBendPos; //store pitch bend position
    short converted = getMIDI14bitValueFromShort(newPitchBendPos);
    MidiSend(converted<<8 | MIDI_PitchBend_Command|nMidiCh);
}


//rewbs.introVST - many changes to MidiCommand, still to be refined.
void CVstPlugin::MidiCommand(UINT nMidiCh, UINT nMidiProg, uint16_t wMidiBank, UINT note, UINT vol, UINT trackChannel)
//----------------------------------------------------------------------------------------------------------------
{
    UINT nCh = (--nMidiCh) & 0x0f;
    PVSTINSTCH pCh = &m_MidiCh[nCh];
    uint32_t dwMidiCode = 0;
    bool bankChanged = (pCh->wMidiBank != --wMidiBank) && (wMidiBank < 0x80);
    bool progChanged = (pCh->nProgram != --nMidiProg) && (nMidiProg < 0x80);
    //bool chanChanged = nCh != m_nPreviousMidiChan;
    //get vol in [0,128[
    vol = bad_min(vol/2, 127);

    // Note: Some VSTis update bank/prog on midi channel change, others don't.
    //       For those that don't, we do it for them.
    // Note 17/12/2005 - I have disabled that because it breaks Edirol Hypercanvas
    // and I can't remember why it would be useful.

    // Bank change
    if ((wMidiBank < 0x80) && ( bankChanged /*|| chanChanged */))
    {
        pCh->wMidiBank = wMidiBank;
        MidiSend(((wMidiBank<<16)|(0x20<<8))|(0xB0|nCh));
    }
    // Program change
    // Note: Some plugs (Edirol Orchestral) don't update on bank change only -
    // need to force it by sending prog change too.
    if ((nMidiProg < 0x80) && (progChanged || bankChanged /*|| chanChanged */ ))
    {
        pCh->nProgram = nMidiProg;
        //GetSoundFile()->ProcessMIDIMacro(trackChannel, false, GetSoundFile()->m_MidiCfg.szMidiGlb[MIDIOUT_PROGRAM], 0);
        MidiSend((nMidiProg<<8)|(0xC0|nCh));
    }


    // Specific Note Off
    if (note > NoteKeyOff)                    //rewbs.vstiLive
    {
        dwMidiCode = 0x80|nCh; //note off, on chan nCh

        note--;
        UINT i = note - NoteKeyOff;
        if (pCh->uNoteOnMap[i][trackChannel])
        {
            pCh->uNoteOnMap[i][trackChannel]--;
            MidiSend(dwMidiCode|(i<<8));
        }
    }

    // "Hard core" All Sounds Off on this midi and tracker channel
    // This one doesn't check the note mask - just one note off per note.
    // Also less likely to cause a VST event buffer overflow.
    else if (note == NoteNoteCut)    // ^^
    {
        //MidiSend(0xB0|nCh|(0x79<<8)); // reset all controllers
        MidiSend(0xB0|nCh|(0x7b<<8));   // all notes off
        MidiSend(0xB0|nCh|(0x78<<8));   // all sounds off

        dwMidiCode = 0x80|nCh|(vol<<16); //note off, on chan nCh; vol is note off velocity.
        for (UINT i=0; i<128; i++)    //all notes
        {
            //if (!(m_pEffect->uniqueID==1413633619L) || pCh->uNoteOnMap[i][trackChannel]>0) { //only send necessary NOs for TBVS.
                pCh->uNoteOnMap[i][trackChannel]=0;
                MidiSend(dwMidiCode|(i<<8));
            //}
        }

    }

    // All "active" notes off on this midi and tracker channel
    // using note mask.
    else if (note > 0x80) // ==
    {
        dwMidiCode = 0x80|nCh|(vol<<16); //note off, on chan nCh; vol is note off velocity.

        for (UINT i=0; i<128; i++)
        {
            // Some VSTis need a note off for each instance of a note on, e.g. fabfilter.
            // But this can cause a VST event overflow if we have many active notes...
            while (pCh->uNoteOnMap[i][trackChannel])
            {
                if (MidiSend(dwMidiCode|(i<<8))) {
                    pCh->uNoteOnMap[i][trackChannel]--;
                } else { //VST event queue overflow, no point in submitting more note offs.
                    break; //todo: secondary buffer?
                }
            }
        }
    }

    // Note On
    else if (note > 0)
    {
        dwMidiCode = 0x90|nCh; //note on, on chan nCh

        note--;

        //reset pitch bend on each new note, tracker style.
        if (m_nMidiPitchBendPos[nCh] != MIDI_PitchBend_Centre) {
            MidiPitchBend(nCh, MIDI_PitchBend_Centre);
        }

        // count instances of active notes.
        // This is to send a note off for each instance of a note, for plugs like Fabfilter.
        // Problem: if a note dies out naturally and we never send a note off, this counter
        // will block at bad_max until note off. Is this a problem?
        // Safe to assume we won't need more than 16 note offs bad_max on a given note?
        if (pCh->uNoteOnMap[note][trackChannel]<17)
            pCh->uNoteOnMap[note][trackChannel]++;



        MidiSend(dwMidiCode|(note<<8)|(vol<<16));
    }

    m_nPreviousMidiChan = nCh;

}

bool CVstPlugin::isPlaying(UINT note, UINT midiChn, UINT trackerChn)
//------------------------------------------------------------------
{
    note--;
    PVSTINSTCH pMidiCh = &m_MidiCh[(midiChn-1) & 0x0f];
    return  (pMidiCh->uNoteOnMap[note][trackerChn] != 0);
}

bool CVstPlugin::MoveNote(UINT note, UINT midiChn, UINT sourceTrackerChn, UINT destTrackerChn)
//---------------------------------------------------------------------------------------------
{
    note--;
    PVSTINSTCH pMidiCh = &m_MidiCh[(midiChn-1) & 0x0f];

    if (!(pMidiCh->uNoteOnMap[note][sourceTrackerChn]))
        return false;

    pMidiCh->uNoteOnMap[note][sourceTrackerChn]--;
    pMidiCh->uNoteOnMap[note][destTrackerChn]++;
    return true;
}
//end rewbs.introVST


void CVstPlugin::SetZxxParameter(UINT nParam, UINT nValue)
//--------------------------------------------------------
{
    FLOAT fValue = nValue / 127.0f;
    SetParameter(nParam, fValue);
}

//rewbs.smoothVST
UINT CVstPlugin::GetZxxParameter(UINT nParam)
//-------------------------------------------
{
    return (UINT) (GetParameter(nParam) * 127.0f+0.5f);
}
//end rewbs.smoothVST


void CVstPlugin::SaveAllParameters()
//----------------------------------
{
    if ((m_pEffect) && (m_pMixStruct))
    {
        m_pMixStruct->defaultProgram = -1;
        m_bModified = false;

        if ((m_pEffect->flags & effFlagsProgramChunks)
         && (Dispatch(effIdentify, 0,0, NULL, 0) == 'NvEf')
         && (m_pEffect->uniqueID != CCONST('S', 'y', 't', 'r'))) //special case: imageline sytrus pretends to support chunks but gives us garbage.
        {
            PVOID p = NULL;
            LONG nByteSize = 0;

            // Try to get whole bank
            if (m_pEffect->uniqueID != 1984054788) { //special case: VB ffx4 pretends to get a valid bank but gives us garbage.
                nByteSize = Dispatch(effGetChunk, 0,0, &p, 0);
            }

            if (/*(nByteSize < 8) ||*/ !p) { //If bank is less that 8 bytes, the plug must be kidding. :) (e.g.: imageline sytrus)
                nByteSize = Dispatch(effGetChunk, 1,0, &p, 0);     // Getting bank failed, try to get get just preset
            } else {
                m_pMixStruct->defaultProgram = GetCurrentProgram();    //we managed to get the bank, now we need to remember which program we're on.
            }
            if (/*(nByteSize >= 8) && */(p)) //If program is less that 8 bytes, the plug must be kidding. :) (e.g.: imageline sytrus)
            {
                if ((m_pMixStruct->pPluginData) && (m_pMixStruct->nPluginDataSize >= (UINT)(nByteSize+4)))
                {
                    m_pMixStruct->nPluginDataSize = nByteSize+4;
                } else
                {
                    delete[] m_pMixStruct->pPluginData;
                    m_pMixStruct->nPluginDataSize = 0;
                    m_pMixStruct->pPluginData = new char[nByteSize+4];
                    if (m_pMixStruct->pPluginData)
                    {
                        m_pMixStruct->nPluginDataSize = nByteSize+4;
                    }
                }
                if (m_pMixStruct->pPluginData)
                {
                    *(ULONG *)m_pMixStruct->pPluginData = 'NvEf';
                    memcpy(((uint8_t*)m_pMixStruct->pPluginData)+4, p, nByteSize);
                    return;
                }
            }
        }
        // This plug doesn't support chunks: save parameters
        UINT nParams = (m_pEffect->numParams > 0) ? m_pEffect->numParams : 0;
        UINT nLen = nParams * sizeof(FLOAT);
        if (!nLen) return;
        nLen += 4;
        if ((m_pMixStruct->pPluginData) && (m_pMixStruct->nPluginDataSize >= nLen))
        {
            m_pMixStruct->nPluginDataSize = nLen;
        } else
        {
            if (m_pMixStruct->pPluginData) delete[] m_pMixStruct->pPluginData;
            m_pMixStruct->nPluginDataSize = 0;
            m_pMixStruct->pPluginData = new char[nLen];
            if (m_pMixStruct->pPluginData)
            {
                m_pMixStruct->nPluginDataSize = nLen;
            }
        }
        if (m_pMixStruct->pPluginData)
        {
            FLOAT *p = (FLOAT *)m_pMixStruct->pPluginData;
            *(ULONG *)p = 0;
            p++;
            for (UINT i=0; i<nParams; i++)
            {
                p[i] = GetParameter(i);
            }
        }
    }
    return;
}


void CVstPlugin::RestoreAllParameters(long nProgram)
//--------------------------------------------------
{
    if ((m_pEffect) && (m_pMixStruct) && (m_pMixStruct->pPluginData) && (m_pMixStruct->nPluginDataSize >= 4))
    {
        UINT nParams = (m_pEffect->numParams > 0) ? m_pEffect->numParams : 0;
        UINT nLen = nParams * sizeof(FLOAT);
        ULONG nType = *(ULONG *)m_pMixStruct->pPluginData;

        if ((Dispatch(effIdentify, 0,0, NULL, 0) == 'NvEf') && (nType == 'NvEf'))
        {
            PVOID p = NULL;
            Dispatch(effGetChunk, 0,0, &p, 0); //init plug for chunk reception

            if ((nProgram>=0) && (nProgram < m_pEffect->numPrograms)) { // Bank:
                Dispatch(effSetChunk, 0, m_pMixStruct->nPluginDataSize-4, ((uint8_t *)m_pMixStruct->pPluginData)+4, 0);
                SetCurrentProgram(nProgram);
            } else { // Program:
                Dispatch(effSetChunk, 1, m_pMixStruct->nPluginDataSize-4, ((uint8_t *)m_pMixStruct->pPluginData)+4, 0);
            }

        } else
        {
            FLOAT *p = (FLOAT *)m_pMixStruct->pPluginData;
            if (m_pMixStruct->nPluginDataSize >= nLen+4) p++;
            if (m_pMixStruct->nPluginDataSize >= nLen)
            {
                for (UINT i=0; i<nParams; i++)
                {
                    SetParameter(i, p[i]);
                }
            }
        }
    }
}


VOID CVstPlugin::ToggleEditor()
//-----------------------------
{
    if (!m_pEffect) return;

    try {
        if ((m_pEditor) && (!m_pEditor->m_hWnd))
        {
            delete m_pEditor;
            m_pEditor = NULL;
        }
        if (m_pEditor)
        {
            if (m_pEditor->m_hWnd) m_pEditor->DoClose();
            if ((volatile void *)m_pEditor) delete m_pEditor;
            m_pEditor = NULL;
        } else
        {
            //rewbs.defaultPlugGui
            if (HasEditor())
                m_pEditor =  new COwnerVstEditor(this);
            else
                m_pEditor = new CDefaultVstEditor(this);
            //end rewbs.defaultPlugGui

            if (m_pEditor)
                m_pEditor->OpenEditor(CMainFrame::GetMainFrame());
        }
    } catch (...) {
        CVstPluginManager::ReportPlugException("Exception in ToggleEditor() (Plugin=%s)\n", m_pFactory->szLibraryName);
    }
}


UINT CVstPlugin::GetNumCommands()
//-------------------------------
{
    if (m_pEffect)
    {
        return m_pEffect->numParams;
    }
    return 0;
}


BOOL CVstPlugin::GetCommandName(UINT nIndex, LPSTR pszName)
//---------------------------------------------------------
{
    if (m_pEffect)
    {
        return Dispatch(effGetParamName, nIndex,0,pszName,0);
    }
    return 0;
}


BOOL CVstPlugin::ExecuteCommand(UINT nIndex)
//------------------------------------------
{
    return 0;
}

//rewbs.defaultPlugGui
CAbstractVstEditor* CVstPlugin::GetEditor()
//-----------------------------------------
{
    return m_pEditor;
}

bool CVstPlugin::Bypass(bool bypass)
//-----------------------------------
{
    if (bypass)
        m_pMixStruct->Info.dwInputRouting |= MIXPLUG_INPUTF_BYPASS;
    else
        m_pMixStruct->Info.dwInputRouting &= ~MIXPLUG_INPUTF_BYPASS;

    if (m_pModDoc)
        m_pModDoc->UpdateAllViews(NULL, HINT_MIXPLUGINS, NULL);

    return bypass;
}
bool CVstPlugin::Bypass()
//-----------------------
{
    return Bypass(!IsBypassed());
}

bool CVstPlugin::IsBypassed()
//---------------------------
{
    return ((m_pMixStruct->Info.dwInputRouting & MIXPLUG_INPUTF_BYPASS) != 0);
}

//end rewbs.defaultPlugGui
//rewbs.defaultPlugGui: CVstEditor now COwnerVstEditor


//rewbs.VSTcompliance
BOOL CVstPlugin::GetSpeakerArrangement()
//--------------------------------------
{
    VstSpeakerArrangement **pSA = NULL;
    Dispatch(effGetSpeakerArrangement, 0,0,pSA,0);
    //Dispatch(effGetSpeakerArrangement, 0,(long)pSA,NULL,0);
    if (pSA)
        memcpy((void*)(&speakerArrangement), (void*)(pSA[0]), sizeof(VstSpeakerArrangement));

    return true;
}
void CVstPlugin::NotifySongPlaying(bool playing)
//----------------------------------------------
{
    m_bSongPlaying=playing;
}

UINT CVstPlugin::FindSlot()
//------------------------
{
    UINT slot=0;
    if (m_pSndFile) {
        while ((m_pMixStruct != &(m_pSndFile->m_MixPlugins[slot])) && slot<MAX_MIXPLUGINS-1) {
            slot++;
        }
    }
    return slot;
}

void CVstPlugin::SetSlot(UINT slot)
//------------------------
{
    m_nSlot = slot;
}

UINT CVstPlugin::GetSlot()
//------------------------
{
    return m_nSlot;
}

void CVstPlugin::UpdateMixStructPtr(PSNDMIXPLUGIN p)
//--------------------------------------------------
{
    m_pMixStruct = p;
}

//end rewbs.VSTcompliance

bool CVstPlugin::isInstrument() // ericus 18/02/2005
//-----------------------------
{
    if(m_pEffect) return ((m_pEffect->flags & effFlagsIsSynth) || (!m_pEffect->numInputs)) ? true : false; // rewbs.dryRatio
    return false;
}

bool CVstPlugin::CanRecieveMidiEvents()
//-------------------------------------
{
    CString s = "receiveVstMidiEvent";
    return (CVstPlugin::Dispatch(effCanDo, 0, 0, (char*)(LPCTSTR)s, 0)) ? true : false;
}

bool CVstPlugin::KeysRequired()
//-----------------------------
{
    return (CVstPlugin::Dispatch(effKeysRequired, 0, 0, NULL, 0) != 0);
}

void CVstPlugin::GetOutputPlugList(CArray<CVstPlugin*,CVstPlugin*> &list)
//-----------------------------------------------------------------------
{
    // At the moment we know there will only be 1 output.
    // Returning NULL ptr means plugin outputs directly to master.
    list.RemoveAll();

    CVstPlugin *pOutputPlug = NULL;
    if (m_pMixStruct->Info.dwOutputRouting & 0x80)    {
        UINT nOutput = m_pMixStruct->Info.dwOutputRouting & 0x7f;
        if (m_pSndFile && (nOutput > m_nSlot) && (nOutput < MAX_MIXPLUGINS)) {
            pOutputPlug = (CVstPlugin*) m_pSndFile->m_MixPlugins[nOutput].pMixPlugin;
        }
    }
    list.Add(pOutputPlug);

    return;
}

void CVstPlugin::GetInputPlugList(CArray<CVstPlugin*,CVstPlugin*> &list)
//----------------------------------------------------------------------
{
    if(m_pSndFile == 0) return;

    CArray<CVstPlugin*, CVstPlugin*> candidatePlugOutputs;
    CVstPlugin* pCandidatePlug = NULL;
    list.RemoveAll();

    for (int nPlug=0; nPlug<MAX_MIXPLUGINS; nPlug++) {
        pCandidatePlug = (CVstPlugin*) m_pSndFile->m_MixPlugins[nPlug].pMixPlugin;
        if (pCandidatePlug) {
            pCandidatePlug->GetOutputPlugList(candidatePlugOutputs);

            for(int nOutput=0; nOutput<candidatePlugOutputs.GetSize(); nOutput++)     {
                if (candidatePlugOutputs[nOutput] == this) {
                    list.Add(pCandidatePlug);
                    break;
                }
            }
        }
    }

    return;
}

void CVstPlugin::GetInputInstrumentList(CArray<UINT,UINT> &list)
//--------------------------------------------------------------
{
    list.RemoveAll();
    if(m_pSndFile == 0) return;

    UINT nThisMixPlug = m_nSlot+1;            //m_nSlot is position in mixplug array.
    for (int nIns=0; nIns<MAX_INSTRUMENTS; nIns++) {
        if (m_pSndFile->Instruments[nIns] && (m_pSndFile->Instruments[nIns]->nMixPlug==nThisMixPlug)) {
            list.Add(nIns);
        }
    }

    return;

}

void CVstPlugin::GetInputChannelList(CArray<UINT,UINT> &list)
//------------------------------------------------------------
{
    if(m_pSndFile == 0) return;
    list.RemoveAll();

    UINT nThisMixPlug = m_nSlot+1;            //m_nSlot is position in mixplug array.
    const modplug::tracker::chnindex_t chnCount = m_pSndFile->GetNumChannels();
    for (modplug::tracker::chnindex_t nChn=0; nChn<chnCount; nChn++) {
        if (m_pSndFile->ChnSettings[nChn].nMixPlugin==nThisMixPlug) {
            list.Add(nChn);
        }
    }

    return;

}

static const float _f2si = 32768.0f;
static const float _si2f = 1.0f / 32768.0f;

void X86_FloatToStereo16Mix(const float *pIn1, const float *pIn2, short int *pOut, int sampleframes)
//--------------------------------------------------------------------------------------------------
{
    for (int i=0; i<sampleframes; i++)
    {
        int xl = (int)(pIn1[i] * _f2si);
        int xr = (int)(pIn2[i] * _f2si);
        if (xl < -32768) xl = -32768;
        if (xl > 32767) xl = 32767;
        if (xr < -32768) xr = -32768;
        if (xr > 32767) xr = 32767;
        pOut[i*2] = xl;
        pOut[i*2+1] = xr;
    }
}


void X86_Stereo16AddMixToFloat(const short int *pIn, float *pOut1, float *pOut2, int sampleframes)
//------------------------------------------------------------------------------------------------
{
    for (int i=0; i<sampleframes; i++)
    {
        float xl = _si2f * (float)pIn[i*2];
        float xr = _si2f * (float)pIn[i*2+1];
        pOut1[i] += xl;
        pOut2[i] += xr;
    }
}


#ifdef ENABLE_SSE
void SSE_FloatToStereo16Mix(const float *pIn1, const float *pIn2, short int *pOut, int sampleframes)
//--------------------------------------------------------------------------------------------------
{
    _asm {
    mov eax, pIn1
    mov edx, pIn2
    mov ebx, pOut
    mov ecx, sampleframes
    movss xmm2, _f2si
    xorps xmm0, xmm0
    xorps xmm1, xmm1
    shufps xmm2, xmm2, 0x00
    pxor mm0, mm0
    inc ecx
    shr ecx, 1
mainloop:
    movlps xmm0, [eax]
    movlps xmm1, [edx]
    mulps xmm0, xmm2
    mulps xmm1, xmm2
    add ebx, 8
    cvtps2pi mm0, xmm0    // mm0 = [ x2l | x1l ]
    add eax, 8
    cvtps2pi mm1, xmm1    // mm1 = [ x2r | x1r ]
    add edx, 8
    packssdw mm0, mm1    // mm0 = [x2r|x1r|x2l|x1l]
    pshufw mm0, mm0, 0xD8
    dec ecx
    movq [ebx-8], mm0
    jnz mainloop
    emms
    }
}


void SSE_Stereo16AddMixToFloat(const short int *pIn, float *pOut1, float *pOut2, int sampleframes)
//------------------------------------------------------------------------------------------------
{
    _asm {
    mov ebx, pIn
    mov eax, pOut1
    mov edx, pOut2
    mov ecx, sampleframes
    movss xmm7, _si2f
    inc ecx
    shr ecx, 1
    shufps xmm7, xmm7, 0x00
    xorps xmm0, xmm0
    xorps xmm1, xmm1
    xorps xmm2, xmm2
mainloop:
    movq mm0, [ebx]            // mm0 = [x2r|x2l|x1r|x1l]
    add ebx, 8
    pxor mm1, mm1
    pxor mm2, mm2
    punpcklwd mm1, mm0    // mm1 = [x1r|0|x1l|0]
    punpckhwd mm2, mm0    // mm2 = [x2r|0|x2l|0]
    psrad mm1, 16            // mm1 = [x1r|x1l]
    movlps xmm2, [eax]
    psrad mm2, 16            // mm2 = [x2r|x2l]
    cvtpi2ps xmm0, mm1    // xmm0 = [ ? | ? |x1r|x1l]
    dec ecx
    cvtpi2ps xmm1, mm2    // xmm1 = [ ? | ? |x2r|x2l]
    movhps xmm2, [edx]    // xmm2 = [y2r|y1r|y2l|y1l]
    movlhps xmm0, xmm1    // xmm0 = [x2r|x2l|x1r|x1l]
    shufps xmm0, xmm0, 0xD8
    lea eax, [eax+8]
    mulps xmm0, xmm7
    addps xmm0, xmm2
    lea edx, [edx+8]
    movlps [eax-8], xmm0
    movhps [edx-8], xmm0
    jnz mainloop
    emms
    }
}

#endif

const char* SNDMIXPLUGIN::GetLibraryName()
//------------------------------------
{
    Info.szLibraryName[63] = 0;
    if(Info.szLibraryName[0]) return Info.szLibraryName;
    else return 0;
}

#endif // NO_VST

CString SNDMIXPLUGIN::GetParamName(const UINT index) const
//--------------------------------------------------------
{
    if(pMixPlugin)
    {
        char s[64];
        ((CVstPlugin*)(pMixPlugin))->GetParamName(index, s, sizeof(s));
        s[sizeof(s)-1] = 0;
        return CString(s);
    }
    else
        return CString();
}
