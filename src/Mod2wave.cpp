#include "stdafx.h"
#include "mptrack.h"
#include "legacy_soundlib/sndfile.h"
#include "legacy_soundlib/dlsbank.h"
#include "mainfrm.h"
#include "mpdlgs.h"
#include "vstplug.h"
#include "mod2wave.h"
#include "legacy_soundlib/Wav.h"

#define NUMMIXRATE    16

UINT nMixingRates[NUMMIXRATE] = {
    16000,
    19800,
    20000,
    22050,
    24000,
    32000,
    33075,
    37800,
    40000,
    44100,
    48000,
    64000,
    88200,
    96000,
    176400,
    192000,
};

LPCSTR gszChnCfgNames[3] =
{
    "Mono",
    "Stereo",
    "Quad"
};

// this converts a buffer of 32-bit integer sample data to 32 bit floating point
static void __cdecl M2W_32ToFloat(void *pBuffer, long nCount)
{
//    const float _ki2f = 1.0f / (FLOAT)(ULONG)(0x80000000); //olivier
    const float _ki2f = 1.0f / (FLOAT)(ULONG)(0x7fffffff); //ericus' 32bit fix
//    const float _ki2f = 1.0f / (FLOAT)(ULONG)(0x7ffffff);  //robin
    _asm {
    mov esi, pBuffer
    mov ecx, nCount
    fld _ki2f
    test ecx, 1
    jz evencount
    fild dword ptr [esi]
    fmul st(0), st(1)
    fstp dword ptr [esi]
    add esi, 4
evencount:
    shr ecx, 1
    or ecx, ecx
    jz loopdone
cvtloop:
    fild dword ptr [esi]
    fild dword ptr [esi+4]
    fmul st(0), st(2)
    fstp dword ptr [esi+4]
    fmul st(0), st(1)
    fstp dword ptr [esi]
    add esi, 8
    dec ecx
    jnz cvtloop
loopdone:
    fstp st(0)
    }
}

///////////////////////////////////////////////////
// CWaveConvert - setup for converting a wave file

BEGIN_MESSAGE_MAP(CWaveConvert, CDialog)
    ON_COMMAND(IDC_CHECK1,                    OnCheck1)
    ON_COMMAND(IDC_CHECK2,                    OnCheck2)
    ON_COMMAND(IDC_CHECK4,                    OnCheckChannelMode)
    ON_COMMAND(IDC_CHECK6,                    OnCheckInstrMode)
    ON_COMMAND(IDC_RADIO1,                    UpdateDialog)
    ON_COMMAND(IDC_RADIO2,                    UpdateDialog)
    ON_COMMAND(IDC_PLAYEROPTIONS,   OnPlayerOptions) //rewbs.resamplerConf
    ON_CBN_SELCHANGE(IDC_COMBO2,    OnFormatChanged)
END_MESSAGE_MAP()


CWaveConvert::CWaveConvert(CWnd *parent, modplug::tracker::orderindex_t nMinOrder, modplug::tracker::orderindex_t nMaxOrder):
    CDialog(IDD_WAVECONVERT, parent)
//-----------------------------------------------------------------------------------
{
    m_bGivePlugsIdleTime = false;
    m_bNormalize = false;
    m_bHighQuality = false;
    m_bSelectPlay = false;
    if(nMinOrder != modplug::tracker::OrderIndexInvalid && nMaxOrder != modplug::tracker::OrderIndexInvalid)
    {
        // render selection
        m_nMinOrder = nMinOrder;
        m_nMaxOrder = nMaxOrder;
        m_bSelectPlay = true;
    } else
    {
        m_nMinOrder = m_nMaxOrder = 0;
    }
    m_dwFileLimit = 0;
    m_dwSongLimit = 0;
    MemsetZero(WaveFormat);
    WaveFormat.Format.wFormatTag = WAVE_FORMAT_PCM;
    WaveFormat.Format.nChannels = 2;
    WaveFormat.Format.nSamplesPerSec = 44100;
    WaveFormat.Format.wBitsPerSample = 16;
    WaveFormat.Format.nBlockAlign = (WaveFormat.Format.wBitsPerSample * WaveFormat.Format.nChannels) / 8;
    WaveFormat.Format.nAvgBytesPerSec = WaveFormat.Format.nSamplesPerSec * WaveFormat.Format.nBlockAlign;
}


