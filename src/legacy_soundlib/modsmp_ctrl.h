/*
 * modinstrument_t related functions.
 */

#ifndef MODSMP_CTRL_H
#define MODSMP_CTRL_H

#include "Sndfile.h"

namespace ctrlSmp
{

typedef uintptr_t SmpLength;

enum ResetFlag
{
    SmpResetCompo = 1,
    SmpResetInit,
    SmpResetVibrato,
};

// Insert silence to given location.
// Note: Is currently implemented only for inserting silence to the beginning and to the end of the sample.
// Return: Length of the new sample.
SmpLength InsertSilence(modplug::tracker::modsample_t& smp, const SmpLength nSilenceLength, const SmpLength nStartFrom, module_renderer* pSndFile);

// Change sample size.
// Note: If resized sample is bigger, silence will be added to the sample's tail.
// Return: Length of the new sample.
SmpLength ResizeSample(modplug::tracker::modsample_t& smp, const SmpLength nNewLength, module_renderer* pSndFile);

// Replaces sample in 'smp' with given sample and frees the old sample.
// If valid CSoundFile pointer is given, the sample will be replaced also from the sounds channels.
void ReplaceSample(modplug::tracker::modsample_t& smp, const LPSTR pNewSample,  const SmpLength nNewLength, module_renderer* pSndFile);

bool AdjustEndOfSample(modplug::tracker::modsample_t& smp, module_renderer* pSndFile = 0);

// Returns the number of bytes allocated(at least) for sample data.
// Note: Currently the return value is based on the sample length and the actual
//       allocation may be more than what this function returns.
inline SmpLength GetSampleCapacity(modplug::tracker::modsample_t& smp) {return smp.GetSampleSizeInBytes();}

// Resets samples.
void ResetSamples(module_renderer& rSndFile, ResetFlag resetflag);

// Remove DC offset and normalize.
// Return: If DC offset was removed, returns original offset value, zero otherwise.
float RemoveDCOffset(modplug::tracker::modsample_t& smp,
    				 SmpLength iStart,		// Start position (for partial DC offset removal).
    				 SmpLength iEnd,		// End position (for partial DC offset removal).
    				 const MODTYPE modtype,	// Used to determine whether to adjust global or default volume
    										// to keep volume level the same given the normalization.
    										// Volume adjustment is not done if this param is MOD_TYPE_NONE.
    				 module_renderer* const pSndFile); // Passed to AdjustEndOfSample.

// Reverse sample data
bool ReverseSample(modplug::tracker::modsample_t *pSmp, SmpLength iStart, SmpLength iEnd, module_renderer *pSndFile);

// Virtually unsign sample data
bool UnsignSample(modplug::tracker::modsample_t *pSmp, SmpLength iStart, SmpLength iEnd, module_renderer *pSndFile);

// Invert sample data (flip by 180 degrees)
bool InvertSample(modplug::tracker::modsample_t *pSmp, SmpLength iStart, SmpLength iEnd, module_renderer *pSndFile);

// Crossfade sample data to create smooth loops
bool XFadeSample(modplug::tracker::modsample_t *pSmp, SmpLength iFadeLength, module_renderer *pSndFile);

// Convert a sample with any number of channels to mono
bool ConvertToMono(modplug::tracker::modsample_t *pSmp, module_renderer *pSndFile);

} // Namespace ctrlSmp

namespace ctrlChn
{

// Replaces sample from sound channels by given sample.
void ReplaceSample( modplug::tracker::modchannel_t (&Chn)[MAX_VIRTUAL_CHANNELS],
    				LPCSTR pOldSample,
    				LPSTR pNewSample,
    				const ctrlSmp::SmpLength nNewLength,
    				uint32_t orFlags = 0,
    				uint32_t andFlags = MAXDWORD);

} // namespace ctrlChn

#endif