/*
 * OpenMPT
 *
 * Sndfile.cpp
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
 *          OpenMPT devs
*/

#include "stdafx.h"

#include "../serializers/wao.hpp"

#include "../mainfrm.h"
#include "../moddoc.h"
#include "../version.h"
#include "../serialization_utils.h"
#include "sndfile.h"
#include "wavConverter.h"
#include "tuningcollection.h"
#include "../mixgraph/constants.hpp"
#include <vector>
#include <list>
#include <algorithm>
#include <memory>

#include "../pervasives/binaryparse.hpp"
#include "../serializers/xm.hpp"
#include "../serializers/it.hpp"

#include "../tracker/eval.hpp"

#define str_SampleAllocationError (GetStrI18N(_TEXT("Sample allocation error")))
#define str_Error                 (GetStrI18N(_TEXT("Error")))

#ifndef NO_MMCMP_SUPPORT
#define MMCMP_SUPPORT
#endif // NO_MMCMP_SUPPORT
#ifndef NO_ARCHIVE_SUPPORT
#define UNRAR_SUPPORT
#define UNLHA_SUPPORT
#define UNGZIP_SUPPORT
#define ZIPPED_MOD_SUPPORT
LPCSTR glpszModExtensions = "mod|s3m|xm|it|stm|nst|ult|669|wow|mtm|med|far|mdl|ams|dsm|amf|okt|dmf|ptm|psm|mt2|umx|gdm|imf|j2b"
#ifndef NO_UNMO3_SUPPORT
"|mo3"
#endif
;
//Should there be mptm?
#endif // NO_ARCHIVE_SUPPORT
#ifdef ZIPPED_MOD_SUPPORT
#include "unzip32.h"
#endif

#ifdef UNRAR_SUPPORT
#include "unrar32.h"
#endif

#ifdef UNLHA_SUPPORT
#include "../unlha/unlha32.h"
#endif

#ifdef UNGZIP_SUPPORT
#include "../ungzip/ungzip.h"
#endif

using namespace modplug::tracker;
using namespace modplug::pervasives;

// External decompressors
extern uint32_t ITReadBits(uint32_t &bitbuf, UINT &bitnum, LPBYTE &ibuf, CHAR n);
extern void ITUnpack8Bit(LPSTR pSample, uint32_t dwLen, LPBYTE lpMemFile, uint32_t dwMemLength, BOOL b215);
extern void ITUnpack16Bit(LPSTR pSample, uint32_t dwLen, LPBYTE lpMemFile, uint32_t dwMemLength, BOOL b215);

#define MAX_PACK_TABLES            3


// Compression table
static char UnpackTable[MAX_PACK_TABLES][16] =
//--------------------------------------------
{
    // CPU-generated dynamic table
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    // u-Law table
    0, 1, 2, 4, 8, 16, 32, 64,
    -1, -2, -4, -8, -16, -32, -48, -64,
    // Linear table
    0, 1, 2, 3, 5, 7, 12, 19,
    -1, -2, -3, -5, -7, -12, -19, -31,
};

// --------------------------------------------------------------------------------------------
// Convenient macro to help WRITE_HEADER declaration for single type members ONLY (non-array)
// --------------------------------------------------------------------------------------------
#define WRITE_MPTHEADER_sized_member(name,type,code) \
static_assert(sizeof(input->name) >= sizeof(type), "");\
fcode = #@code;\
fwrite(& fcode , 1 , sizeof( __int32 ) , file);\
fsize = sizeof( type );\
fwrite(& fsize , 1 , sizeof( __int16 ) , file);\
fwrite(&input-> name , 1 , fsize , file);

// --------------------------------------------------------------------------------------------
// Convenient macro to help WRITE_HEADER declaration for array members ONLY
// --------------------------------------------------------------------------------------------
#define WRITE_MPTHEADER_array_member(name,type,code,arraysize) \
ASSERT(sizeof(input->name) >= sizeof(type) * arraysize);\
fcode = #@code;\
fwrite(& fcode , 1 , sizeof( __int32 ) , file);\
fsize = sizeof( type ) * arraysize;\
fwrite(& fsize , 1 , sizeof( __int16 ) , file);\
fwrite(&input-> name , 1 , fsize , file);

namespace {
// Create 'dF..' entry.
uint32_t CreateExtensionFlags(const modplug::tracker::modinstrument_t& ins)
//--------------------------------------------------
{
    uint32_t dwFlags = 0;
    if (ins.volume_envelope.flags & ENV_ENABLED)    dwFlags |= dFdd_VOLUME;
    if (ins.volume_envelope.flags & ENV_SUSTAIN)    dwFlags |= dFdd_VOLSUSTAIN;
    if (ins.volume_envelope.flags & ENV_LOOP)            dwFlags |= dFdd_VOLLOOP;
    if (ins.panning_envelope.flags & ENV_ENABLED)    dwFlags |= dFdd_PANNING;
    if (ins.panning_envelope.flags & ENV_SUSTAIN)    dwFlags |= dFdd_PANSUSTAIN;
    if (ins.panning_envelope.flags & ENV_LOOP)            dwFlags |= dFdd_PANLOOP;
    if (ins.pitch_envelope.flags & ENV_ENABLED)    dwFlags |= dFdd_PITCH;
    if (ins.pitch_envelope.flags & ENV_SUSTAIN)    dwFlags |= dFdd_PITCHSUSTAIN;
    if (ins.pitch_envelope.flags & ENV_LOOP)    dwFlags |= dFdd_PITCHLOOP;
    if (ins.flags & INS_SETPANNING)            dwFlags |= dFdd_SETPANNING;
    if (ins.pitch_envelope.flags & ENV_FILTER)    dwFlags |= dFdd_FILTER;
    if (ins.volume_envelope.flags & ENV_CARRY)            dwFlags |= dFdd_VOLCARRY;
    if (ins.panning_envelope.flags & ENV_CARRY)            dwFlags |= dFdd_PANCARRY;
    if (ins.pitch_envelope.flags & ENV_CARRY)    dwFlags |= dFdd_PITCHCARRY;
    if (ins.flags & INS_MUTE)                            dwFlags |= dFdd_MUTE;
    return dwFlags;
}
} // unnamed namespace.