void CWaveConvert::DoDataExchange(CDataExchange *pDX)
//---------------------------------------------------
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_COMBO1,    m_CbnSampleRate);
    DDX_Control(pDX, IDC_COMBO2,    m_CbnSampleFormat);
    DDX_Control(pDX, IDC_EDIT3,            m_EditMinOrder);
    DDX_Control(pDX, IDC_EDIT4,            m_EditMaxOrder);
}


BOOL CWaveConvert::OnInitDialog()
//-------------------------------
{
    CHAR s[128];

    CDialog::OnInitDialog();
    CheckRadioButton(IDC_RADIO1, IDC_RADIO2, m_bSelectPlay ? IDC_RADIO2 : IDC_RADIO1);

    CheckDlgButton(IDC_CHECK3, MF_CHECKED);            // HQ resampling
    CheckDlgButton(IDC_CHECK5, MF_UNCHECKED);    // rewbs.NoNormalize

// -> CODE#0024
// -> DESC="wav export update"
    CheckDlgButton(IDC_CHECK4, MF_UNCHECKED);
    CheckDlgButton(IDC_CHECK6, MF_UNCHECKED);
// -! NEW_FEATURE#0024

    SetDlgItemInt(IDC_EDIT3, m_nMinOrder);
    SetDlgItemInt(IDC_EDIT4, m_nMaxOrder);


    for (size_t i = 0; i < CountOf(nMixingRates); i++)
    {
        UINT n = nMixingRates[i];
        wsprintf(s, "%d Hz", n);
        m_CbnSampleRate.SetItemData(m_CbnSampleRate.AddString(s), n);
        if (n == WaveFormat.Format.nSamplesPerSec) m_CbnSampleRate.SetCurSel(i);
    }
// -> CODE#0024
// -> DESC="wav export update"
//    for (UINT j=0; j<3*3; j++)
    for (UINT j=0; j<3*4; j++)
// -! NEW_FEATURE#0024
    {
// -> CODE#0024
// -> DESC="wav export update"
//            UINT n = 3*3-1-j;
//            UINT nBits = 8 << (n % 3);
//            UINT nChannels = 1 << (n/3);
        UINT n = 3*4-1-j;
        UINT nBits = 8 * (1 + n % 4);
        UINT nChannels = 1 << (n/4);
// -! NEW_FEATURE#0024
        if ((nBits >= 16) || (nChannels <= 2))
        {
// -> CODE#0024
// -> DESC="wav export update"
//                    wsprintf(s, "%s, %d Bit", gszChnCfgNames[j/3], nBits);
            wsprintf(s, "%s, %d-Bit", gszChnCfgNames[n/4], nBits);
// -! NEW_FEATURE#0024
            UINT ndx = m_CbnSampleFormat.AddString(s);
            m_CbnSampleFormat.SetItemData(ndx, (nChannels<<8)|nBits);
            if ((nBits == WaveFormat.Format.wBitsPerSample) && (nChannels == WaveFormat.Format.nChannels))
            {
                m_CbnSampleFormat.SetCurSel(ndx);
            }
        }
    }
    UpdateDialog();
    return TRUE;
}


void CWaveConvert::OnFormatChanged()
//----------------------------------
{
    uint32_t dwFormat = m_CbnSampleFormat.GetItemData(m_CbnSampleFormat.GetCurSel());
    UINT nBits = dwFormat & 0xFF;
// -> CODE#0024
// -> DESC="wav export update"
//    ::EnableWindow( ::GetDlgItem(m_hWnd, IDC_CHECK5), (nBits <= 16) ? TRUE : FALSE );
    ::EnableWindow( ::GetDlgItem(m_hWnd, IDC_CHECK5), (nBits <= 24) ? TRUE : FALSE );
// -! NEW_FEATURE#0024
}


void CWaveConvert::UpdateDialog()
//-------------------------------
{
    CheckDlgButton(IDC_CHECK1, (m_dwFileLimit) ? MF_CHECKED : 0);
    CheckDlgButton(IDC_CHECK2, (m_dwSongLimit) ? MF_CHECKED : 0);
    ::EnableWindow(::GetDlgItem(m_hWnd, IDC_EDIT1), (m_dwFileLimit) ? TRUE : FALSE);
    ::EnableWindow(::GetDlgItem(m_hWnd, IDC_EDIT2), (m_dwSongLimit) ? TRUE : FALSE);
    BOOL bSel = IsDlgButtonChecked(IDC_RADIO2) ? TRUE : FALSE;
    m_EditMinOrder.EnableWindow(bSel);
    m_EditMaxOrder.EnableWindow(bSel);
}


void CWaveConvert::OnCheck1()
//---------------------------
{
    if (IsDlgButtonChecked(IDC_CHECK1))
    {
        m_dwFileLimit = GetDlgItemInt(IDC_EDIT1, NULL, FALSE);
        if (!m_dwFileLimit)
        {
            m_dwFileLimit = 1000;
            SetDlgItemText(IDC_EDIT1, "1000");
        }
    } else m_dwFileLimit = 0;
    UpdateDialog();
}

//rewbs.resamplerConf
void CWaveConvert::OnPlayerOptions()
//----------------------------------
{
    CMainFrame::m_nLastOptionsPage = 2;
    CMainFrame::GetMainFrame()->OnViewOptions();
}
//end rewbs.resamplerConf

void CWaveConvert::OnCheck2()
//---------------------------
{
    if (IsDlgButtonChecked(IDC_CHECK2))
    {
        m_dwSongLimit = GetDlgItemInt(IDC_EDIT2, NULL, FALSE);
        if (!m_dwSongLimit)
        {
            m_dwSongLimit = 600;
            SetDlgItemText(IDC_EDIT2, "600");
        }
    } else m_dwSongLimit = 0;
    UpdateDialog();
}


// Channel render is mutually exclusive with instrument render
void CWaveConvert::OnCheckChannelMode()
//-------------------------------------
{
    CheckDlgButton(IDC_CHECK6, MF_UNCHECKED);
}


// Channel render is mutually exclusive with instrument render
void CWaveConvert::OnCheckInstrMode()
//-----------------------------------
{
    CheckDlgButton(IDC_CHECK4, MF_UNCHECKED);
}