// Write (in 'file') 'input' modplug::tracker::modinstrument_t with 'code' & 'size' extra field infos for each member
void WriteInstrumentHeaderStruct(modplug::tracker::modinstrument_t * input, FILE * file)
{
__int32 fcode;
__int16 fsize;
WRITE_MPTHEADER_sized_member(    fadeout, UINT                        , FO..                                                        )

{ // dwFlags needs to be constructed so write it manually.
    //WRITE_MPTHEADER_sized_member(    dwFlags                                        , uint32_t                        , dF..                                                        )
    const uint32_t dwFlags = CreateExtensionFlags(*input);
    fcode = 'dF..';
    fwrite(&fcode, 1, sizeof(int32_t), file);
    fsize = sizeof(dwFlags);
    fwrite(&fsize, 1, sizeof(int16_t), file);
    fwrite(&dwFlags, 1, fsize, file);
}

WRITE_MPTHEADER_sized_member( global_volume                   , UINT         , GV..                                         )
WRITE_MPTHEADER_sized_member( default_pan                     , UINT         , P...                                         )
WRITE_MPTHEADER_sized_member( volume_envelope.num_nodes       , UINT         , VE..                                         )
WRITE_MPTHEADER_sized_member( panning_envelope.num_nodes      , UINT         , PE..                                         )
WRITE_MPTHEADER_sized_member( pitch_envelope.num_nodes        , UINT         , PiE.                                         )
WRITE_MPTHEADER_sized_member( volume_envelope.loop_start      , uint8_t      , VLS.                                         )
WRITE_MPTHEADER_sized_member( volume_envelope.loop_end        , uint8_t      , VLE.                                         )
WRITE_MPTHEADER_sized_member( volume_envelope.sustain_start   , uint8_t      , VSB.                                         )
WRITE_MPTHEADER_sized_member( volume_envelope.sustain_end     , uint8_t      , VSE.                                         )
WRITE_MPTHEADER_sized_member( panning_envelope.loop_start     , uint8_t      , PLS.                                         )
WRITE_MPTHEADER_sized_member( panning_envelope.loop_end       , uint8_t      , PLE.                                         )
WRITE_MPTHEADER_sized_member( panning_envelope.sustain_start  , uint8_t      , PSB.                                         )
WRITE_MPTHEADER_sized_member( panning_envelope.sustain_end    , uint8_t      , PSE.                                         )
WRITE_MPTHEADER_sized_member( pitch_envelope.loop_start       , uint8_t      , PiLS                                         )
WRITE_MPTHEADER_sized_member( pitch_envelope.loop_end         , uint8_t      , PiLE                                         )
WRITE_MPTHEADER_sized_member( pitch_envelope.sustain_start    , uint8_t      , PiSB                                         )
WRITE_MPTHEADER_sized_member( pitch_envelope.sustain_end      , uint8_t      , PiSE                                         )
WRITE_MPTHEADER_sized_member( new_note_action                 , uint8_t      , NNA.                                         )
WRITE_MPTHEADER_sized_member( duplicate_check_type            , uint8_t      , DCT.                                         )
WRITE_MPTHEADER_sized_member( duplicate_note_action           , uint8_t      , DNA.                                         )
WRITE_MPTHEADER_sized_member( random_pan_weight               , uint8_t      , PS..                                         )
WRITE_MPTHEADER_sized_member( random_volume_weight            , uint8_t      , VS..                                         )
WRITE_MPTHEADER_sized_member( default_filter_cutoff           , uint8_t      , IFC.                                         )
WRITE_MPTHEADER_sized_member( default_filter_resonance        , uint8_t      , IFR.                                         )
WRITE_MPTHEADER_sized_member( midi_bank                       , uint16_t     , MB..                                         )
WRITE_MPTHEADER_sized_member( midi_program                    , uint8_t      , MP..                                         )
WRITE_MPTHEADER_sized_member( midi_channel                    , uint8_t      , MC..                                         )
WRITE_MPTHEADER_sized_member( midi_drum_set                   , uint8_t      , MDK.                                         )
WRITE_MPTHEADER_sized_member( pitch_pan_separation            , signed char  , PPS.                                         )
WRITE_MPTHEADER_sized_member( pitch_pan_center                , unsigned char, PPC.                                         )
WRITE_MPTHEADER_array_member( volume_envelope.Ticks           , uint16_t     , VP[. , ((input->volume_envelope.num_nodes > 32) ? MAX_ENVPOINTS : 32))
WRITE_MPTHEADER_array_member( panning_envelope.Ticks          , uint16_t     , PP[. , ((input->panning_envelope.num_nodes > 32) ? MAX_ENVPOINTS : 32))
WRITE_MPTHEADER_array_member( pitch_envelope.Ticks            , uint16_t     , PiP[ , ((input->pitch_envelope.num_nodes > 32) ? MAX_ENVPOINTS : 32))
WRITE_MPTHEADER_array_member( volume_envelope.Values          , uint8_t      , VE[. , ((input->volume_envelope.num_nodes > 32) ? MAX_ENVPOINTS : 32))
WRITE_MPTHEADER_array_member( panning_envelope.Values         , uint8_t      , PE[. , ((input->panning_envelope.num_nodes > 32) ? MAX_ENVPOINTS : 32))
WRITE_MPTHEADER_array_member( pitch_envelope.Values           , uint8_t      , PiE[ , ((input->pitch_envelope.num_nodes > 32) ? MAX_ENVPOINTS : 32))
WRITE_MPTHEADER_array_member( NoteMap                         , uint8_t      , NM[. , 128                                )
WRITE_MPTHEADER_array_member( Keyboard                        , uint16_t     , K[.. , 128                                )
WRITE_MPTHEADER_array_member( name                            , CHAR         , n[.. , 32                                )
WRITE_MPTHEADER_array_member( legacy_filename                 , CHAR         , fn[. , 12                                )
WRITE_MPTHEADER_sized_member( nMixPlug                        , uint8_t      , MiP.                                         )
WRITE_MPTHEADER_sized_member( volume_ramp_up                  , USHORT       , VR..                                         )
WRITE_MPTHEADER_sized_member( volume_ramp_down                , USHORT       , VRD.                                         )
WRITE_MPTHEADER_sized_member( resampling_mode                 , USHORT       , R...                                         )
WRITE_MPTHEADER_sized_member( random_cutoff_weight            , uint8_t      , CS..                                         )
WRITE_MPTHEADER_sized_member( random_resonance_weight         , uint8_t      , RS..                                         )
WRITE_MPTHEADER_sized_member( default_filter_mode             , uint8_t      , FM..                                         )
WRITE_MPTHEADER_sized_member( nPluginVelocityHandling         , uint8_t      , PVEH                                         )
WRITE_MPTHEADER_sized_member( nPluginVolumeHandling           , uint8_t      , PVOH                                         )
WRITE_MPTHEADER_sized_member( pitch_to_tempo_lock             , uint16_t     , PTTL                                         )
WRITE_MPTHEADER_sized_member( pitch_envelope.release_node     , uint8_t      , PERN                                         )
WRITE_MPTHEADER_sized_member( panning_envelope.release_node   , uint8_t      , AERN                                         )
WRITE_MPTHEADER_sized_member( volume_envelope.release_node    , uint8_t      , VERN                                         )
WRITE_MPTHEADER_sized_member( pitch_envelope.flags            , uint32_t     , PFLG                                         )
WRITE_MPTHEADER_sized_member( panning_envelope.flags          , uint32_t     , AFLG                                         )
WRITE_MPTHEADER_sized_member( volume_envelope.flags           , uint32_t     , VFLG                                         )
}

// --------------------------------------------------------------------------------------------
// Convenient macro to help GET_HEADER declaration for single type members ONLY (non-array)
// --------------------------------------------------------------------------------------------
#define GET_MPTHEADER_sized_member(name,type,code) \
case( #@code ):\
if( fsize <= sizeof( type ) ) pointer = (uint8_t *)&input-> name ;\
break;

// --------------------------------------------------------------------------------------------
// Convenient macro to help GET_HEADER declaration for array members ONLY
// --------------------------------------------------------------------------------------------
#define GET_MPTHEADER_array_member(name,type,code,arraysize) \
case( #@code ):\
if( fsize <= sizeof( type ) * arraysize ) pointer = (uint8_t *)&input-> name ;\
break;

// Return a pointer on the wanted field in 'input' modplug::tracker::modinstrument_t given field code & size
uint8_t * GetInstrumentHeaderFieldPointer(modplug::tracker::modinstrument_t * input, __int32 fcode, __int16 fsize)
{
if(input == NULL) return NULL;
uint8_t * pointer = NULL;

switch(fcode){
GET_MPTHEADER_sized_member( fadeout                        , UINT               , FO..                                 )
GET_MPTHEADER_sized_member( flags                          , uint32_t           , dF..                                 )
GET_MPTHEADER_sized_member( global_volume                  , UINT               , GV..                                 )
GET_MPTHEADER_sized_member( default_pan                    , UINT               , P...                                 )
GET_MPTHEADER_sized_member( volume_envelope.num_nodes      , UINT               , VE..                                 )
GET_MPTHEADER_sized_member( panning_envelope.num_nodes     , UINT               , PE..                                 )
GET_MPTHEADER_sized_member( pitch_envelope.num_nodes       , UINT               , PiE.                                 )

GET_MPTHEADER_sized_member( volume_envelope.loop_start     , uint8_t            , VLS.                                 )
GET_MPTHEADER_sized_member( volume_envelope.loop_end       , uint8_t            , VLE.                                 )
GET_MPTHEADER_sized_member( volume_envelope.sustain_start  , uint8_t            , VSB.                                 )
GET_MPTHEADER_sized_member( volume_envelope.sustain_end    , uint8_t            , VSE.                                 )

GET_MPTHEADER_sized_member( panning_envelope.loop_start    , uint8_t            , PLS.                                 )
GET_MPTHEADER_sized_member( panning_envelope.loop_end      , uint8_t            , PLE.                                 )
GET_MPTHEADER_sized_member( panning_envelope.sustain_start , uint8_t            , PSB.                                 )
GET_MPTHEADER_sized_member( panning_envelope.sustain_end   , uint8_t            , PSE.                                 )

GET_MPTHEADER_sized_member( pitch_envelope.loop_start      , uint8_t            , PiLS                                 )
GET_MPTHEADER_sized_member( pitch_envelope.loop_end        , uint8_t            , PiLE                                 )
GET_MPTHEADER_sized_member( pitch_envelope.sustain_start   , uint8_t            , PiSB                                 )
GET_MPTHEADER_sized_member( pitch_envelope.sustain_end     , uint8_t            , PiSE                                 )

GET_MPTHEADER_sized_member( new_note_action                , uint8_t            , NNA.                                 )
GET_MPTHEADER_sized_member( duplicate_check_type           , uint8_t            , DCT.                                 )
GET_MPTHEADER_sized_member( duplicate_note_action          , uint8_t            , DNA.                                 )
GET_MPTHEADER_sized_member( random_pan_weight              , uint8_t            , PS..                                 )
GET_MPTHEADER_sized_member( random_volume_weight           , uint8_t            , VS..                                 )
GET_MPTHEADER_sized_member( default_filter_cutoff          , uint8_t            , IFC.                                 )
GET_MPTHEADER_sized_member( default_filter_resonance       , uint8_t            , IFR.                                 )

GET_MPTHEADER_sized_member( midi_bank                      , uint16_t           , MB..                                 )
GET_MPTHEADER_sized_member( midi_program                   , uint8_t            , MP..                                 )
GET_MPTHEADER_sized_member( midi_channel                   , uint8_t            , MC..                                 )
GET_MPTHEADER_sized_member( midi_drum_set                  , uint8_t            , MDK.                                 )

GET_MPTHEADER_sized_member( pitch_pan_separation           , signed char        , PPS.                                 )
GET_MPTHEADER_sized_member( pitch_pan_center               , unsigned char      , PPC.                                 )

GET_MPTHEADER_array_member( volume_envelope.Ticks          , uint16_t           , VP[.                , MAX_ENVPOINTS  )
GET_MPTHEADER_array_member( panning_envelope.Ticks         , uint16_t           , PP[.                , MAX_ENVPOINTS  )
GET_MPTHEADER_array_member( pitch_envelope.Ticks           , uint16_t           , PiP[                , MAX_ENVPOINTS  )
GET_MPTHEADER_array_member( volume_envelope.Values         , uint8_t            , VE[.                , MAX_ENVPOINTS  )
GET_MPTHEADER_array_member( panning_envelope.Values        , uint8_t            , PE[.                , MAX_ENVPOINTS  )
GET_MPTHEADER_array_member( pitch_envelope.Values          , uint8_t            , PiE[                , MAX_ENVPOINTS  )

GET_MPTHEADER_array_member( NoteMap                        , uint8_t            , NM[.                , 128            )
GET_MPTHEADER_array_member( Keyboard                       , uint16_t           , K[..                , 128            )
GET_MPTHEADER_array_member( name                           , CHAR               , n[..                , 32             )
GET_MPTHEADER_array_member( legacy_filename                , CHAR               , fn[.                , 12             )

GET_MPTHEADER_sized_member( nMixPlug                       , uint8_t            , MiP.                                 )
GET_MPTHEADER_sized_member( volume_ramp_up                 , USHORT             , VR..                                 )
GET_MPTHEADER_sized_member( volume_ramp_down               , USHORT             , VRD.                                 )
GET_MPTHEADER_sized_member( resampling_mode                , UINT               , R...                                 )

GET_MPTHEADER_sized_member( random_cutoff_weight           , uint8_t            , CS..                                 )
GET_MPTHEADER_sized_member( random_resonance_weight        , uint8_t            , RS..                                 )
GET_MPTHEADER_sized_member( default_filter_mode            , uint8_t            , FM..                                 )

GET_MPTHEADER_sized_member( pitch_to_tempo_lock            , uint16_t           , PTTL                                 )
GET_MPTHEADER_sized_member( nPluginVelocityHandling        , uint8_t            , PVEH                                 )
GET_MPTHEADER_sized_member( nPluginVolumeHandling          , uint8_t            , PVOH                                 )

GET_MPTHEADER_sized_member( pitch_envelope.release_node    , uint8_t            , PERN                                 )
GET_MPTHEADER_sized_member( panning_envelope.release_node  , uint8_t            , AERN                                 )
GET_MPTHEADER_sized_member( volume_envelope.release_node   , uint8_t            , VERN                                 )

GET_MPTHEADER_sized_member( pitch_envelope.flags           , uint32_t           , PFLG                                 )
GET_MPTHEADER_sized_member( panning_envelope.flags         , uint32_t           , AFLG                                 )
GET_MPTHEADER_sized_member( volume_envelope.flags          , uint32_t           , VFLG                                 )
}

return pointer;
}

// -! NEW_FEATURE#0027


CTuning* modplug::tracker::modinstrument_t::s_DefaultTuning = 0;


//////////////////////////////////////////////////////////
// CSoundFile

CTuningCollection* module_renderer::s_pTuningsSharedBuiltIn(0);
CTuningCollection* module_renderer::s_pTuningsSharedLocal(0);
uint8_t module_renderer::s_DefaultPlugVolumeHandling = PLUGIN_VOLUMEHANDLING_IGNORE;

#pragma warning(disable : 4355) // "'this' : used in base member initializer list"
module_renderer::module_renderer()
    : Patterns(*this)
    , Order(*this)
    , m_PlaybackEventer(*this)
    , m_pModSpecs(&ModSpecs::itEx)
    , m_MIDIMapper(*this)
    , eval_state(new eval_state_ty)
#pragma warning(default : 4355) // "'this' : used in base member initializer list"
//----------------------
{
    m_nType = MOD_TYPE_NONE;
    m_dwSongFlags = 0;
    m_nChannels = 0;
    m_nMixChannels = 0;
    m_nSamples = 0;
    m_nInstruments = 0;
    m_lpszSongComments = nullptr;
    m_nMasterVolume = 128;
    m_nMinPeriod = MIN_PERIOD;
    m_nMaxPeriod = 0x7FFF;
    m_nRepeatCount = 0;
    m_nSeqOverride = 0;
    m_bPatternTransitionOccurred = false;
    m_nDefaultRowsPerBeat = m_nCurrentRowsPerBeat = (CMainFrame::m_nRowSpacing2) ? CMainFrame::m_nRowSpacing2 : 4;
    m_nDefaultRowsPerMeasure = m_nCurrentRowsPerMeasure = (CMainFrame::m_nRowSpacing >= m_nDefaultRowsPerBeat) ? CMainFrame::m_nRowSpacing : m_nDefaultRowsPerBeat * 4;
    m_nTempoMode = tempo_mode_classic;
    m_bIsRendering = false;

    m_ModFlags = 0;
    m_bITBidiMode = false;

    m_pModDoc = NULL;
    m_dwLastSavedWithVersion=0;
    m_dwCreatedWithVersion=0;
    MemsetZero(m_bChannelMuteTogglePending);

    MemsetZero(Chn);
    MemsetZero(ChnMix);
    MemsetZero(Samples);
    MemsetZero(ChnSettings);
    MemsetZero(Instruments);
    MemsetZero(m_szNames);
    MemsetZero(m_MixPlugins);
    Order.Init();
    Patterns.ClearPatterns();
    m_lTotalSampleCount=0;

    m_pConfig = new CSoundFilePlayConfig();
    m_pTuningsTuneSpecific = new CTuningCollection("Tune specific tunings");

    BuildDefaultInstrument();

    #pragma region InitEvalState
    eval_state->patterns = nullptr;
    eval_state->pos = default_pos;

    eval_state->tempo = 120;
    eval_state->ticks_per_row = 3;
    eval_state->ticks_per_row = 8;

    eval_state->mixing_freq = 44100;
    eval_state->tick_width = empty_tick_width;
    eval_state->tempo_mode = tempo_mode_ty::Classic;
    eval_state->channels = &this->Chn;
    eval_state->mixgraph = &this->mixgraph;
    #pragma endregion InitEvalState
}


module_renderer::~module_renderer()
//-----------------------
{
    delete m_pConfig;
    delete m_pTuningsTuneSpecific;
    Destroy();
}


BOOL module_renderer::Create(const uint8_t * lpStream, CModDoc *pModDoc, uint32_t dwMemLength)
//----------------------------------------------------------------------------
{
    m_pModDoc = pModDoc;
    m_nType = MOD_TYPE_NONE;
    m_dwSongFlags = 0;
    m_nChannels = 0;
    m_nMixChannels = 0;
    m_nSamples = 0;
    m_nInstruments = 0;
    m_nMasterVolume = 128;
    m_nDefaultGlobalVolume = MAX_GLOBAL_VOLUME;
    m_nGlobalVolume = MAX_GLOBAL_VOLUME;
    m_nOldGlbVolSlide = 0;
    m_nDefaultSpeed = 6;
    m_nDefaultTempo = 125;
    m_nPatternDelay = 0;
    m_nFrameDelay = 0;
    m_nNextRow = 0;
    m_nRow = 0;
    m_nPattern = 0;
    m_nCurrentPattern = 0;
    m_nNextPattern = 0;
    m_nNextPatStartRow = 0;
    m_nSeqOverride = 0;
    m_nRestartPos = 0;
    m_nMinPeriod = 16;
    m_nMaxPeriod = 32767;
    m_nSamplePreAmp = 48;
    m_nVSTiVolume = 48;
    m_nMaxOrderPosition = 0;
    m_lpszSongComments = nullptr;
    m_nMixLevels = mixLevels_compatible;    // Will be overridden if appropriate.
    MemsetZero(Samples);
    MemsetZero(ChnMix);
    MemsetZero(Chn);
    MemsetZero(Instruments);
    MemsetZero(m_szNames);
    MemsetZero(m_MixPlugins);
    //Order.assign(MAX_ORDERS, Order.GetInvalidPatIndex());
    Order.resize(1);
    Patterns.ClearPatterns();
    ResetMidiCfg();

    for (modplug::tracker::chnindex_t nChn = 0; nChn < MAX_BASECHANNELS; nChn++)
    {
        InitChannel(nChn);
    }
    if (lpStream)
    {
#ifdef ZIPPED_MOD_SUPPORT
        CZipArchive archive(glpszModExtensions);
        if (CZipArchive::IsArchive((LPBYTE)lpStream, dwMemLength))
        {
            if (archive.UnzipArchive((LPBYTE)lpStream, dwMemLength) && archive.GetOutputFile())
            {
                lpStream = archive.GetOutputFile();
                dwMemLength = archive.GetOutputFileLength();
            }
        }
#endif
#ifdef UNRAR_SUPPORT
        CRarArchive unrar((LPBYTE)lpStream, dwMemLength, glpszModExtensions);
        if (unrar.IsArchive())
        {
            if (unrar.ExtrFile() && unrar.GetOutputFile())
            {
                lpStream = unrar.GetOutputFile();
                dwMemLength = unrar.GetOutputFileLength();
            }
        }
#endif
#ifdef UNLHA_SUPPORT
        CLhaArchive unlha((LPBYTE)lpStream, dwMemLength, glpszModExtensions);
        if (unlha.IsArchive())
        {
            if (unlha.ExtractFile() && unlha.GetOutputFile())
            {
                lpStream = unlha.GetOutputFile();
                dwMemLength = unlha.GetOutputFileLength();
            }
        }
#endif
#ifdef UNGZIP_SUPPORT
        CGzipArchive ungzip((LPBYTE)lpStream, dwMemLength);
        if (ungzip.IsArchive())
        {
            if (ungzip.ExtractFile() && ungzip.GetOutputFile())
            {
                lpStream = ungzip.GetOutputFile();
                dwMemLength = ungzip.GetOutputFileLength();
            }
        }
#endif
        std::shared_ptr<const uint8_t> buf(lpStream, [] (const uint8_t *) { });
        auto ctx = binaryparse::mkcontext(buf, dwMemLength);
        if (this == nullptr) throw new std::runtime_error("ASSFACE");
        if (
         (!modplug::serializers::xm::read(*this, ctx)) &&
         (!modplug::serializers::it::read(*this, ctx))
         //(!ReadIT(lpStream, dwMemLength))
         ) {
             m_nType = MOD_TYPE_NONE;
        }
#ifdef ZIPPED_MOD_SUPPORT
        if ((!m_lpszSongComments) && (archive.GetComments(FALSE)))
        {
            m_lpszSongComments = archive.GetComments(TRUE);
        }
#endif
    } else {
        // New song
        m_dwCreatedWithVersion = MptVersion::num;
    }

    // Adjust song names
    for (UINT iSmp=0; iSmp<MAX_SAMPLES; iSmp++)
    {
        LPSTR p = m_szNames[iSmp];
        int j = 31;
        p[j] = 0;
        while ((j>=0) && (p[j]<=' ')) p[j--] = 0;
        while (j>=0)
        {
            if (((uint8_t)p[j]) < ' ') p[j] = ' ';
            j--;
        }
    }
    // Adjust channels
    for (UINT ich=0; ich<MAX_BASECHANNELS; ich++)
    {
        if (ChnSettings[ich].nVolume > 64) ChnSettings[ich].nVolume = 64;
        if (ChnSettings[ich].nPan > 256) ChnSettings[ich].nPan = 128;
        Chn[ich].nPan = ChnSettings[ich].nPan;
        Chn[ich].nGlobalVol = ChnSettings[ich].nVolume;
        Chn[ich].flags = voicef_from_oldchn(ChnSettings[ich].dwFlags);
        Chn[ich].nVolume = 256;
        Chn[ich].nCutOff = 0x7F;
        Chn[ich].nEFxSpeed = 0;
        //IT compatibility 15. Retrigger
        if(IsCompatibleMode(TRK_IMPULSETRACKER))
        {
            Chn[ich].nRetrigParam = Chn[ich].nRetrigCount = 1;
        }
    }
    // Checking instruments
    modplug::tracker::modsample_t *pSmp = Samples.data();
    for (UINT iIns=0; iIns<MAX_INSTRUMENTS; iIns++, pSmp++)
    {
        if (pSmp->sample_data.generic)
        {
            if (pSmp->loop_end > pSmp->length) pSmp->loop_end = pSmp->length;
            if (pSmp->loop_start >= pSmp->loop_end)
            {
                pSmp->loop_start = 0;
                pSmp->loop_end = 0;
            }
            if (pSmp->sustain_end > pSmp->length) pSmp->sustain_end = pSmp->length;
            if (pSmp->sustain_start >= pSmp->sustain_end)
            {
                pSmp->sustain_start = 0;
                pSmp->sustain_end = 0;
            }
        } else
        {
            pSmp->length = 0;
            pSmp->loop_start = 0;
            pSmp->loop_end = 0;
            pSmp->sustain_start = 0;
            pSmp->sustain_end = 0;
        }
        if (!pSmp->loop_end) {
            bitset_remove(pSmp->flags, sflag_ty::Loop);
            bitset_remove(pSmp->flags, sflag_ty::BidiLoop);
        }
        if (!pSmp->sustain_end) {
            bitset_remove(pSmp->flags, sflag_ty::SustainLoop);
            bitset_remove(pSmp->flags, sflag_ty::BidiSustainLoop);
        }
        if (pSmp->global_volume > 64) pSmp->global_volume = 64;
    }
    // Check invalid instruments
    while ((m_nInstruments > 0) && (!Instruments[m_nInstruments])) m_nInstruments--;
    // Set default values
    if (m_nDefaultTempo < 32) m_nDefaultTempo = 125;
    if (!m_nDefaultSpeed) m_nDefaultSpeed = 6;
    m_nMusicSpeed = m_nDefaultSpeed;
    m_nMusicTempo = m_nDefaultTempo;
    m_nGlobalVolume = m_nDefaultGlobalVolume;
    m_lHighResRampingGlobalVolume = m_nGlobalVolume<<modplug::mixgraph::VOLUME_RAMP_PRECISION;
    m_nGlobalVolumeDestination = m_nGlobalVolume;
    m_nSamplesToGlobalVolRampDest=0;
    m_nNextPattern = 0;
    m_nCurrentPattern = 0;
    m_nPattern = 0;
    m_nBufferCount = 0;
    m_dBufferDiff = 0;
    m_nTickCount = m_nMusicSpeed;
    m_nNextRow = 0;
    m_nRow = 0;

    InitializeVisitedRows(true);

    switch(m_nTempoMode)
    {
        case tempo_mode_alternative:
            m_nSamplesPerTick = deprecated_global_mixing_freq / m_nMusicTempo; break;
        case tempo_mode_modern:
            m_nSamplesPerTick = deprecated_global_mixing_freq * (60/m_nMusicTempo / (m_nMusicSpeed * m_nCurrentRowsPerBeat)); break;
        case tempo_mode_classic: default:
            m_nSamplesPerTick = (deprecated_global_mixing_freq * 5 * m_nTempoFactor) / (m_nMusicTempo << 8);
    }

    if ((m_nRestartPos >= Order.size()) || (Order[m_nRestartPos] >= Patterns.Size())) m_nRestartPos = 0;

    // plugin loader
    string sNotFound;
    std::list<PLUGINDEX> notFoundIDs;

    // Load plugins only when m_pModDoc != 0.  (can be == 0 for example when examining module samples in treeview.
    if (gpMixPluginCreateProc && GetpModDoc())
    {
        for (PLUGINDEX iPlug = 0; iPlug < MAX_MIXPLUGINS; iPlug++)
        {
            if ((m_MixPlugins[iPlug].Info.dwPluginId1)
             || (m_MixPlugins[iPlug].Info.dwPluginId2))
            {
                gpMixPluginCreateProc(&m_MixPlugins[iPlug], this);
                if (m_MixPlugins[iPlug].pMixPlugin)
                {
                    // plugin has been found
                    m_MixPlugins[iPlug].pMixPlugin->RestoreAllParameters(m_MixPlugins[iPlug].defaultProgram); //rewbs.plugDefaultProgram: added param
                }
                else
                {
                    // plugin not found - add to list
                    bool bFound = false;
                    for(std::list<PLUGINDEX>::iterator i = notFoundIDs.begin(); i != notFoundIDs.end(); ++i)
                    {
                        if(m_MixPlugins[*i].Info.dwPluginId2 == m_MixPlugins[iPlug].Info.dwPluginId2)
                        {
                            bFound = true;
                            break;
                        }
                    }

                    if(bFound == false)
                    {
                        sNotFound = sNotFound + m_MixPlugins[iPlug].Info.szLibraryName + "\n";
                        notFoundIDs.push_back(iPlug); // add this to the list of missing IDs so we will find the needed plugins later when calling KVRAudio
                    }
                }
            }
        }
    }

    // Display a nice message so the user sees what plugins are missing
    // TODO: Use IDD_MODLOADING_WARNINGS dialog (NON-MODAL!) to display all warnings that are encountered when loading a module.
    if(!notFoundIDs.empty())
    {
        //XXXih: kill this
        if(notFoundIDs.size() == 1)
        {
            sNotFound = "The following plugin has not been found:\n\n" + sNotFound + "\nDo you want to search for it on KVRAudio?";
        }
        else
        {
            sNotFound =    "The following plugins have not been found:\n\n" + sNotFound + "\nDo you want to search for them on KVRAudio?"
                        "\nWARNING: A browser window / tab is opened for every plugin. If you do not want that, you can visit http://www.kvraudio.com/search.php";
        }
        if (::MessageBox(0, sNotFound.c_str(), "OpenMPT - Plugins missing", MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) == IDYES)
            for(std::list<PLUGINDEX>::iterator i = notFoundIDs.begin(); i != notFoundIDs.end(); ++i)
            {
                CString sUrl;
                sUrl.Format("http://www.kvraudio.com/search.php?lq=inurl%3Aget&q=%s", m_MixPlugins[*i].Info.szLibraryName);
            }
    }

    // Set up mix levels
    m_pConfig->SetMixLevels(m_nMixLevels);
    RecalculateGainForAllPlugs();

    //modplug::serializers::write_wao(Patterns, *this);

    if (m_nType)
    {
        SetModSpecsPointer(m_pModSpecs, m_nType);
        const modplug::tracker::orderindex_t nMinLength = std::min<uint32_t>(modplug::tracker::deprecated_modsequence_list_t::s_nCacheSize, GetModSpecifications().ordersMax);
        if (Order.GetLength() < nMinLength)
            Order.resize(nMinLength);
        return TRUE;
    }

    return FALSE;
}


BOOL module_renderer::Destroy()
//------------------------
{
    size_t i;
    Patterns.DestroyPatterns();

    for (i=1; i<MAX_SAMPLES; i++)
    {
        modplug::tracker::modsample_t *pSmp = &Samples[i];
        if (pSmp->sample_data.generic)
        {
            modplug::legacy_soundlib::free_sample(pSmp->sample_data.generic);
            pSmp->sample_data.generic = nullptr;
        }
    }
    for (i = 0; i < MAX_INSTRUMENTS; i++)
    {
        delete Instruments[i];
        Instruments[i] = nullptr;
    }
    for (i=0; i<MAX_MIXPLUGINS; i++)
    {
        if ((m_MixPlugins[i].nPluginDataSize) && (m_MixPlugins[i].pPluginData))
        {
            m_MixPlugins[i].nPluginDataSize = 0;
            delete[] m_MixPlugins[i].pPluginData;
            m_MixPlugins[i].pPluginData = NULL;
        }
        m_MixPlugins[i].pMixState = NULL;
        if (m_MixPlugins[i].pMixPlugin)
        {
            m_MixPlugins[i].pMixPlugin->Release();
            m_MixPlugins[i].pMixPlugin = NULL;
        }
    }
    m_nType = MOD_TYPE_NONE;
    m_nChannels = m_nSamples = m_nInstruments = 0;
    return TRUE;
}


//////////////////////////////////////////////////////////////////////////
// Misc functions

void module_renderer::ResetMidiCfg()
//-----------------------------
{
    MemsetZero(m_MidiCfg);
    lstrcpy(m_MidiCfg.szMidiGlb[MIDIOUT_START], "FF");
    lstrcpy(m_MidiCfg.szMidiGlb[MIDIOUT_STOP], "FC");
    lstrcpy(m_MidiCfg.szMidiGlb[MIDIOUT_NOTEON], "9c n v");
    lstrcpy(m_MidiCfg.szMidiGlb[MIDIOUT_NOTEOFF], "9c n 0");
    lstrcpy(m_MidiCfg.szMidiGlb[MIDIOUT_PROGRAM], "Cc p");
    lstrcpy(m_MidiCfg.szMidiSFXExt[0], "F0F000z");
    for (int iz=0; iz<16; iz++) wsprintf(m_MidiCfg.szMidiZXXExt[iz], "F0F001%02X", iz*8);
}


// Set null terminator for all MIDI macros
void module_renderer::SanitizeMacros()
//-------------------------------
{
    for(size_t i = 0; i < CountOf(m_MidiCfg.szMidiGlb); i++)
    {
        SetNullTerminator(m_MidiCfg.szMidiGlb[i]);
    }
    for(size_t i = 0; i < CountOf(m_MidiCfg.szMidiSFXExt); i++)
    {
        SetNullTerminator(m_MidiCfg.szMidiSFXExt[i]);
    }
    for(size_t i = 0; i < CountOf(m_MidiCfg.szMidiZXXExt); i++)
    {
        SetNullTerminator(m_MidiCfg.szMidiZXXExt[i]);
    }
}


BOOL module_renderer::deprecated_SetResamplingMode(UINT nMode)
//--------------------------------------------
{
    uint32_t d = deprecated_global_sound_setup_bitmask & ~(SNDMIX_NORESAMPLING|SNDMIX_SPLINESRCMODE|SNDMIX_POLYPHASESRCMODE|SNDMIX_FIRFILTERSRCMODE);
    switch(nMode)
    {
    case SRCMODE_NEAREST:    d |= SNDMIX_NORESAMPLING; break;
    case SRCMODE_LINEAR:    break; // default
    //rewbs.resamplerConf
    //case SRCMODE_SPLINE:    d |= SNDMIX_HQRESAMPLER; break;
    //case SRCMODE_POLYPHASE:    d |= (SNDMIX_HQRESAMPLER|SNDMIX_ULTRAHQSRCMODE); break;
    case SRCMODE_SPLINE:    d |= SNDMIX_SPLINESRCMODE; break;
    case SRCMODE_POLYPHASE:    d |= SNDMIX_POLYPHASESRCMODE; break;
    case SRCMODE_FIRFILTER:    d |= SNDMIX_FIRFILTERSRCMODE; break;
    default: return FALSE;
    //end rewbs.resamplerConf
    }
    deprecated_global_sound_setup_bitmask = d;
    return TRUE;
}


void module_renderer::SetMasterVolume(UINT nVol, bool adjustAGC)
//---------------------------------------------------------
{
    if (nVol < 1) nVol = 1;
    if (nVol > 0x200) nVol = 0x200;    // x4 maximum
    m_nMasterVolume = nVol;
}


modplug::tracker::patternindex_t module_renderer::GetNumPatterns() const
//---------------------------------------------
{
    modplug::tracker::patternindex_t bad_max = 0;
    for(modplug::tracker::patternindex_t i = 0; i < Patterns.Size(); i++)
    {
        if(Patterns.IsValidPat(i))
            bad_max = i + 1;
    }
    return bad_max;
}


UINT module_renderer::GetMaxPosition() const
//-------------------------------------
{
    UINT bad_max = 0;
    UINT i = 0;

    while ((i < Order.size()) && (Order[i] != Order.GetInvalidPatIndex()))
    {
        if (Order[i] < Patterns.Size()) bad_max += Patterns[Order[i]].GetNumRows();
        i++;
    }
    return bad_max;
}


UINT module_renderer::GetCurrentPos() const
//------------------------------------
{
    UINT pos = 0;

    for (UINT i=0; i<m_nCurrentPattern; i++) if (Order[i] < Patterns.Size())
        pos += Patterns[Order[i]].GetNumRows();
    return pos + m_nRow;
}

double  module_renderer::GetCurrentBPM() const
//---------------------------------------
{
    double bpm;

    if (m_nTempoMode == tempo_mode_modern)                    // With modern mode, we trust that true bpm
    {                                                                                            // is close enough to what user chose.
        bpm = static_cast<double>(m_nMusicTempo);    // This avoids oscillation due to tick-to-tick corrections.
    } else
    {                                                                                                                                    //with other modes, we calculate it:
        double ticksPerBeat = m_nMusicSpeed * m_nCurrentRowsPerBeat;    //ticks/beat = ticks/row  * rows/beat
        double samplesPerBeat = m_nSamplesPerTick * ticksPerBeat;            //samps/beat = samps/tick * ticks/beat
        bpm =  deprecated_global_mixing_freq/samplesPerBeat * 60;                                            //beats/sec  = samps/sec  / samps/beat
    }                                                                                                                                    //beats/bad_min  =  beats/sec * 60

    return bpm;
}

void module_renderer::SetCurrentPos(UINT nPos)
//---------------------------------------
{
    modplug::tracker::orderindex_t nPattern;
    uint8_t resetMask = (!nPos) ? CHNRESET_SETPOS_FULL : CHNRESET_SETPOS_BASIC;

    for (modplug::tracker::chnindex_t i=0; i<MAX_VIRTUAL_CHANNELS; i++)
        ResetChannelState(i, resetMask);

    if (!nPos)
    {
        m_nGlobalVolume = m_nDefaultGlobalVolume;
        m_nMusicSpeed = m_nDefaultSpeed;
        m_nMusicTempo = m_nDefaultTempo;
    }
    //m_dwSongFlags &= ~(SONG_PATTERNLOOP|SONG_CPUVERYHIGH|SONG_FADINGSONG|SONG_ENDREACHED|SONG_GLOBALFADE);
    m_dwSongFlags &= ~(SONG_CPUVERYHIGH|SONG_FADINGSONG|SONG_ENDREACHED|SONG_GLOBALFADE);
    for (nPattern = 0; nPattern < Order.size(); nPattern++)
    {
        UINT ord = Order[nPattern];
        if(ord == Order.GetIgnoreIndex()) continue;
        if (ord == Order.GetInvalidPatIndex()) break;
        if (ord < Patterns.Size())
        {
            if (nPos < (UINT)Patterns[ord].GetNumRows()) break;
            nPos -= Patterns[ord].GetNumRows();
        }
    }
    // Buggy position ?
    if ((nPattern >= Order.size())
     || (Order[nPattern] >= Patterns.Size())
     || (nPos >= Patterns[Order[nPattern]].GetNumRows()))
    {
        nPos = 0;
        nPattern = 0;
    }
    UINT nRow = nPos;
    if ((nRow) && (Order[nPattern] < Patterns.Size()))
    {
        modplug::tracker::modevent_t *p = Patterns[Order[nPattern]];
        if ((p) && (nRow < Patterns[Order[nPattern]].GetNumRows()))
        {
            bool bOk = false;
            while ((!bOk) && (nRow > 0))
            {
                UINT n = nRow * m_nChannels;
                for (UINT k=0; k<m_nChannels; k++, n++)
                {
                    if (p[n].note)
                    {
                        bOk = true;
                        break;
                    }
                }
                if (!bOk) nRow--;
            }
        }
    }
    m_nNextPattern = nPattern;
    m_nNextRow = nRow;
    m_nTickCount = m_nMusicSpeed;
    m_nBufferCount = 0;
    m_nPatternDelay = 0;
    m_nFrameDelay = 0;
    m_nNextPatStartRow = 0;
    //m_nSeqOverride = 0;
}



void module_renderer::SetCurrentOrder(modplug::tracker::orderindex_t nOrder)
//-------------------------------------------------
{
    while ((nOrder < Order.size()) && (Order[nOrder] == Order.GetIgnoreIndex())) nOrder++;
    if ((nOrder >= Order.size()) || (Order[nOrder] >= Patterns.Size())) return;
    for (modplug::tracker::chnindex_t j = 0; j < MAX_VIRTUAL_CHANNELS; j++)
    {
        Chn[j].nPeriod = 0;
        Chn[j].nNote = NoteNone;
        Chn[j].nPortamentoDest = 0;
        Chn[j].nCommand = 0;
        Chn[j].nPatternLoopCount = 0;
        Chn[j].nPatternLoop = 0;
        Chn[j].nVibratoPos = Chn[j].nTremoloPos = Chn[j].nPanbrelloPos = 0;
        //IT compatibility 15. Retrigger
        if(IsCompatibleMode(TRK_IMPULSETRACKER))
        {
            Chn[j].nRetrigCount = 0;
            Chn[j].nRetrigParam = 1;
        }
        Chn[j].nTremorCount = 0;
    }

#ifndef NO_VST
    // Stop hanging notes from VST instruments as well
    StopAllVsti();
#endif // NO_VST

    if (!nOrder)
    {
        SetCurrentPos(0);
    } else
    {
        m_nNextPattern = nOrder;
        m_nRow = m_nNextRow = 0;
        m_nPattern = 0;
        m_nTickCount = m_nMusicSpeed;
        m_nBufferCount = 0;
        m_nTotalCount = 0;
        m_nPatternDelay = 0;
        m_nFrameDelay = 0;
        m_nNextPatStartRow = 0;
    }
    //m_dwSongFlags &= ~(SONG_PATTERNLOOP|SONG_CPUVERYHIGH|SONG_FADINGSONG|SONG_ENDREACHED|SONG_GLOBALFADE);
    m_dwSongFlags &= ~(SONG_CPUVERYHIGH|SONG_FADINGSONG|SONG_ENDREACHED|SONG_GLOBALFADE);
}

//rewbs.VSTCompliance
void module_renderer::SuspendPlugins()
//-------------------------------
{
    for (UINT iPlug=0; iPlug<MAX_MIXPLUGINS; iPlug++)
    {
        if (!m_MixPlugins[iPlug].pMixPlugin)
            continue;  //most common branch

        IMixPlugin *pPlugin = m_MixPlugins[iPlug].pMixPlugin;
        if (m_MixPlugins[iPlug].pMixState)
        {
            pPlugin->NotifySongPlaying(false);
            pPlugin->HardAllNotesOff();
            pPlugin->Suspend();
        }
    }
    m_lTotalSampleCount=0;
}

void module_renderer::ResumePlugins()
//------------------------------
{
    for (UINT iPlug=0; iPlug<MAX_MIXPLUGINS; iPlug++)
    {
        if (!m_MixPlugins[iPlug].pMixPlugin)
            continue;  //most common branch

        if (m_MixPlugins[iPlug].pMixState)
        {
            IMixPlugin *pPlugin = m_MixPlugins[iPlug].pMixPlugin;
            pPlugin->NotifySongPlaying(true);
            pPlugin->Resume();
        }
    }
    m_lTotalSampleCount=GetSampleOffset();

}


void module_renderer::StopAllVsti()
//----------------------------
{
    for (UINT iPlug=0; iPlug<MAX_MIXPLUGINS; iPlug++)
    {
        if (!m_MixPlugins[iPlug].pMixPlugin)
            continue;  //most common branch

        IMixPlugin *pPlugin = m_MixPlugins[iPlug].pMixPlugin;
        if (m_MixPlugins[iPlug].pMixState)
        {
            pPlugin->HardAllNotesOff();
        }
    }
    m_lTotalSampleCount = GetSampleOffset();
}


void module_renderer::RecalculateGainForAllPlugs()
//-------------------------------------------
{
    for (UINT iPlug=0; iPlug<MAX_MIXPLUGINS; iPlug++)
    {
        if (!m_MixPlugins[iPlug].pMixPlugin)
            continue;  //most common branch

        IMixPlugin *pPlugin = m_MixPlugins[iPlug].pMixPlugin;
        if (m_MixPlugins[iPlug].pMixState)
        {
            pPlugin->RecalculateGain();
        }
    }
}

//end rewbs.VSTCompliance

void module_renderer::ResetChannels()
//------------------------------
{
    m_dwSongFlags &= ~(SONG_CPUVERYHIGH|SONG_FADINGSONG|SONG_ENDREACHED|SONG_GLOBALFADE);
    m_nBufferCount = 0;
    for (UINT i=0; i<MAX_VIRTUAL_CHANNELS; i++)
    {
        Chn[i].nROfs = Chn[i].nLOfs = Chn[i].length = 0;
    }
}



void module_renderer::LoopPattern(modplug::tracker::patternindex_t nPat, modplug::tracker::rowindex_t nRow)
//------------------------------------------------------------
{
    if ((nPat < 0) || (nPat >= Patterns.Size()) || (!Patterns[nPat]))
    {
        m_dwSongFlags &= ~SONG_PATTERNLOOP;
    } else
    {
        if ((nRow < 0) || (nRow >= (int)Patterns[nPat].GetNumRows())) nRow = 0;
        m_nPattern = nPat;
        m_nRow = m_nNextRow = nRow;
        m_nTickCount = m_nMusicSpeed;
        m_nPatternDelay = 0;
        m_nFrameDelay = 0;
        m_nBufferCount = 0;
        m_nNextPatStartRow = 0;
        m_dwSongFlags |= SONG_PATTERNLOOP;
    //    m_nSeqOverride = 0;
    }
}
//rewbs.playSongFromCursor
void module_renderer::DontLoopPattern(modplug::tracker::patternindex_t nPat, modplug::tracker::rowindex_t nRow)
//----------------------------------------------------------------
{
    if ((nPat < 0) || (nPat >= Patterns.Size()) || (!Patterns[nPat])) nPat = 0;
    if ((nRow < 0) || (nRow >= (int)Patterns[nPat].GetNumRows())) nRow = 0;
    m_nPattern = nPat;
    m_nRow = m_nNextRow = nRow;
    m_nTickCount = m_nMusicSpeed;
    m_nPatternDelay = 0;
    m_nFrameDelay = 0;
    m_nBufferCount = 0;
    m_nNextPatStartRow = 0;
    m_dwSongFlags &= ~SONG_PATTERNLOOP;
    //m_nSeqOverride = 0;
}

modplug::tracker::orderindex_t module_renderer::FindOrder(modplug::tracker::patternindex_t nPat, UINT startFromOrder, bool direction)
//--------------------------------------------------------------------------------------
{
    modplug::tracker::orderindex_t foundAtOrder = modplug::tracker::OrderIndexInvalid;
    modplug::tracker::orderindex_t candidateOrder = 0;

    for (modplug::tracker::orderindex_t p = 0; p < Order.size(); p++)
    {
        if (direction)
        {
            candidateOrder = (startFromOrder + p) % Order.size();                    //wrap around MAX_ORDERS
        } else {
            candidateOrder = (startFromOrder - p + Order.size()) % Order.size();    //wrap around 0 and MAX_ORDERS
        }
        if (Order[candidateOrder] == nPat) {
            foundAtOrder = candidateOrder;
            break;
        }
    }

    return foundAtOrder;
}
//end rewbs.playSongFromCursor


MODTYPE module_renderer::GetBestSaveFormat() const
//-------------------------------------------
{
    if ((!m_nSamples) || (!m_nChannels)) return MOD_TYPE_NONE;
    if (!m_nType) return MOD_TYPE_NONE;
    if (m_nType & (MOD_TYPE_MOD/*|MOD_TYPE_OKT*/))
        return MOD_TYPE_MOD;
    if (m_nType & (MOD_TYPE_S3M|MOD_TYPE_STM|MOD_TYPE_ULT|MOD_TYPE_FAR|MOD_TYPE_PTM|MOD_TYPE_MTM))
        return MOD_TYPE_S3M;
    if (m_nType & (MOD_TYPE_XM|MOD_TYPE_MED/*|MOD_TYPE_MT2*/))
        return MOD_TYPE_XM;
    if(m_nType & MOD_TYPE_MPT)
        return MOD_TYPE_MPT;
    return MOD_TYPE_IT;
}


MODTYPE module_renderer::GetSaveFormats() const
//----------------------------------------
{
    UINT n = 0;
    if ((!m_nSamples) || (!m_nChannels) || (m_nType == MOD_TYPE_NONE)) return 0;
    switch(m_nType)
    {
    case MOD_TYPE_MOD:    n = MOD_TYPE_MOD;
    case MOD_TYPE_S3M:    n = MOD_TYPE_S3M;
    }
    n |= MOD_TYPE_XM | MOD_TYPE_IT | MOD_TYPE_MPT;
    if (!m_nInstruments)
    {
        if (m_nSamples < 32) n |= MOD_TYPE_MOD;
        n |= MOD_TYPE_S3M;
    }
    return n;
}


LPCTSTR module_renderer::GetSampleName(UINT nSample) const
//---------------------------------------------------
{
    if (nSample<MAX_SAMPLES) {
        return m_szNames[nSample];
    } else {
        return gszEmpty;
    }
}


CString module_renderer::GetInstrumentName(UINT nInstr) const
//------------------------------------------------------
{
    if ((nInstr >= MAX_INSTRUMENTS) || (!Instruments[nInstr]))
        return TEXT("");

    const size_t nSize = CountOf(Instruments[nInstr]->name);
    CString str;
    LPTSTR p = str.GetBuffer(nSize + 1);
    std::copy_n(Instruments[nInstr]->name, nSize, p);
    p[nSize] = 0;
    str.ReleaseBuffer();
    return str;
}


bool module_renderer::InitChannel(modplug::tracker::chnindex_t nChn)
//---------------------------------------------
{
    if(nChn >= MAX_BASECHANNELS) return true;

    ChnSettings[nChn].nPan = 128;
    ChnSettings[nChn].nVolume = 64;
    ChnSettings[nChn].dwFlags = 0;
    ChnSettings[nChn].nMixPlugin = 0;
    strcpy(ChnSettings[nChn].szName, "");

    ResetChannelState(nChn, CHNRESET_TOTAL);

    if(m_pModDoc)
    {
        m_pModDoc->Record1Channel(nChn, false);
        m_pModDoc->Record2Channel(nChn, false);
    }
    m_bChannelMuteTogglePending[nChn] = false;

    return false;
}

void module_renderer::ResetChannelState(modplug::tracker::chnindex_t i, uint8_t resetMask)
//----------------------------------------------------------------
{
    if(i >= MAX_VIRTUAL_CHANNELS) return;

    if(resetMask & 2)
    {
        Chn[i].nNote = Chn[i].nNewNote = Chn[i].nNewIns = 0;
        Chn[i].sample = nullptr;
        Chn[i].instrument = nullptr;
        Chn[i].nPortamentoDest = 0;
        Chn[i].nCommand = 0;
        Chn[i].nPatternLoopCount = 0;
        Chn[i].nPatternLoop = 0;
        Chn[i].nFadeOutVol = 0;
        bitset_add(Chn[i].flags, vflag_ty::KeyOff);
        bitset_add(Chn[i].flags, vflag_ty::NoteFade);
        //IT compatibility 15. Retrigger
        if(IsCompatibleMode(TRK_IMPULSETRACKER))
        {
            Chn[i].nRetrigParam = 1;
            Chn[i].nRetrigCount = 0;
        }
        Chn[i].nTremorCount = 0;
        Chn[i].nEFxSpeed = 0;
    }

    if(resetMask & 4)
    {
        Chn[i].nPeriod = 0;
        Chn[i].sample_position = Chn[i].length = 0;
        Chn[i].loop_start = 0;
        Chn[i].loop_end = 0;
        Chn[i].nROfs = Chn[i].nLOfs = 0;
        Chn[i].sample_data.generic = nullptr;
        Chn[i].sample = nullptr;
        Chn[i].instrument = nullptr;
        Chn[i].nCutOff = 0x7F;
        Chn[i].nResonance = 0;
        Chn[i].nFilterMode = 0;
        Chn[i].left_volume = Chn[i].right_volume = 0;
        Chn[i].nNewLeftVol = Chn[i].nNewRightVol = 0;
        Chn[i].left_ramp = Chn[i].right_ramp = 0;
        Chn[i].nVolume = 256;
        Chn[i].nVibratoPos = Chn[i].nTremoloPos = Chn[i].nPanbrelloPos = 0;

        //-->Custom tuning related
        Chn[i].m_ReCalculateFreqOnFirstTick = false;
        Chn[i].m_CalculateFreq = false;
        Chn[i].m_PortamentoFineSteps = 0;
        Chn[i].m_PortamentoTickSlide = 0;
        Chn[i].m_Freq = 0;
        Chn[i].m_VibratoDepth = 0;
        //<--Custom tuning related.
    }

    if(resetMask & 1)
    {
        if(i < MAX_BASECHANNELS)
        {
            Chn[i].flags = voicef_from_oldchn(ChnSettings[i].dwFlags);
            Chn[i].nPan = ChnSettings[i].nPan;
            Chn[i].nGlobalVol = ChnSettings[i].nVolume;
        }
        else
        {
            Chn[i].flags = vflags_ty();
            Chn[i].nPan = 128;
            Chn[i].nGlobalVol = 64;
        }
        Chn[i].nRestorePanOnNewNote = 0;
        Chn[i].nRestoreCutoffOnNewNote = 0;
        Chn[i].nRestoreResonanceOnNewNote = 0;

    }
}


#ifndef NO_PACKING
UINT module_renderer::PackSample(int &sample, int next)
//------------------------------------------------
{
    UINT i = 0;
    int delta = next - sample;
    if (delta >= 0)
    {
        for (i=0; i<7; i++) if (delta <= (int)CompressionTable[i+1]) break;
    } else
    {
        for (i=8; i<15; i++) if (delta >= (int)CompressionTable[i+1]) break;
    }
    sample += (int)CompressionTable[i];
    return i;
}


bool module_renderer::CanPackSample(LPSTR pSample, UINT nLen, UINT nPacking, uint8_t *result/*=NULL*/)
//--------------------------------------------------------------------------------------------
{
    int pos, old, oldpos, besttable = 0;
    uint32_t dwErr, dwTotal, dwResult;
    int i,j;

    if (result) *result = 0;
    if ((!pSample) || (nLen < 1024)) return false;
    // Try packing with different tables
    dwResult = 0;
    for (j=1; j<MAX_PACK_TABLES; j++)
    {
        memcpy(CompressionTable, UnpackTable[j], 16);
        dwErr = 0;
        dwTotal = 1;
        old = pos = oldpos = 0;
        for (i=0; i<(int)nLen; i++)
        {
            int s = (int)pSample[i];
            PackSample(pos, s);
            dwErr += abs(pos - oldpos);
            dwTotal += abs(s - old);
            old = s;
            oldpos = pos;
        }
        dwErr = _muldiv(dwErr, 100, dwTotal);
        if (dwErr >= dwResult)
        {
            dwResult = dwErr;
            besttable = j;
        }
    }
    memcpy(CompressionTable, UnpackTable[besttable], 16);
    if (result)
    {
        if (dwResult > 100) *result    = 100; else *result = (uint8_t)dwResult;
    }
    return (dwResult >= nPacking) ? true : false;
}
#endif // NO_PACKING

#ifndef MODPLUG_NO_FILESAVE

UINT module_renderer::WriteSample(FILE *f, modplug::tracker::modsample_t *pSmp, UINT nFlags, UINT nMaxLen)
//-------------------------------------------------------------------------------
{
    UINT len = 0, bufcount;
    char buffer[4096];
    signed char *pSample = (signed char *)pSmp->sample_data.generic;
    UINT nLen = pSmp->length;

    if ((nMaxLen) && (nLen > nMaxLen)) nLen = nMaxLen;
// -> CODE#0023
// -> DESC="IT project files (.itp)"
//    if ((!pSample) || (f == NULL) || (!nLen)) return 0;
    if ((!pSample) || (!nLen)) return 0;
// NOTE : also added all needed 'if(f)' in this function
// -! NEW_FEATURE#0023
    switch(nFlags)
    {
#ifndef NO_PACKING
    // 3: 4-bit ADPCM data
    case RS_ADPCM4:
        {
            int pos;
            len = (nLen + 1) / 2;
            if(f) fwrite(CompressionTable, 16, 1, f);
            bufcount = 0;
            pos = 0;
            for (UINT j=0; j<len; j++)
            {
                uint8_t b;
                // Sample #1
                b = PackSample(pos, (int)pSample[j*2]);
                // Sample #2
                b |= PackSample(pos, (int)pSample[j*2+1]) << 4;
                buffer[bufcount++] = (char)b;
                if (bufcount >= sizeof(buffer))
                {
                    if(f) fwrite(buffer, 1, bufcount, f);
                    bufcount = 0;
                }
            }
            if (bufcount) if(f) fwrite(buffer, 1, bufcount, f);
            len += 16;
        }
        break;
#endif // NO_PACKING

    // 16-bit samples
    case RS_PCM16U:
    case RS_PCM16D:
    case RS_PCM16S:
        {
            int16_t *p = (int16_t *)pSample;
            int s_old = 0, s_ofs;
            len = nLen * 2;
            bufcount = 0;
            s_ofs = (nFlags == RS_PCM16U) ? 0x8000 : 0;
            for (UINT j=0; j<nLen; j++)
            {
                int s_new = *p;
                p++;
                if (bitset_is_set(pSmp->flags, sflag_ty::Stereo))
                {
                    s_new = (s_new + (*p) + 1) >> 1;
                    p++;
                }
                if (nFlags == RS_PCM16D)
                {
                    *((int16_t *)(&buffer[bufcount])) = (int16_t)(s_new - s_old);
                    s_old = s_new;
                } else
                {
                    *((int16_t *)(&buffer[bufcount])) = (int16_t)(s_new + s_ofs);
                }
                bufcount += 2;
                if (bufcount >= sizeof(buffer) - 1)
                {
                    if(f) fwrite(buffer, 1, bufcount, f);
                    bufcount = 0;
                }
            }
            if (bufcount) if(f) fwrite(buffer, 1, bufcount, f);
        }
        break;

    // 8-bit Stereo samples (not interleaved)
    case RS_STPCM8S:
    case RS_STPCM8U:
    case RS_STPCM8D:
        {
            int s_ofs = (nFlags == RS_STPCM8U) ? 0x80 : 0;
            for (UINT iCh=0; iCh<2; iCh++)
            {
                int8_t *p = ((int8_t *)pSample) + iCh;
                int s_old = 0;

                bufcount = 0;
                for (UINT j=0; j<nLen; j++)
                {
                    int s_new = *p;
                    p += 2;
                    if (nFlags == RS_STPCM8D)
                    {
                        buffer[bufcount++] = (int8_t)(s_new - s_old);
                        s_old = s_new;
                    } else
                    {
                        buffer[bufcount++] = (int8_t)(s_new + s_ofs);
                    }
                    if (bufcount >= sizeof(buffer))
                    {
                        if(f) fwrite(buffer, 1, bufcount, f);
                        bufcount = 0;
                    }
                }
                if (bufcount) if(f) fwrite(buffer, 1, bufcount, f);
            }
        }
        len = nLen * 2;
        break;

    // 16-bit Stereo samples (not interleaved)
    case RS_STPCM16S:
    case RS_STPCM16U:
    case RS_STPCM16D:
        {
            int s_ofs = (nFlags == RS_STPCM16U) ? 0x8000 : 0;
            for (UINT iCh=0; iCh<2; iCh++)
            {
                int16_t *p = ((int16_t *)pSample) + iCh;
                int s_old = 0;

                bufcount = 0;
                for (UINT j=0; j<nLen; j++)
                {
                    int s_new = *p;
                    p += 2;
                    if (nFlags == RS_STPCM16D)
                    {
                        *((int16_t *)(&buffer[bufcount])) = (int16_t)(s_new - s_old);
                        s_old = s_new;
                    } else
                    {
                        *((int16_t *)(&buffer[bufcount])) = (int16_t)(s_new + s_ofs);
                    }
                    bufcount += 2;
                    if (bufcount >= sizeof(buffer))
                    {
                        if(f) fwrite(buffer, 1, bufcount, f);
                        bufcount = 0;
                    }
                }
                if (bufcount) if(f) fwrite(buffer, 1, bufcount, f);
            }
        }
        len = nLen*4;
        break;

    //    Stereo signed interleaved
    case RS_STIPCM8S:
    case RS_STIPCM16S:
        len = nLen * 2;
        if (nFlags == RS_STIPCM16S) len *= 2;
        if(f) fwrite(pSample, 1, len, f);
        break;

    //    Stereo unsigned interleaved
    case RS_STIPCM8U:
        len = nLen * 2;
        bufcount = 0;
        for (UINT j=0; j<len; j++)
        {
            *((uint8_t *)(&buffer[bufcount])) = *((uint8_t *)(&pSample[j])) + 0x80;
            bufcount++;
            if (bufcount >= sizeof(buffer))
            {
                if(f) fwrite(buffer, 1, bufcount, f);
                bufcount = 0;
            }
        }
        if (bufcount) if(f) fwrite(buffer, 1, bufcount, f);
        break;

    // Default: assume 8-bit PCM data
    default:
        len = nLen;
        bufcount = 0;
        {
            int8_t *p = (int8_t *)pSample;
            int sinc = pSmp->sample_tag == stag_ty::Int16 ? 2 : 1;
            int s_old = 0, s_ofs = (nFlags == RS_PCM8U) ? 0x80 : 0;
            if (pSmp->sample_tag == stag_ty::Int16) p++;
            for (UINT j=0; j<len; j++)
            {
                int s_new = (int8_t)(*p);
                p += sinc;
                if (bitset_is_set(pSmp->flags, sflag_ty::Stereo))
                {
                    s_new = (s_new + ((int)*p) + 1) >> 1;
                    p += sinc;
                }
                if (nFlags == RS_PCM8D)
                {
                    buffer[bufcount++] = (int8_t)(s_new - s_old);
                    s_old = s_new;
                } else
                {
                    buffer[bufcount++] = (int8_t)(s_new + s_ofs);
                }
                if (bufcount >= sizeof(buffer))
                {
                    if(f) fwrite(buffer, 1, bufcount, f);
                    bufcount = 0;
                }
            }
            if (bufcount) if(f) fwrite(buffer, 1, bufcount, f);
        }
    }
    return len;
}

#endif // MODPLUG_NO_FILESAVE


// Flags:
//    0 = signed 8-bit PCM data (default)
//    1 = unsigned 8-bit PCM data
//    2 = 8-bit ADPCM data with linear table
//    3 = 4-bit ADPCM data
//    4 = 16-bit ADPCM data with linear table
//    5 = signed 16-bit PCM data
//    6 = unsigned 16-bit PCM data

UINT module_renderer::ReadSample(modplug::tracker::modsample_t *pSmp, UINT nFlags, LPCSTR lpMemFile, uint32_t dwMemLength, const uint16_t format)
//---------------------------------------------------------------------------------------------------------------
{
    if ((!pSmp) || (pSmp->length < 2) || (!lpMemFile)) return 0;

    if(pSmp->length > MAX_SAMPLE_LENGTH)
        pSmp->length = MAX_SAMPLE_LENGTH;

    UINT len = 0, mem = pSmp->length+6;

    pSmp->sample_tag = stag_ty::Int8;
    bitset_remove(pSmp->flags, sflag_ty::Stereo);
    if (nFlags & RSF_16BIT)
    {
        mem *= 2;
        pSmp->sample_tag = stag_ty::Int16;
    }
    if (nFlags & RSF_STEREO)
    {
        mem *= 2;
        bitset_add(pSmp->flags, sflag_ty::Stereo);
    }

    if ((pSmp->sample_data.generic = modplug::legacy_soundlib::alloc_sample(mem)) == NULL)
    {
        pSmp->length = 0;
        return 0;
    }

    // Check that allocated memory size is not less than what the modinstrument itself
    // thinks it is.
    if( mem < sample_len_bytes(*pSmp) )
    {
        pSmp->length = 0;
        modplug::legacy_soundlib::free_sample(pSmp->sample_data.generic);
        pSmp->sample_data.generic = nullptr;
        MessageBox(0, str_SampleAllocationError, str_Error, MB_ICONERROR);
        return 0;
    }

    switch(nFlags)
    {
    // 1: 8-bit unsigned PCM data
    case RS_PCM8U:
        {
            len = pSmp->length;
            if (len > dwMemLength) len = pSmp->length = dwMemLength;
            LPSTR pSample = pSmp->sample_data.generic;
            for (UINT j=0; j<len; j++) pSample[j] = (char)(lpMemFile[j] - 0x80);
        }
        break;

    // 2: 8-bit ADPCM data with linear table
    case RS_PCM8D:
        {
            len = pSmp->length;
            if (len > dwMemLength) break;
            LPSTR pSample = pSmp->sample_data.generic;
            const char *p = (const char *)lpMemFile;
            int delta = 0;
            for (UINT j=0; j<len; j++)
            {
                delta += p[j];
                *pSample++ = (char)delta;
            }
        }
        break;

    // 3: 4-bit ADPCM data
    case RS_ADPCM4:
        {
            len = (pSmp->length + 1) / 2;
            if (len > dwMemLength - 16) break;
            memcpy(CompressionTable, lpMemFile, 16);
            lpMemFile += 16;
            LPSTR pSample = pSmp->sample_data.generic;
            char delta = 0;
            for (UINT j=0; j<len; j++)
            {
                uint8_t b0 = (uint8_t)lpMemFile[j];
                uint8_t b1 = (uint8_t)(lpMemFile[j] >> 4);
                delta = (char)GetDeltaValue((int)delta, b0);
                pSample[0] = delta;
                delta = (char)GetDeltaValue((int)delta, b1);
                pSample[1] = delta;
                pSample += 2;
            }
            len += 16;
        }
        break;

    // 4: 16-bit ADPCM data with linear table
    case RS_PCM16D:
        {
            len = pSmp->length * 2;
            if (len > dwMemLength) break;
            short int *pSample = (short int *)pSmp->sample_data.generic;
            short int *p = (short int *)lpMemFile;
            int delta16 = 0;
            for (UINT j=0; j<len; j+=2)
            {
                delta16 += *p++;
                *pSample++ = (short int)delta16;
            }
        }
        break;

    // 5: 16-bit signed PCM data
    case RS_PCM16S:
        len = pSmp->length * 2;
        if (len <= dwMemLength) memcpy(pSmp->sample_data.generic, lpMemFile, len);
        break;

    // 16-bit signed mono PCM motorola byte order
    case RS_PCM16M:
        len = pSmp->length * 2;
        if (len > dwMemLength) len = dwMemLength & ~1;
        if (len > 1)
        {
            char *pSample = (char *)pSmp->sample_data.generic;
            char *pSrc = (char *)lpMemFile;
            for (UINT j=0; j<len; j+=2)
            {
                pSample[j] = pSrc[j+1];
                pSample[j+1] = pSrc[j];
            }
        }
        break;

    // 6: 16-bit unsigned PCM data
    case RS_PCM16U:
        {
            len = pSmp->length * 2;
            if (len > dwMemLength) break;
            short int *pSample = (short int *)pSmp->sample_data.generic;
            short int *pSrc = (short int *)lpMemFile;
            for (UINT j=0; j<len; j+=2) *pSample++ = (*(pSrc++)) - 0x8000;
        }
        break;

    // 16-bit signed stereo big endian
    case RS_STPCM16M:
        len = pSmp->length * 2;
        if (len*2 <= dwMemLength)
        {
            char *pSample = (char *)pSmp->sample_data.generic;
            char *pSrc = (char *)lpMemFile;
            for (UINT j=0; j<len; j+=2)
            {
                pSample[j*2] = pSrc[j+1];
                pSample[j*2+1] = pSrc[j];
                pSample[j*2+2] = pSrc[j+1+len];
                pSample[j*2+3] = pSrc[j+len];
            }
            len *= 2;
        }
        break;

    // 8-bit stereo samples
    case RS_STPCM8S:
    case RS_STPCM8U:
    case RS_STPCM8D:
        {
            len = pSmp->length;
            char *psrc = (char *)lpMemFile;
            char *pSample = (char *)pSmp->sample_data.generic;
            if (len*2 > dwMemLength) break;
            for (UINT c=0; c<2; c++)
            {
                int iadd = 0;
                if (nFlags == RS_STPCM8U) iadd = -128;
                if (nFlags == RS_STPCM8D)
                {
                    for (UINT j=0; j<len; j++)
                    {
                        iadd += psrc[0];
                        psrc++;
                        pSample[j*2] = (char)iadd;
                    }
                } else
                {
                    for (UINT j=0; j<len; j++)
                    {
                        pSample[j*2] = (char)(psrc[0] + iadd);
                        psrc++;
                    }
                }
                pSample++;
            }
            len *= 2;
        }
        break;

    // 16-bit stereo samples
    case RS_STPCM16S:
    case RS_STPCM16U:
    case RS_STPCM16D:
        {
            len = pSmp->length;
            short int *psrc = (short int *)lpMemFile;
            short int *pSample = (short int *)pSmp->sample_data.generic;
            if (len*4 > dwMemLength) break;
            for (UINT c=0; c<2; c++)
            {
                int iadd = 0;
                if (nFlags == RS_STPCM16U) iadd = -0x8000;
                if (nFlags == RS_STPCM16D)
                {
                    for (UINT j=0; j<len; j++)
                    {
                        iadd += psrc[0];
                        psrc++;
                        pSample[j*2] = (short int)iadd;
                    }
                } else
                {
                    for (UINT j=0; j<len; j++)
                    {
                        pSample[j*2] = (short int) (psrc[0] + iadd);
                        psrc++;
                    }
                }
                pSample++;
            }
            len *= 4;
        }
        break;

    // IT 2.14 compressed samples
    case RS_IT2148:
    case RS_IT21416:
    case RS_IT2158:
    case RS_IT21516:
        len = dwMemLength;
        if (len < 4) break;
        if ((nFlags == RS_IT2148) || (nFlags == RS_IT2158))
            ITUnpack8Bit(pSmp->sample_data.generic, pSmp->length, (LPBYTE)lpMemFile, dwMemLength, (nFlags == RS_IT2158));
        else
            ITUnpack16Bit(pSmp->sample_data.generic, pSmp->length, (LPBYTE)lpMemFile, dwMemLength, (nFlags == RS_IT21516));
        break;

    // 8-bit interleaved stereo samples
    case RS_STIPCM8S:
    case RS_STIPCM8U:
        {
            int iadd = 0;
            if (nFlags == RS_STIPCM8U) { iadd = -0x80; }
            len = pSmp->length;
            if (len*2 > dwMemLength) len = dwMemLength >> 1;
            LPBYTE psrc = (LPBYTE)lpMemFile;
            LPBYTE pSample = (LPBYTE)pSmp->sample_data.generic;
            for (UINT j=0; j<len; j++)
            {
                pSample[j*2] = (char)(psrc[0] + iadd);
                pSample[j*2+1] = (char)(psrc[1] + iadd);
                psrc+=2;
            }
            len *= 2;
        }
        break;

    // 16-bit interleaved stereo samples
    case RS_STIPCM16S:
    case RS_STIPCM16U:
        {
            int iadd = 0;
            if (nFlags == RS_STIPCM16U) iadd = -32768;
            len = pSmp->length;
            if (len*4 > dwMemLength) len = dwMemLength >> 2;
            short int *psrc = (short int *)lpMemFile;
            short int *pSample = (short int *)pSmp->sample_data.generic;
            for (UINT j=0; j<len; j++)
            {
                pSample[j*2] = (short int)(psrc[0] + iadd);
                pSample[j*2+1] = (short int)(psrc[1] + iadd);
                psrc += 2;
            }
            len *= 4;
        }
        break;


    // PTM 8bit delta to 16-bit sample
    case RS_PTM8DTO16:
        {
            len = pSmp->length * 2;
            if (len > dwMemLength) break;
            signed char *pSample = (signed char *)pSmp->sample_data.generic;
            signed char delta8 = 0;
            for (UINT j=0; j<len; j++)
            {
                delta8 += lpMemFile[j];
                *pSample++ = delta8;
            }
        }
        break;

#ifdef MODPLUG_TRACKER
    // Mono PCM 24/32-bit signed & 32 bit float -> load sample, and normalize it to 16-bit
    case RS_PCM24S:
    case RS_PCM32S:
        len = pSmp->length * 3;
        if (nFlags == RS_PCM32S) len += pSmp->length;
        if (len > dwMemLength) break;
        if (len > 4*8)
        {
            if(nFlags == RS_PCM24S)
            {
                char* pSrc = (char*)lpMemFile;
                char* pDest = (char*)pSmp->sample_data.generic;
                CopyWavBuffer<3, 2, WavSigned24To16, MaxFinderSignedInt<3> >(pSrc, len, pDest, sample_len_bytes(*pSmp));
            }
            else //RS_PCM32S
            {
                char* pSrc = (char*)lpMemFile;
                char* pDest = (char*)pSmp->sample_data.generic;
                if(format == 3)
                    CopyWavBuffer<4, 2, WavFloat32To16, MaxFinderFloat32>(pSrc, len, pDest, sample_len_bytes(*pSmp));
                else
                    CopyWavBuffer<4, 2, WavSigned32To16, MaxFinderSignedInt<4> >(pSrc, len, pDest, sample_len_bytes(*pSmp));
            }
        }
        break;

    // Stereo PCM 24/32-bit signed & 32 bit float -> convert sample to 16 bit
    case RS_STIPCM24S:
    case RS_STIPCM32S:
        if(format == 3 && nFlags == RS_STIPCM32S) //Microsoft IEEE float
        {
            len = pSmp->length * 6;
            //pSmp->length tells(?) the number of frames there
            //are. One 'frame' of 1 byte(== 8 bit) mono data requires
            //1 byte of space, while one frame of 3 byte(24 bit)
            //stereo data requires 3*2 = 6 bytes. This is(?)
            //why there is factor 6.

            len += pSmp->length * 2;
            //Compared to 24 stereo, 32 bit stereo needs 16 bits(== 2 bytes)
            //more per frame.

            if(len > dwMemLength) break;
            char* pSrc = (char*)lpMemFile;
            char* pDest = (char*)pSmp->sample_data.generic;
            if (len > 8*8)
            {
                CopyWavBuffer<4, 2, WavFloat32To16, MaxFinderFloat32>(pSrc, len, pDest, sample_len_bytes(*pSmp));
            }
        }
        else
        {
            len = pSmp->length * 6;
            if (nFlags == RS_STIPCM32S) len += pSmp->length * 2;
            if (len > dwMemLength) break;
            if (len > 8*8)
            {
                char* pSrc = (char*)lpMemFile;
                char* pDest = (char*)pSmp->sample_data.generic;
                if(nFlags == RS_STIPCM32S)
                {
                    CopyWavBuffer<4,2,WavSigned32To16, MaxFinderSignedInt<4> >(pSrc, len, pDest, sample_len_bytes(*pSmp));
                }
                if(nFlags == RS_STIPCM24S)
                {
                    CopyWavBuffer<3,2,WavSigned24To16, MaxFinderSignedInt<3> >(pSrc, len, pDest, sample_len_bytes(*pSmp));
                }

            }
        }
        break;

    // 16-bit signed big endian interleaved stereo
    case RS_STIPCM16M:
        {
            len = pSmp->length;
            if (len*4 > dwMemLength) len = dwMemLength >> 2;
            const uint8_t * psrc = (const uint8_t *)lpMemFile;
            short int *pSample = (short int *)pSmp->sample_data.generic;
            for (UINT j=0; j<len; j++)
            {
                pSample[j*2] = (signed short)(((UINT)psrc[0] << 8) | (psrc[1]));
                pSample[j*2+1] = (signed short)(((UINT)psrc[2] << 8) | (psrc[3]));
                psrc += 4;
            }
            len *= 4;
        }
        break;

#endif // MODPLUG_TRACKER

    // Default: 8-bit signed PCM data
    default:
        len = pSmp->length;
        if (len > dwMemLength) len = pSmp->length = dwMemLength;
        memcpy(pSmp->sample_data.generic, lpMemFile, len);
    }
    if (len > dwMemLength)
    {
        if (pSmp->sample_data.generic)
        {
            pSmp->length = 0;
            modplug::legacy_soundlib::free_sample(pSmp->sample_data.generic);
            pSmp->sample_data.generic = nullptr;
        }
        return 0;
    }
    AdjustSampleLoop(pSmp);
    return len;
}


void module_renderer::AdjustSampleLoop(modplug::tracker::modsample_t *pSmp)
//------------------------------------------------
{
    if ((!pSmp->sample_data.generic) || (!pSmp->length)) return;
    if (pSmp->loop_end > pSmp->length) pSmp->loop_end = pSmp->length;
    if (pSmp->loop_start >= pSmp->loop_end)
    {
        pSmp->loop_start = pSmp->loop_end = 0;
        bitset_remove(pSmp->flags, sflag_ty::Loop);
    }
    const auto loop = sflags_ty(sflag_ty::Loop);
    const auto test = bitset_set(bitset_set(loop, sflag_ty::BidiLoop), sflag_ty::Stereo);
    UINT len = pSmp->length;
    if (pSmp->sample_tag == stag_ty::Int16) {
        short int *pSample = (short int *)pSmp->sample_data.generic;
        // Adjust end of sample
        if (bitset_is_set(pSmp->flags, sflag_ty::Stereo)) {
            pSample[len*2+6] = pSample[len*2+4] = pSample[len*2+2] = pSample[len*2] = pSample[len*2-2];
            pSample[len*2+7] = pSample[len*2+5] = pSample[len*2+3] = pSample[len*2+1] = pSample[len*2-1];
        } else {
            pSample[len+4] = pSample[len+3] = pSample[len+2] = pSample[len+1] = pSample[len] = pSample[len-1];
        }
        if (bitset_intersect(pSmp->flags, test) == loop) {
            // Fix bad loops
            if ((pSmp->loop_end+3 >= pSmp->length) || (m_nType & MOD_TYPE_S3M)) {
                pSample[pSmp->loop_end] = pSample[pSmp->loop_start];
                pSample[pSmp->loop_end+1] = pSample[pSmp->loop_start+1];
                pSample[pSmp->loop_end+2] = pSample[pSmp->loop_start+2];
                pSample[pSmp->loop_end+3] = pSample[pSmp->loop_start+3];
                pSample[pSmp->loop_end+4] = pSample[pSmp->loop_start+4];
            }
        }
    } else
    {
        LPSTR pSample = pSmp->sample_data.generic;
        // Crappy samples (except chiptunes) ?
        if ( (pSmp->length > 0x100) &&
             (m_nType & (MOD_TYPE_MOD|MOD_TYPE_S3M)) &&
             (!bitset_is_set(pSmp->flags, sflag_ty::Stereo)) )
        {
            int smpend = pSample[pSmp->length-1], smpfix = 0, kscan;
            for (kscan=pSmp->length-1; kscan>0; kscan--) {
                smpfix = pSample[kscan-1];
                if (smpfix != smpend) break;
            }
            int delta = smpfix - smpend;
            if ( ((!bitset_is_set(pSmp->flags, sflag_ty::Loop)) || (kscan > (int)pSmp->loop_end)) &&
                 ((delta < -8) || (delta > 8)) )
            {
                while (kscan<(int)pSmp->length)
                {
                    if (!(kscan & 7))
                    {
                        if (smpfix > 0) smpfix--;
                        if (smpfix < 0) smpfix++;
                    }
                    pSample[kscan] = (char)smpfix;
                    kscan++;
                }
            }
        }
        // Adjust end of sample
        if (bitset_is_set(pSmp->flags, sflag_ty::Stereo)) {
            pSample[len*2+6] = pSample[len*2+4] = pSample[len*2+2] = pSample[len*2] = pSample[len*2-2];
            pSample[len*2+7] = pSample[len*2+5] = pSample[len*2+3] = pSample[len*2+1] = pSample[len*2-1];
        } else {
            pSample[len+4] = pSample[len+3] = pSample[len+2] = pSample[len+1] = pSample[len] = pSample[len-1];
        }
        if (bitset_intersect(pSmp->flags, test) == loop) {
            if ((pSmp->loop_end+3 >= pSmp->length) || (m_nType & (MOD_TYPE_MOD|MOD_TYPE_S3M)))
            {
                pSample[pSmp->loop_end] = pSample[pSmp->loop_start];
                pSample[pSmp->loop_end+1] = pSample[pSmp->loop_start+1];
                pSample[pSmp->loop_end+2] = pSample[pSmp->loop_start+2];
                pSample[pSmp->loop_end+3] = pSample[pSmp->loop_start+3];
                pSample[pSmp->loop_end+4] = pSample[pSmp->loop_start+4];
            }
        }
    }
}



void module_renderer::CheckCPUUsage(UINT nCPU)
//---------------------------------------
{
    if (nCPU > 100) nCPU = 100;
    gnCPUUsage = nCPU;
    if (nCPU < 90)
    {
        m_dwSongFlags &= ~SONG_CPUVERYHIGH;
    } else
    if ((m_dwSongFlags & SONG_CPUVERYHIGH) && (nCPU >= 94))
    {
        UINT i=MAX_VIRTUAL_CHANNELS;
        while (i >= 8)
        {
            i--;
            if (Chn[i].length)
            {
                Chn[i].length = Chn[i].sample_position = 0;
                nCPU -= 2;
                if (nCPU < 94) break;
            }
        }
    } else
    if (nCPU > 90)
    {
        m_dwSongFlags |= SONG_CPUVERYHIGH;
    }
}


// Check whether a sample is used.
// In sample mode, the sample numbers in all patterns are checked.
// In instrument mode, it is only checked if a sample is referenced by an instrument (but not if the sample is actually played anywhere)
bool module_renderer::IsSampleUsed(modplug::tracker::sampleindex_t nSample) const
//------------------------------------------------------
{
    if ((!nSample) || (nSample > GetNumSamples())) return false;
    if (GetNumInstruments())
    {
        for (UINT i = 1; i <= GetNumInstruments(); i++) if (Instruments[i])
        {
            modplug::tracker::modinstrument_t *pIns = Instruments[i];
            for (UINT j = 0; j < CountOf(pIns->Keyboard); j++)
            {
                if (pIns->Keyboard[j] == nSample) return true;
            }
        }
    } else
    {
        for (UINT i=0; i<Patterns.Size(); i++) if (Patterns[i])
        {
            const modplug::tracker::modevent_t *m = Patterns[i];
            for (UINT j=m_nChannels*Patterns[i].GetNumRows(); j; m++, j--)
            {
                if (m->instr == nSample && !m->IsPcNote()) return true;
            }
        }
    }
    return false;
}


bool module_renderer::IsInstrumentUsed(modplug::tracker::instrumentindex_t nInstr) const
//-------------------------------------------------------------
{
    if ((!nInstr) || (nInstr > GetNumInstruments()) || (!Instruments[nInstr])) return false;
    for (UINT i=0; i<Patterns.Size(); i++) if (Patterns[i])
    {
        const modplug::tracker::modevent_t *m = Patterns[i];
        for (UINT j=m_nChannels*Patterns[i].GetNumRows(); j; m++, j--)
        {
            if (m->instr == nInstr && !m->IsPcNote()) return true;
        }
    }
    return false;
}


// Detect samples that are referenced by an instrument, but actually not used in a song.
// Only works in instrument mode. Unused samples are marked as false in the vector.
modplug::tracker::sampleindex_t module_renderer::DetectUnusedSamples(vector<bool> &sampleUsed) const
//-------------------------------------------------------------------------
{
    sampleUsed.assign(GetNumSamples() + 1, false);

    if(GetNumInstruments() == 0)
    {
        return 0;
    }
    modplug::tracker::sampleindex_t nExt = 0;

    for (modplug::tracker::patternindex_t nPat = 0; nPat < GetNumPatterns(); nPat++)
    {
        const modplug::tracker::modevent_t *p = Patterns[nPat];
        if(p == nullptr)
        {
            continue;
        }

        UINT jmax = Patterns[nPat].GetNumRows() * GetNumChannels();
        for (UINT j=0; j<jmax; j++, p++)
        {
            if ((p->note) && (p->note <= NoteMax) && (!p->IsPcNote()))
            {
                if ((p->instr) && (p->instr < MAX_INSTRUMENTS))
                {
                    modplug::tracker::modinstrument_t *pIns = Instruments[p->instr];
                    if (pIns)
                    {
                        modplug::tracker::sampleindex_t n = pIns->Keyboard[p->note-1];
                        if (n <= GetNumSamples()) sampleUsed[n] = true;
                    }
                } else
                {
                    for (modplug::tracker::instrumentindex_t k = GetNumInstruments(); k >= 1; k--)
                    {
                        modplug::tracker::modinstrument_t *pIns = Instruments[k];
                        if (pIns)
                        {
                            modplug::tracker::sampleindex_t n = pIns->Keyboard[p->note-1];
                            if (n <= GetNumSamples()) sampleUsed[n] = true;
                        }
                    }
                }
            }
        }
    }
    for (modplug::tracker::sampleindex_t ichk = GetNumSamples(); ichk >= 1; ichk--)
    {
        if ((!sampleUsed[ichk]) && (Samples[ichk].sample_data.generic)) nExt++;
    }

    return nExt;
}


// Destroy samples where keepSamples index is false. First sample is keepSamples[1]!
modplug::tracker::sampleindex_t module_renderer::RemoveSelectedSamples(const vector<bool> &keepSamples)
//----------------------------------------------------------------------------
{
    if(keepSamples.empty())
    {
        return 0;
    }
    modplug::tracker::sampleindex_t nRemoved = 0;
    for(modplug::tracker::sampleindex_t nSmp = (modplug::tracker::sampleindex_t)bad_min(GetNumSamples(), keepSamples.size() - 1); nSmp >= 1; nSmp--)
    {
        if(!keepSamples[nSmp])
        {
            if(DestroySample(nSmp))
            {
                nRemoved++;
            }
            if((nSmp == GetNumSamples()) && (nSmp > 1)) m_nSamples--;
        }
    }
    return nRemoved;
}


bool module_renderer::DestroySample(modplug::tracker::sampleindex_t nSample)
//-------------------------------------------------
{
    if ((!nSample) || (nSample >= MAX_SAMPLES)) return false;
    if (!Samples[nSample].sample_data.generic) return true;
    modplug::tracker::modsample_t *pSmp = &Samples[nSample];
    LPSTR pSample = pSmp->sample_data.generic;
    pSmp->sample_data.generic = nullptr;
    pSmp->length = 0;
    pSmp->sample_tag = stag_ty::Int8;
    for (UINT i=0; i<MAX_VIRTUAL_CHANNELS; i++)
    {
        if (Chn[i].sample_data.generic == pSample)
        {
            Chn[i].sample_position = Chn[i].length = 0;
            Chn[i].sample_data.generic = Chn[i].active_sample_data.generic = nullptr;
        }
    }
    modplug::legacy_soundlib::free_sample(pSample);
    return true;
}

// -> CODE#0020
// -> DESC="rearrange sample list"
bool module_renderer::MoveSample(modplug::tracker::sampleindex_t from, modplug::tracker::sampleindex_t to)
//-----------------------------------------------------------
{
    if (!from || from >= MAX_SAMPLES || !to || to >= MAX_SAMPLES) return false;
    if (/*!Ins[from].sample_data ||*/ Samples[to].sample_data.generic) return true;

    modplug::tracker::modsample_t *pFrom = &Samples[from];
    modplug::tracker::modsample_t *pTo = &Samples[to];

    memcpy(pTo, pFrom, sizeof(modplug::tracker::modsample_t));

    pFrom->sample_data.generic = nullptr;
    pFrom->length = 0;
    pFrom->sample_tag = stag_ty::Int8;

    return true;
}
// -! NEW_FEATURE#0020

//rewbs.plugDocAware
/*PSNDMIXPLUGIN CSoundFile::GetSndPlugMixPlug(IMixPlugin *pPlugin)
{
    for (UINT iPlug=0; iPlug<MAX_MIXPLUGINS; iPlug++)
    {
        if (m_MixPlugins[iPlug].pMixPlugin == pPlugin)
            return &(m_MixPlugins[iPlug]);
    }

    return NULL;
}*/
//end rewbs.plugDocAware


void module_renderer::BuildDefaultInstrument()
//---------------------------------------
{
// m_defaultInstrument is currently only used to get default values for extented properties.
// In the future we can make better use of this.
    MemsetZero(m_defaultInstrument);
    m_defaultInstrument.resampling_mode = SRCMODE_DEFAULT;
    m_defaultInstrument.default_filter_mode = FLTMODE_UNCHANGED;
    m_defaultInstrument.pitch_pan_center = 5*12;
    m_defaultInstrument.global_volume=64;
    m_defaultInstrument.default_pan = 0x20 << 2;
    //m_defaultInstrument.default_filter_cutoff = 0xFF;
    m_defaultInstrument.panning_envelope.release_node=ENV_RELEASE_NODE_UNSET;
    m_defaultInstrument.pitch_envelope.release_node=ENV_RELEASE_NODE_UNSET;
    m_defaultInstrument.volume_envelope.release_node=ENV_RELEASE_NODE_UNSET;
    m_defaultInstrument.pitch_to_tempo_lock = 0;
    m_defaultInstrument.pTuning = m_defaultInstrument.s_DefaultTuning;
    m_defaultInstrument.nPluginVelocityHandling = PLUGIN_VELOCITYHANDLING_CHANNEL;
    m_defaultInstrument.nPluginVolumeHandling = module_renderer::s_DefaultPlugVolumeHandling;
}


void module_renderer::DeleteStaticdata()
//---------------------------------
{
    delete s_pTuningsSharedLocal; s_pTuningsSharedLocal = nullptr;
    delete s_pTuningsSharedBuiltIn; s_pTuningsSharedBuiltIn = nullptr;
}

bool module_renderer::SaveStaticTunings()
//----------------------------------
{
    if(s_pTuningsSharedLocal->Serialize())
    {
        ErrorBox(IDS_ERR_TUNING_SERIALISATION, NULL);
        return true;
    }
    return false;
}

void SimpleMessageBox(const char* message, const char* title)
//-----------------------------------------------------------
{
    MessageBox(0, message, title, MB_ICONINFORMATION);
}

bool module_renderer::LoadStaticTunings()
//----------------------------------
{
    if(s_pTuningsSharedLocal || s_pTuningsSharedBuiltIn) return true;
    //For now not allowing to reload tunings(one should be careful when reloading them
    //since various parts may use addresses of the tuningobjects).

    CTuning::MessageHandler = &SimpleMessageBox;

    s_pTuningsSharedBuiltIn = new CTuningCollection;
    s_pTuningsSharedLocal = new CTuningCollection("Local tunings");

    // Load built-in tunings.
    const char* pData = nullptr;
    HGLOBAL hglob = nullptr;
    size_t nSize = 0;
    if (LoadResource(MAKEINTRESOURCE(IDR_BUILTIN_TUNINGS), TEXT("TUNING"), pData, nSize, hglob) != nullptr)
    {
        std::istrstream iStrm(pData, nSize);
        s_pTuningsSharedBuiltIn->Deserialize(iStrm);
        FreeResource(hglob);
    }
    if(s_pTuningsSharedBuiltIn->GetNumTunings() == 0)
    {
        ASSERT(false);
        CTuningRTI* pT = new CTuningRTI;
        //Note: Tuning collection class handles deleting.
        pT->CreateGeometric(1,1);
        if(s_pTuningsSharedBuiltIn->AddTuning(pT))
            delete pT;
    }

    // Load local tunings.
    CString sPath;
    sPath.Format(TEXT("%slocal_tunings%s"), CMainFrame::GetDefaultDirectory(DIR_TUNING), CTuningCollection::s_FileExtension);
    s_pTuningsSharedLocal->SetSavefilePath(sPath);
    s_pTuningsSharedLocal->Deserialize();

    // Enabling adding/removing of tunings for standard collection
    // only for debug builds.
    #ifdef DEBUG
        s_pTuningsSharedBuiltIn->SetConstStatus(CTuningCollection::EM_ALLOWALL);
    #else
        s_pTuningsSharedBuiltIn->SetConstStatus(CTuningCollection::EM_CONST);
    #endif

    modplug::tracker::modinstrument_t::s_DefaultTuning = NULL;

    return false;
}



void module_renderer::SetDefaultInstrumentValues(modplug::tracker::modinstrument_t *pIns)
//--------------------------------------------------------------
{
    pIns->resampling_mode = m_defaultInstrument.resampling_mode;
    pIns->default_filter_mode = m_defaultInstrument.default_filter_mode;
    pIns->pitch_envelope.release_node = m_defaultInstrument.pitch_envelope.release_node;
    pIns->panning_envelope.release_node = m_defaultInstrument.panning_envelope.release_node;
    pIns->volume_envelope.release_node = m_defaultInstrument.volume_envelope.release_node;
    pIns->pTuning = m_defaultInstrument.pTuning;
    pIns->nPluginVelocityHandling = m_defaultInstrument.nPluginVelocityHandling;
    pIns->nPluginVolumeHandling = m_defaultInstrument.nPluginVolumeHandling;

}


long module_renderer::GetSampleOffset()
//--------------------------------
{
    //TODO: This is where we could inform patterns of the exact song position when playback starts.
    //order: m_nNextPattern
    //long ticksFromStartOfPattern = m_nRow*m_nMusicSpeed;
    //return ticksFromStartOfPattern*m_nSamplesPerTick;
    return 0;
}

string module_renderer::GetNoteName(const CTuning::NOTEINDEXTYPE& note, const modplug::tracker::instrumentindex_t inst) const
//--------------------------------------------------------------------------------------------------
{
    if((inst >= MAX_INSTRUMENTS && inst != modplug::tracker::InstrumentIndexInvalid) || note < 1 || note > NoteMax) return "BUG";

    // For MPTM instruments with custom tuning, find the appropriate note name. Else, use default note names.
    if(inst != modplug::tracker::InstrumentIndexInvalid && m_nType == MOD_TYPE_MPT && Instruments[inst] && Instruments[inst]->pTuning)
        return Instruments[inst]->pTuning->GetNoteName(note - NoteMiddleC);
    else
        return szDefaultNoteNames[note - 1];
}


void module_renderer::SetModSpecsPointer(const CModSpecifications*& pModSpecs, const MODTYPE type)
//------------------------------------------------------------------------------------------
{
    switch(type)
    {
        case MOD_TYPE_MPT:
            pModSpecs = &ModSpecs::mptm;
        break;

        case MOD_TYPE_IT:
            pModSpecs = &ModSpecs::itEx;
        break;

        case MOD_TYPE_XM:
            pModSpecs = &ModSpecs::xmEx;
        break;

        case MOD_TYPE_S3M:
            pModSpecs = &ModSpecs::s3mEx;
        break;

        case MOD_TYPE_MOD:
        default:
            pModSpecs = &ModSpecs::modEx;
            break;
    }
}

uint16_t module_renderer::GetModFlagMask(const MODTYPE oldtype, const MODTYPE newtype) const
//-----------------------------------------------------------------------------------
{
    const MODTYPE combined = oldtype | newtype;

    // XM <-> IT/MPT conversion.
    if(combined == (MOD_TYPE_IT|MOD_TYPE_XM) || combined == (MOD_TYPE_MPT|MOD_TYPE_XM))
        return (1 << MSF_COMPATIBLE_PLAY) | (1 << MSF_MIDICC_BUGEMULATION);

    // IT <-> MPT conversion.
    if(combined == (MOD_TYPE_IT|MOD_TYPE_MPT))
        return UINT16_MAX;

    return (1 << MSF_COMPATIBLE_PLAY);
}

void module_renderer::ChangeModTypeTo(const MODTYPE& newType)
//------------------------------------------------------
{
    const MODTYPE oldtype = m_nType;
    if (oldtype == newType)
        return;
    m_nType = newType;
    SetModSpecsPointer(m_pModSpecs, m_nType);
    SetupMODPanning(); // Setup LRRL panning scheme if needed
    SetupITBidiMode(); // Setup IT bidi mode

    m_ModFlags = m_ModFlags & GetModFlagMask(oldtype, newType);

    Order.OnModTypeChanged(oldtype);
    Patterns.OnModTypeChanged(oldtype);
}


bool module_renderer::SetTitle(const char* titleCandidate, size_t strSize)
//-------------------------------------------------------------------
{
    if (song_name.compare(titleCandidate)) {
        song_name.assign(titleCandidate);
        return true;
    }
    return false;
}


double module_renderer::GetPlaybackTimeAt(modplug::tracker::orderindex_t ord, modplug::tracker::rowindex_t row, bool updateVars)
//---------------------------------------------------------------------------------
{
    const GetLengthType t = GetLength(updateVars ? eAdjust : eNoAdjust, ord, row);
    if(t.targetReached) return t.duration;
    else return -1; //Given position not found from play sequence.
}


const CModSpecifications& module_renderer::GetModSpecifications(const MODTYPE type)
//----------------------------------------------------------------------------
{
    const CModSpecifications* p = nullptr;
    SetModSpecsPointer(p, type);
    return *p;
}


// Set up channel panning and volume suitable for MOD + similar files. If the current mod type is not MOD, bForceSetup has to be set to true.
void module_renderer::SetupMODPanning(bool bForceSetup)
//------------------------------------------------
{
    // Setup LRRL panning, bad_max channel volume
    if((m_nType & MOD_TYPE_MOD) == 0 && bForceSetup == false) return;

    for(modplug::tracker::chnindex_t nChn = 0; nChn < MAX_BASECHANNELS; nChn++)
    {
        ChnSettings[nChn].nVolume = 64;
        if(deprecated_global_sound_setup_bitmask & SNDMIX_MAXDEFAULTPAN)
            ChnSettings[nChn].nPan = (((nChn & 3) == 1) || ((nChn & 3) == 2)) ? 256 : 0;
        else
            ChnSettings[nChn].nPan = (((nChn & 3) == 1) || ((nChn & 3) == 2)) ? 0xC0 : 0x40;
    }
}


// Set or unset the IT bidi loop mode (see Fastmix.cpp for an explanation). The variable has to be static...
void module_renderer::SetupITBidiMode()
//--------------------------------
{
    m_bITBidiMode = IsCompatibleMode(TRK_IMPULSETRACKER);
}