void CWaveConvert::OnOK()
//-----------------------
{
    if (m_dwFileLimit) m_dwFileLimit = GetDlgItemInt(IDC_EDIT1, NULL, FALSE);
    if (m_dwSongLimit) m_dwSongLimit = GetDlgItemInt(IDC_EDIT2, NULL, FALSE);
    m_bSelectPlay = IsDlgButtonChecked(IDC_RADIO2) ? true : false;
    m_nMinOrder = (modplug::tracker::orderindex_t)GetDlgItemInt(IDC_EDIT3, NULL, FALSE);
    m_nMaxOrder = (modplug::tracker::orderindex_t)GetDlgItemInt(IDC_EDIT4, NULL, FALSE);
    if (m_nMaxOrder < m_nMinOrder) m_bSelectPlay = false;
    //m_bHighQuality = IsDlgButtonChecked(IDC_CHECK3) ? true : false; //rewbs.resamplerConf - we don't want this anymore.
    m_bNormalize = IsDlgButtonChecked(IDC_CHECK5) ? true : false;
    m_bGivePlugsIdleTime = IsDlgButtonChecked(IDC_GIVEPLUGSIDLETIME) ? true : false;
    if (m_bGivePlugsIdleTime)
    {
        if (MessageBox("You only need slow render if you are experiencing dropped notes with a Kontakt based sampler with Direct-From-Disk enabled.\nIt will make rendering *very* slow.\n\nAre you sure you want to enable slow render?",
            "Really enable slow render?", MB_YESNO) == IDNO )
        {
            CheckDlgButton(IDC_GIVEPLUGSIDLETIME, BST_UNCHECKED);
            return;
        }
    }

// -> CODE#0024
// -> DESC="wav export update"
    m_bChannelMode = IsDlgButtonChecked(IDC_CHECK4) ? true : false;
// -! NEW_FEATURE#0024
    m_bInstrumentMode= IsDlgButtonChecked(IDC_CHECK6) ? true : false;

    // WaveFormatEx
    uint32_t dwFormat = m_CbnSampleFormat.GetItemData(m_CbnSampleFormat.GetCurSel());
    WaveFormat.Format.wFormatTag = WAVE_FORMAT_PCM;
    WaveFormat.Format.nSamplesPerSec = m_CbnSampleRate.GetItemData(m_CbnSampleRate.GetCurSel());
    if (WaveFormat.Format.nSamplesPerSec < 11025) WaveFormat.Format.nSamplesPerSec = 11025;
    if (WaveFormat.Format.nSamplesPerSec > MAX_SAMPLE_RATE) WaveFormat.Format.nSamplesPerSec = MAX_SAMPLE_RATE;
    WaveFormat.Format.nChannels = (uint16_t)(dwFormat >> 8);
    if ((WaveFormat.Format.nChannels != 1) && (WaveFormat.Format.nChannels != 4)) WaveFormat.Format.nChannels = 2;
    WaveFormat.Format.wBitsPerSample = (uint16_t)(dwFormat & 0xFF);

// -> CODE#0024
// -> DESC="wav export update"
//    if ((WaveFormat.Format.wBitsPerSample != 8) && (WaveFormat.Format.wBitsPerSample != 32)) WaveFormat.Format.wBitsPerSample = 16;
    if ((WaveFormat.Format.wBitsPerSample != 8) && (WaveFormat.Format.wBitsPerSample != 24) && (WaveFormat.Format.wBitsPerSample != 32)) WaveFormat.Format.wBitsPerSample = 16;
// -! NEW_FEATURE#0024

    WaveFormat.Format.nBlockAlign = (WaveFormat.Format.wBitsPerSample * WaveFormat.Format.nChannels) / 8;
    WaveFormat.Format.nAvgBytesPerSec = WaveFormat.Format.nSamplesPerSec * WaveFormat.Format.nBlockAlign;
    WaveFormat.Format.cbSize = 0;
    // Mono/Stereo 32-bit floating-point
    if ((WaveFormat.Format.wBitsPerSample == 32) && (WaveFormat.Format.nChannels <= 2))
    {
        WaveFormat.Format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    } else
    // MultiChannel configuration
    if ((WaveFormat.Format.wBitsPerSample == 32) || (WaveFormat.Format.nChannels > 2))
    {
        const GUID guid_MEDIASUBTYPE_PCM = {0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x0, 0xAA, 0x0, 0x38, 0x9B, 0x71};
        WaveFormat.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        WaveFormat.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
        WaveFormat.Samples.wValidBitsPerSample = WaveFormat.Format.wBitsPerSample;
        switch(WaveFormat.Format.nChannels)
        {
        case 1:            WaveFormat.dwChannelMask = 0x0004; break; // FRONT_CENTER
        case 2:            WaveFormat.dwChannelMask = 0x0003; break; // FRONT_LEFT | FRONT_RIGHT
        case 3:            WaveFormat.dwChannelMask = 0x0103; break; // FRONT_LEFT|FRONT_RIGHT|BACK_CENTER
        case 4:            WaveFormat.dwChannelMask = 0x0033; break; // FRONT_LEFT|FRONT_RIGHT|BACK_LEFT|BACK_RIGHT
        default:    WaveFormat.dwChannelMask = 0; break;
        }
        WaveFormat.SubFormat = guid_MEDIASUBTYPE_PCM;
    }
    CDialog::OnOK();
}



/////////////////////////////////////////////////////////////////////////////////////////
// CDoWaveConvert: save a mod as a wave file

BEGIN_MESSAGE_MAP(CDoWaveConvert, CDialog)
    ON_COMMAND(IDC_BUTTON1,    OnButton1)
END_MESSAGE_MAP()


BOOL CDoWaveConvert::OnInitDialog()
//---------------------------------
{
    CDialog::OnInitDialog();
    PostMessage(WM_COMMAND, IDC_BUTTON1);
    return TRUE;
}

// -> CODE#0024
// -> DESC="wav export update"
//#define WAVECONVERTBUFSIZE    2048
#define WAVECONVERTBUFSIZE    modplug::mixgraph::MIX_BUFFER_SIZE //Going over modplug::mixgraph::MIX_BUFFER_SIZE can kill VSTPlugs
// -! NEW_FEATURE#0024


void CDoWaveConvert::OnButton1()
//------------------------------
{
    //XXXih:  onbutton1          :   -   (
    FILE *f;
    WAVEFILEHEADER header;
    WAVEDATAHEADER datahdr, fmthdr;
    MSG msg;
    uint8_t buffer[WAVECONVERTBUFSIZE];
    CHAR s[80];
    HWND progress = ::GetDlgItem(m_hWnd, IDC_PROGRESS1);
    UINT ok = IDOK, pos;
    ULONGLONG ullSamples, ullMaxSamples;
    uint32_t dwDataOffset;
    LONG lMax = 256;

    if ((!m_pSndFile) || (!m_lpszFileName) || ((f = fopen(m_lpszFileName, "w+b")) == NULL))
    {
        ::AfxMessageBox("Could not open file for writing. Is it open in another application?");
        EndDialog(IDCANCEL);
        return;
    }
    SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
    int oldVol = m_pSndFile->GetMasterVolume();
    int nOldRepeat = m_pSndFile->GetRepeatCount();
    module_renderer::deprecated_global_sound_setup_bitmask |= SNDMIX_DIRECTTODISK;
    if ((!m_dwFileLimit) && (!m_dwSongLimit)) module_renderer::deprecated_global_sound_setup_bitmask |= SNDMIX_NOBACKWARDJUMPS;
    module_renderer::deprecated_global_mixing_freq = m_pWaveFormat->nSamplesPerSec;
    module_renderer::deprecated_global_bits_per_sample = m_pWaveFormat->wBitsPerSample;
    module_renderer::deprecated_global_channels = m_pWaveFormat->nChannels;
// -> CODE#0024
// -> DESC="wav export update"
//    if ((m_bNormalize) && (m_pWaveFormat->wBitsPerSample <= 16))
    if ((m_bNormalize) && (m_pWaveFormat->wBitsPerSample <= 24))
// -! NEW_FEATURE#0024
    {
        m_pSndFile->deprecated_global_bits_per_sample = 24;
        if (oldVol > 128) m_pSndFile->SetMasterVolume(128);
    } else
    {
        m_bNormalize = false;
    }
    m_pSndFile->ResetChannels();
    module_renderer::InitPlayer(TRUE);
    m_pSndFile->SetRepeatCount(0);
    if ((!m_dwFileLimit) || (m_dwFileLimit > 2047*1024)) m_dwFileLimit = 2047*1024; // 2GB
    m_dwFileLimit <<= 10;
    // File Header
    header.id_RIFF = IFFID_RIFF;
    header.filesize = sizeof(WAVEFILEHEADER) - 8;
    header.id_WAVE = IFFID_WAVE;
    // Wave Format Header
    fmthdr.id_data = IFFID_fmt;
    fmthdr.length = 16;
    if (m_pWaveFormat->cbSize) fmthdr.length += 2 + m_pWaveFormat->cbSize;
    header.filesize += sizeof(fmthdr) + fmthdr.length;
    // Data header
    datahdr.id_data = IFFID_data;
    datahdr.length = 0;
    header.filesize += sizeof(datahdr);
    // Writing Headers
    fwrite(&header, 1, sizeof(header), f);
    fwrite(&fmthdr, 1, sizeof(fmthdr), f);
    fwrite(m_pWaveFormat, 1, fmthdr.length, f);
    fwrite(&datahdr, 1, sizeof(datahdr), f);
    dwDataOffset = ftell(f);
    pos = 0;
    ullSamples = 0;
    ullMaxSamples = m_dwFileLimit / (m_pWaveFormat->nChannels * m_pWaveFormat->wBitsPerSample / 8);
    if (m_dwSongLimit)
    {
        ULONGLONG l = (ULONGLONG)m_dwSongLimit * m_pWaveFormat->nSamplesPerSec;
        if (l < ullMaxSamples) ullMaxSamples = l;
    }

    // calculate maximum samples
    ULONGLONG max = ullMaxSamples;
    ULONGLONG l = ((ULONGLONG)m_pSndFile->GetSongTime()) * m_pWaveFormat->nSamplesPerSec;
    if (m_nMaxPatterns > 0)
    {
        uint32_t dwOrds = m_pSndFile->Order.GetLengthFirstEmpty();
        if ((m_nMaxPatterns < dwOrds) && (dwOrds > 0)) l = (l*m_nMaxPatterns) / dwOrds;
    }

    if (l < max) max = l;

    if (progress != NULL)
    {
        ::SendMessage(progress, PBM_SETRANGE, 0, MAKELPARAM(0, (uint32_t)(max >> 14)));
    }

    // No pattern cue points yet
    m_pSndFile->m_PatternCuePoints.clear();
    m_pSndFile->m_PatternCuePoints.reserve(m_pSndFile->Order.GetLength());

    // Process the conversion
    UINT nBytesPerSample = (module_renderer::deprecated_global_bits_per_sample * module_renderer::deprecated_global_channels) / 8;
    // For calculating the remaining time
    uint32_t dwStartTime = timeGetTime();
    // For giving away some processing time every now and then
    uint32_t dwSleepTime = dwStartTime;

    CMainFrame::GetMainFrame()->InitRenderer(m_pSndFile);    //rewbs.VSTTimeInfo
    for (UINT n = 0; ; n++)
    {
        UINT lRead = m_pSndFile->ReadPattern(buffer, sizeof(buffer));

        // Process cue points (add base offset), if there are any to process.
        vector<PatternCuePoint>::reverse_iterator iter;
        for(iter = m_pSndFile->m_PatternCuePoints.rbegin(); iter != m_pSndFile->m_PatternCuePoints.rend(); ++iter)
        {
            if(iter->processed)
            {
                // From this point, all cues have already been processed.
                break;
            }
            iter->offset += ullSamples;
            iter->processed = true;
        }

/*            if (m_bGivePlugsIdleTime) {
            LARGE_INTEGER startTime, endTime, duration,Freq;
            QueryPerformanceFrequency(&Freq);
            long samplesprocessed = sizeof(buffer)/(m_pWaveFormat->nChannels * m_pWaveFormat->wBitsPerSample / 8);
            duration.QuadPart = samplesprocessed / static_cast<double>(m_pWaveFormat->nSamplesPerSec) * Freq.QuadPart;
            if (QueryPerformanceCounter(&startTime)) {
                endTime.QuadPart=0;
                while ((endTime.QuadPart-startTime.QuadPart)<duration.QuadPart*4) {
                    theApp.GetPluginManager()->OnIdle();
                    QueryPerformanceCounter(&endTime);
                }
            }
    }*/

        if (m_bGivePlugsIdleTime)
        {
            Sleep(20);
        }

        if (!lRead)
            break;
        ullSamples += lRead;
        if (m_bNormalize)
        {
            UINT imax = lRead*3*module_renderer::deprecated_global_channels;
            for (UINT i=0; i<imax; i+=3)
            {
                LONG l = ((((buffer[i+2] << 8) + buffer[i+1]) << 8) + buffer[i]) << 8;
                l /= 256;
                if (l > lMax) lMax = l;
                if (-l > lMax) lMax = -l;
            }
        } else
        if (m_pWaveFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
        {
            M2W_32ToFloat(buffer, lRead*(nBytesPerSample>>2));
        }

        UINT lWrite = fwrite(buffer, 1, lRead*nBytesPerSample, f);
        if (!lWrite)
            break;
        datahdr.length += lWrite;
        if (m_bNormalize)
        {
            ULONGLONG d = ((ULONGLONG)datahdr.length * m_pWaveFormat->wBitsPerSample) / 24;
            if (d >= m_dwFileLimit)
                break;
        } else
        {
            if (datahdr.length >= m_dwFileLimit)
                break;
        }
        if (ullSamples >= ullMaxSamples)
            break;
        if (!(n % 10))
        {
            uint32_t l = (uint32_t)(ullSamples / module_renderer::deprecated_global_mixing_freq);

            const uint32_t dwCurrentTime = timeGetTime();
            uint32_t timeRemaining = 0; // estimated remainig time
            if((ullSamples > 0) && (ullSamples < max))
            {
                timeRemaining = static_cast<uint32_t>(((dwCurrentTime - dwStartTime) * (max - ullSamples) / ullSamples) / 1000);
            }

            wsprintf(s, "Writing file... (%uKB, %umn%02us, %umn%02us remaining)", datahdr.length >> 10, l / 60, l % 60, timeRemaining / 60, timeRemaining % 60);
            SetDlgItemText(IDC_TEXT1, s);

            // Give windows some time to redraw the window, if necessary (else, the infamous "OpenMPT does not respond" will pop up)
            if ((!m_bGivePlugsIdleTime) && (dwCurrentTime > dwSleepTime + 1000))
            {
                Sleep(1);
                dwSleepTime = dwCurrentTime;
            }
        }
        if ((progress != NULL) && ((uint32_t)(ullSamples >> 14) != pos))
        {
            pos = (uint32_t)(ullSamples >> 14);
            ::SendMessage(progress, PBM_SETPOS, pos, 0);
        }
        if (::PeekMessage(&msg, m_hWnd, 0, 0, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }

        if (m_bAbort)
        {
            ok = IDCANCEL;
            break;
        }
    }
    CMainFrame::GetMainFrame()->StopRenderer(m_pSndFile);    //rewbs.VSTTimeInfo
    if (m_bNormalize)
    {
        uint32_t dwLength = datahdr.length;
        uint32_t percent = 0xFF, dwPos, dwSize, dwCount;
        uint32_t dwBitSize, dwOutPos;

        dwPos = dwOutPos = dwDataOffset;
        dwBitSize = m_pWaveFormat->wBitsPerSample / 8;
        datahdr.length = 0;
        ::SendMessage(progress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        dwCount = dwLength;
        while (dwCount >= 3)
        {
            dwSize = (sizeof(buffer) / 3) * 3;
            if (dwSize > dwCount) dwSize = dwCount;
            fseek(f, dwPos, SEEK_SET);
            if (fread(buffer, 1, dwSize, f) != dwSize) break;
            module_renderer::Normalize24BitBuffer(buffer, dwSize, lMax, dwBitSize);
            fseek(f, dwOutPos, SEEK_SET);
            datahdr.length += (dwSize/3)*dwBitSize;
            fwrite(buffer, 1, (dwSize/3)*dwBitSize, f);
            dwCount -= dwSize;
            dwPos += dwSize;
            dwOutPos += (dwSize * m_pWaveFormat->wBitsPerSample) / 24;
            UINT k = (UINT)( ((ULONGLONG)(dwLength - dwCount) * 100) / dwLength );
            if (k != percent)
            {
                percent = k;
                wsprintf(s, "Normalizing... (%d%%)", percent);
                SetDlgItemText(IDC_TEXT1, s);
                ::SendMessage(progress, PBM_SETPOS, percent, 0);
            }
        }
    }

    // Write cue points
    uint32_t cuePointLength = 0;
    if(m_pSndFile->m_PatternCuePoints.size() > 0)
    {
        // Cue point header
        WAVCUEHEADER cuehdr;
        cuehdr.cue_id = LittleEndian(IFFID_cue);
        cuehdr.cue_num = m_pSndFile->m_PatternCuePoints.size();
        cuehdr.cue_len = 4 + cuehdr.cue_num * sizeof(WAVCUEPOINT);
        cuePointLength = 8 + cuehdr.cue_len;
        cuehdr.cue_num = LittleEndian(cuehdr.cue_num);
        cuehdr.cue_len = LittleEndian(cuehdr.cue_len);
        fwrite(&cuehdr, 1, sizeof(WAVCUEHEADER), f);

        // Write all cue points
        vector<PatternCuePoint>::const_iterator iter;
        uint32_t num = 0;
        for(iter = m_pSndFile->m_PatternCuePoints.begin(); iter != m_pSndFile->m_PatternCuePoints.end(); ++iter, num++)
        {
            WAVCUEPOINT cuepoint;
            cuepoint.cp_id = LittleEndian(num);
            cuepoint.cp_pos = LittleEndian((uint32_t)iter->offset);
            cuepoint.cp_chunkid = LittleEndian(IFFID_data);
            cuepoint.cp_chunkstart = 0;            // we use no Wave List Chunk (wavl) as we have only one data block, so this should be 0.
            cuepoint.cp_blockstart = 0;            // dito
            cuepoint.cp_offset = LittleEndian((uint32_t)iter->offset);
            fwrite(&cuepoint, 1, sizeof(WAVCUEPOINT), f);
        }
        m_pSndFile->m_PatternCuePoints.clear();
    }

    header.filesize = (sizeof(WAVEFILEHEADER) - 8) + (8 + fmthdr.length) + (8 + datahdr.length) + (cuePointLength);
    fseek(f, 0, SEEK_SET);
    fwrite(&header, sizeof(header), 1, f);
    fseek(f, dwDataOffset-sizeof(datahdr), SEEK_SET);
    fwrite(&datahdr, sizeof(datahdr), 1, f);
    fclose(f);
    module_renderer::deprecated_global_sound_setup_bitmask &= ~(SNDMIX_DIRECTTODISK|SNDMIX_NOBACKWARDJUMPS);
    m_pSndFile->SetRepeatCount(nOldRepeat);
    m_pSndFile->m_nMaxOrderPosition = 0;
    if (m_bNormalize)
    {
        m_pSndFile->SetMasterVolume(oldVol);
        CFile fw;
        if (fw.Open(m_lpszFileName, CFile::modeReadWrite | CFile::modeNoTruncate))
        {
            fw.SetLength(header.filesize+8);
            fw.Close();
        }
    }

    //XXXih: CMainFrame::GetMainFrame()->UpdateAudioParameters(TRUE);

//rewbs: reduce to normal priority during debug for easier hang debugging
    //SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
    EndDialog(ok);
}


/*

TEXT("IARL"), TEXT("Archival Location"),  TEXT("Indicates where the subject of the file is archived."),
TEXT("IART"), TEXT("Artist"),             TEXT("Lists the artist of the original subject of the file. For example, \"Michaelangelo.\""),
TEXT("ICMS"), TEXT("Commissioned"),       TEXT("Lists the name of the person or organization that commissioned the subject of the file."
                                            " For example, \"Pope Julian II.\""),
TEXT("ICMT"), TEXT("Comments"),           TEXT("Provides general comments about the file or the subject of the file. If the comment is "
                                            "several sentences long, end each sentence with a period. Do not include newline characters."),
TEXT("ICOP"), TEXT("Copyright"),          TEXT("Records the copyright information for the file. For example, \"Copyright Encyclopedia "
                                            "International 1991.\" If there are multiple copyrights, separate them by a semicolon followed "
                                            "by a space."),
TEXT("ICRD"), TEXT("Creation date"),      TEXT("Specifies the date the subject of the file was created. List dates in year-month-day "
                                            "format, padding one-digit months and days with a zero on the left. For example, "
                                            "'1553-05-03' for May 3, 1553."),
TEXT("IENG"), TEXT("Engineer"),           TEXT("Stores the name of the engineer who worked on the file. If there are multiple engineers, "
                                            "separate the names by a semicolon and a blank. For example, \"Smith, John; Adams, Joe.\""),
TEXT("IGNR"), TEXT("Genre"),              TEXT("Describes the original work, such as, \"landscape,\" \"portrait,\" \"still life,\" etc."),
TEXT("IKEY"), TEXT("Keywords"),           TEXT("Provides a list of keywords that refer to the file or subject of the file. Separate "
                                            "multiple keywords with a semicolon and a blank. For example, \"Seattle; aerial view; scenery.\""),
TEXT("IMED"), TEXT("Medium"),             TEXT("Describes the original subject of the file, such as, \"computer image,\" \"drawing,\""
                                            "\"lithograph,\" and so forth."),
TEXT("INAM"), TEXT("Name"),               TEXT("Stores the title of the subject of the file, such as, \"Seattle From Above.\""),
TEXT("IPRD"), TEXT("Product"),            TEXT("Specifies the name of the title the file was originally intended for, such as \"Encyclopedia"
                                            " of Pacific Northwest Geography.\""),
TEXT("ISBJ"), TEXT("Subject"),            TEXT("Describes the contents of the file, such as \"Aerial view of Seattle.\""),
TEXT("ISFT"), TEXT("Software"),           TEXT("Identifies the name of the software package used to create the file, such as "
                                            "\"Microsoft WaveEdit.\""),
TEXT("ISRC"), TEXT("Source"),             TEXT("Identifies the name of the person or organization who supplied the original subject of the file."
                                            " For example, \"Trey Research.\""),
TEXT("ITCH"), TEXT("Technician"),         TEXT("Identifies the technician who digitized the subject file. For example, \"Smith, John.\""),

*/