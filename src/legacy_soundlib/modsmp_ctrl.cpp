/*
 * modplug::tracker::modsample_t related functions.
 */

#include "stdafx.h"
#include "modsmp_ctrl.h"
#include "../MainFrm.h"

#define new DEBUG_NEW

namespace ctrlSmp
{

void ReplaceSample(modplug::tracker::modsample_t& smp, const LPSTR pNewSample, const SmpLength nNewLength, module_renderer* pSndFile)
//----------------------------------------------------------------------------------------------------------
{
    LPSTR const pOldSmp = smp.sample_data;
    uint32_t dwOrFlags = 0;
    uint32_t dwAndFlags = MAXDWORD;
    if(smp.flags & CHN_16BIT)
    	dwOrFlags |= CHN_16BIT;
    else
    	dwAndFlags &= ~CHN_16BIT;
    if(smp.flags & CHN_STEREO)
    	dwOrFlags |= CHN_STEREO;
    else
    	dwAndFlags &= ~CHN_STEREO;

    BEGIN_CRITICAL();
    	if (pSndFile != nullptr)
    		ctrlChn::ReplaceSample(pSndFile->Chn, pOldSmp, pNewSample, nNewLength, dwOrFlags, dwAndFlags);
    	smp.sample_data = pNewSample;
    	smp.length = nNewLength;
    	module_renderer::FreeSample(pOldSmp);
    END_CRITICAL();
}


SmpLength InsertSilence(modplug::tracker::modsample_t& smp, const SmpLength nSilenceLength, const SmpLength nStartFrom, module_renderer* pSndFile)
//-----------------------------------------------------------------------------------------------------------------------
{
    if(nSilenceLength == 0 || nSilenceLength >= MAX_SAMPLE_LENGTH || smp.length > MAX_SAMPLE_LENGTH - nSilenceLength)
    	return smp.length;

    const SmpLength nOldBytes = smp.GetSampleSizeInBytes();
    const SmpLength nSilenceBytes = nSilenceLength * smp.GetBytesPerSample();
    const SmpLength nNewSmpBytes = nOldBytes + nSilenceBytes;
    const SmpLength nNewLength = smp.length + nSilenceLength;

    LPSTR pNewSmp = 0;
    if( GetSampleCapacity(smp) >= nNewSmpBytes ) // If sample has room to expand.
    {
    	AfxMessageBox("Not implemented: GetSampleCapacity(smp) >= nNewSmpBytes");
    	// Not implemented, GetSampleCapacity() currently always returns length based value
    	// even if there is unused space in the sample.
    }
    else // Have to allocate new sample.
    {
    	pNewSmp = module_renderer::AllocateSample(nNewSmpBytes);
    	if(pNewSmp == 0)
    		return smp.length; //Sample allocation failed.
    	if(nStartFrom == 0)
    	{
    		memset(pNewSmp, 0, nSilenceBytes);
    		memcpy(pNewSmp + nSilenceBytes, smp.sample_data, nOldBytes);
    	}
    	else if(nStartFrom == smp.length)
    	{
    		memcpy(pNewSmp, smp.sample_data, nOldBytes);
    		memset(pNewSmp + nOldBytes, 0, nSilenceBytes);
    	}
    	else
    		AfxMessageBox(TEXT("Unsupported start position in InsertSilence."));
    }

    // Set loop points automatically
    if(nOldBytes == 0)
    {
    	smp.loop_start = 0;
    	smp.loop_end = nNewLength;
    	smp.flags |= CHN_LOOP;
    } else
    {
    	if(smp.loop_start >= nStartFrom) smp.loop_start += nSilenceLength;
    	if(smp.loop_end >= nStartFrom) smp.loop_end += nSilenceLength;
    }

    ReplaceSample(smp, pNewSmp, nNewLength, pSndFile);
    AdjustEndOfSample(smp, pSndFile);

    return smp.length;
}


SmpLength ResizeSample(modplug::tracker::modsample_t& smp, const SmpLength nNewLength, module_renderer* pSndFile)
//--------------------------------------------------------------------------------------
{
    // Invalid sample size
    if(nNewLength > MAX_SAMPLE_LENGTH || nNewLength == smp.length)
    	return smp.length;

    // New sample will be bigger so we'll just use "InsertSilence" as it's already there.
    if(nNewLength > smp.length)
    	return InsertSilence(smp, nNewLength - smp.length, smp.length, pSndFile);

    // Else: Shrink sample

    const SmpLength nNewSmpBytes = nNewLength * smp.GetBytesPerSample();

    LPSTR pNewSmp = 0;
    pNewSmp = module_renderer::AllocateSample(nNewSmpBytes);
    if(pNewSmp == 0)
    	return smp.length; //Sample allocation failed.

    // Copy over old data and replace sample by the new one
    memcpy(pNewSmp, smp.sample_data, nNewSmpBytes);
    ReplaceSample(smp, pNewSmp, nNewLength, pSndFile);

    // Adjust loops
    if(smp.loop_start > nNewLength)
    {
    	smp.loop_start = smp.loop_end = 0;
    	smp.flags &= ~CHN_LOOP;
    }
    if(smp.loop_end > nNewLength) smp.loop_end = nNewLength;
    if(smp.sustain_start > nNewLength)
    {
    	smp.sustain_start = smp.sustain_end = 0;
    	smp.flags &= ~CHN_SUSTAINLOOP;
    }
    if(smp.sustain_end > nNewLength) smp.sustain_end = nNewLength;

    AdjustEndOfSample(smp, pSndFile);

    return smp.length;
}

namespace // Unnamed namespace for local implementation functions.
{


template <class T>
void AdjustEndOfSampleImpl(modplug::tracker::modsample_t& smp)
//----------------------------------------
{
    modplug::tracker::modsample_t* const pSmp = &smp;
    const UINT len = pSmp->length;
    T* p = reinterpret_cast<T*>(pSmp->sample_data);
    if (pSmp->flags & CHN_STEREO)
    {
    	p[(len+3)*2] = p[(len+2)*2] = p[(len+1)*2] = p[(len)*2] = p[(len-1)*2];
    	p[(len+3)*2+1] = p[(len+2)*2+1] = p[(len+1)*2+1] = p[(len)*2+1] = p[(len-1)*2+1];
    } else
    {
    	p[len+4] = p[len+3] = p[len+2] = p[len+1] = p[len] = p[len-1];
    }
    if (((pSmp->flags & (CHN_LOOP|CHN_PINGPONGLOOP|CHN_STEREO)) == CHN_LOOP)
     && (pSmp->loop_end == pSmp->length)
     && (pSmp->loop_end > pSmp->loop_start) && (pSmp->length > 2))
    {
    	p[len] = p[pSmp->loop_start];
    	p[len+1] = p[pSmp->loop_start+1];
    	p[len+2] = p[pSmp->loop_start+2];
    	p[len+3] = p[pSmp->loop_start+3];
    	p[len+4] = p[pSmp->loop_start+4];
    }
}

} // unnamed namespace.


bool AdjustEndOfSample(modplug::tracker::modsample_t& smp, module_renderer* pSndFile)
//----------------------------------------------------------
{
    modplug::tracker::modsample_t* const pSmp = &smp;

    if ((!pSmp->length) || (!pSmp->sample_data))
    	return false;

    BEGIN_CRITICAL();

    if (pSmp->GetElementarySampleSize() == 2)
    	AdjustEndOfSampleImpl<int16_t>(*pSmp);
    else if(pSmp->GetElementarySampleSize() == 1)
    	AdjustEndOfSampleImpl<int8_t>(*pSmp);

    // Update channels with new loop values
    if(pSndFile != nullptr)
    {
    	module_renderer& rSndFile = *pSndFile;
    	for (UINT i=0; i<MAX_VIRTUAL_CHANNELS; i++) if ((rSndFile.Chn[i].sample == pSmp) && (rSndFile.Chn[i].length))
    	{
    		if ((pSmp->loop_start + 3 < pSmp->loop_end) && (pSmp->loop_end <= pSmp->length))
    		{
    			rSndFile.Chn[i].loop_start = pSmp->loop_start;
    			rSndFile.Chn[i].loop_end = pSmp->loop_end;
    			rSndFile.Chn[i].length = pSmp->loop_end;
    			if (rSndFile.Chn[i].sample_position > rSndFile.Chn[i].length)
    			{
    				rSndFile.Chn[i].sample_position = rSndFile.Chn[i].loop_start;
    				rSndFile.Chn[i].flags &= ~CHN_PINGPONGFLAG;
    			}
    			uint32_t d = rSndFile.Chn[i].flags & ~(CHN_PINGPONGLOOP|CHN_LOOP);
    			if (pSmp->flags & CHN_LOOP)
    			{
    				d |= CHN_LOOP;
    				if (pSmp->flags & CHN_PINGPONGLOOP) d |= CHN_PINGPONGLOOP;
    			}
    			rSndFile.Chn[i].flags = d;
    		} else
    		if (!(pSmp->flags & CHN_LOOP))
    		{
    			rSndFile.Chn[i].flags &= ~(CHN_PINGPONGLOOP|CHN_LOOP);
    		}
    	}
    }
    END_CRITICAL();
    return true;
}


void ResetSamples(module_renderer& rSndFile, ResetFlag resetflag)
//----------------------------------------------------------
{
    const UINT nSamples = rSndFile.GetNumSamples();
    for(UINT i = 1; i <= nSamples; i++)
    {
    	switch(resetflag)
    	{
    	case SmpResetInit:
    		strcpy(rSndFile.m_szNames[i], "");
    		strcpy(rSndFile.Samples[i].legacy_filename, "");
    		rSndFile.Samples[i].c5_samplerate = 8363;
    		// note: break is left out intentionally. keep this order or c&p the stuff from below if you change anything!
    	case SmpResetCompo:
    		rSndFile.Samples[i].default_pan = 128;
    		rSndFile.Samples[i].global_volume = 64;
    		rSndFile.Samples[i].default_volume = 256;
    		rSndFile.Samples[i].vibrato_depth = 0;
    		rSndFile.Samples[i].vibrato_rate = 0;
    		rSndFile.Samples[i].vibrato_sweep = 0;
    		rSndFile.Samples[i].vibrato_type = 0;
    		rSndFile.Samples[i].flags &= ~CHN_PANNING;
    		break;
    	case SmpResetVibrato:
    		rSndFile.Samples[i].vibrato_depth = 0;
    		rSndFile.Samples[i].vibrato_rate = 0;
    		rSndFile.Samples[i].vibrato_sweep = 0;
    		rSndFile.Samples[i].vibrato_type = 0;
    		break;
    	default:
    		break;
    	}
    }
}


namespace
{
    struct OffsetData
    {
    	double dMax, dMin, dOffset;
    };

    // Returns maximum sample amplitude for given sample type (int8_t/int16_t).
    template <class T>
    double GetMaxAmplitude() {return 1.0 + (std::numeric_limits<T>::max)();}

    // Calculates DC offset and returns struct with DC offset, max and min values.
    // DC offset value is average of [-1.0, 1.0[-normalized offset values.
    template<class T>
    OffsetData CalculateOffset(const T* pStart, const SmpLength nLength)
    //------------------------------------------------------------------
    {
    	OffsetData offsetVals = {0,0,0};

    	if(nLength < 1)
    		return offsetVals;

    	const double dMaxAmplitude = GetMaxAmplitude<T>();

    	double dMax = -1, dMin = 1, dSum = 0;

    	const T* p = pStart;
    	for (SmpLength i = 0; i < nLength; i++, p++)
    	{
    		const double dVal = double(*p) / dMaxAmplitude;
    		dSum += dVal;
    		if(dVal > dMax) dMax = dVal;
    		if(dVal < dMin) dMin = dVal;
    	}

    	offsetVals.dMax = dMax;
    	offsetVals.dMin = dMin;
    	offsetVals.dOffset = (-dSum / (double)(nLength));
    	return offsetVals;
    }

    template <class T>
    void RemoveOffsetAndNormalize(T* pStart, const SmpLength nLength, const double dOffset, const double dAmplify)
    //------------------------------------------------------------------------------------------------------------
    {
    	T* p = pStart;
    	for (UINT i = 0; i < nLength; i++, p++)
    	{
    		double dVal = (*p) * dAmplify + dOffset;
    		Limit(dVal, (std::numeric_limits<T>::min)(), (std::numeric_limits<T>::max)());
    		*p = static_cast<T>(dVal);
    	}
    }
};


// Remove DC offset
float RemoveDCOffset(modplug::tracker::modsample_t& smp,
    				 SmpLength iStart,
    				 SmpLength iEnd,
    				 const MODTYPE modtype,
    				 module_renderer* const pSndFile)
//----------------------------------------------
{
    if(smp.sample_data == nullptr || smp.length < 1)
    	return 0;

    modplug::tracker::modsample_t* const pSmp = &smp;

    if (iEnd > pSmp->length) iEnd = pSmp->length;
    if (iStart > iEnd) iStart = iEnd;
    if (iStart == iEnd)
    {
    	iStart = 0;
    	iEnd = pSmp->length;
    }

    iStart *= pSmp->GetNumChannels();
    iEnd *= pSmp->GetNumChannels();

    const double dMaxAmplitude = (pSmp->GetElementarySampleSize() == 2) ? GetMaxAmplitude<int16_t>() : GetMaxAmplitude<int8_t>();

    // step 1: Calculate offset.
    OffsetData oData = {0,0,0};
    if(pSmp->GetElementarySampleSize() == 2)
    	oData = CalculateOffset(reinterpret_cast<int16_t*>(pSmp->sample_data) + iStart, iEnd - iStart);
    else if(pSmp->GetElementarySampleSize() == 1)
    	oData = CalculateOffset(reinterpret_cast<int8_t*>(pSmp->sample_data) + iStart, iEnd - iStart);

    double dMin = oData.dMin, dMax = oData.dMax, dOffset = oData.dOffset;

    const float fReportOffset = (float)dOffset;

    if((int)(dOffset * dMaxAmplitude) == 0)
    	return 0;

    // those will be changed...
    dMax += dOffset;
    dMin += dOffset;

    // ... and that might cause distortion, so we will normalize this.
    const double dAmplify = 1 / max(dMax, -dMin);

    // step 2: centralize + normalize sample
    dOffset *= dMaxAmplitude * dAmplify;
    if(pSmp->GetElementarySampleSize() == 2)
    	RemoveOffsetAndNormalize( reinterpret_cast<int16_t*>(pSmp->sample_data) + iStart, iEnd - iStart, dOffset, dAmplify);
    else if(pSmp->GetElementarySampleSize() == 1)
    	RemoveOffsetAndNormalize( reinterpret_cast<int8_t*>(pSmp->sample_data) + iStart, iEnd - iStart, dOffset, dAmplify);

    // step 3: adjust global vol (if available)
    if((modtype & (MOD_TYPE_IT | MOD_TYPE_MPT)) && (iStart == 0) && (iEnd == pSmp->length * pSmp->GetNumChannels()))
    {
    	BEGIN_CRITICAL();
    	pSmp->global_volume = min((uint16_t)(pSmp->global_volume / dAmplify), 64);
    	for (CHANNELINDEX i = 0; i < MAX_VIRTUAL_CHANNELS; i++)
    	{
    		if(pSndFile->Chn[i].sample_data == pSmp->sample_data)
    		{
    			pSndFile->Chn[i].nGlobalVol = pSmp->global_volume;
    		}
    	}
    	END_CRITICAL();
    }

    AdjustEndOfSample(smp, pSndFile);

    return fReportOffset;
}


template <class T>
void ReverseSampleImpl(T* pStart, const SmpLength nLength)
//--------------------------------------------------------
{
    for(SmpLength i = 0; i < nLength / 2; i++)
    {
    	std::swap(pStart[i], pStart[nLength - 1 - i]);
    }
}

// Reverse sample data
bool ReverseSample(modplug::tracker::modsample_t *pSmp, SmpLength iStart, SmpLength iEnd, module_renderer *pSndFile)
//-----------------------------------------------------------------------------------------
{
    if(pSmp->sample_data == nullptr) return false;
    if(iEnd == 0 || iStart > pSmp->length || iEnd > pSmp->length)
    {
    	iStart = 0;
    	iEnd = pSmp->length;
    }

    if(iEnd - iStart < 2) return false;

    if(pSmp->GetBytesPerSample() == 8)		// unused (yet)
    	ReverseSampleImpl(reinterpret_cast<int64_t*>(pSmp->sample_data) + iStart, iEnd - iStart);
    else if(pSmp->GetBytesPerSample() == 4)	// 16 bit stereo
    	ReverseSampleImpl(reinterpret_cast<int32_t*>(pSmp->sample_data) + iStart, iEnd - iStart);
    else if(pSmp->GetBytesPerSample() == 2)	// 16 bit mono / 8 bit stereo
    	ReverseSampleImpl(reinterpret_cast<int16_t*>(pSmp->sample_data) + iStart, iEnd - iStart);
    else if(pSmp->GetBytesPerSample() == 1)	// 8 bit mono
    	ReverseSampleImpl(reinterpret_cast<int8_t*>(pSmp->sample_data) + iStart, iEnd - iStart);
    else
    	return false;

    AdjustEndOfSample(*pSmp, pSndFile);
    return true;
}


template <class T>
void UnsignSampleImpl(T* pStart, const SmpLength nLength)
//-------------------------------------------------------
{
    const T offset = (std::numeric_limits<T>::min)();
    for(SmpLength i = 0; i < nLength; i++)
    {
    	pStart[i] += offset;
    }
}

// Virtually unsign sample data
bool UnsignSample(modplug::tracker::modsample_t *pSmp, SmpLength iStart, SmpLength iEnd, module_renderer *pSndFile)
//----------------------------------------------------------------------------------------
{
    if(pSmp->sample_data == nullptr) return false;
    if(iEnd == 0 || iStart > pSmp->length || iEnd > pSmp->length)
    {
    	iStart = 0;
    	iEnd = pSmp->length;
    }
    iStart *= pSmp->GetNumChannels();
    iEnd *= pSmp->GetNumChannels();
    if(pSmp->GetElementarySampleSize() == 2)
    	UnsignSampleImpl(reinterpret_cast<int16_t*>(pSmp->sample_data) + iStart, iEnd - iStart);
    else if(pSmp->GetElementarySampleSize() == 1)
    	UnsignSampleImpl(reinterpret_cast<int8_t*>(pSmp->sample_data) + iStart, iEnd - iStart);
    else
    	return false;

    AdjustEndOfSample(*pSmp, pSndFile);
    return true;
}


template <class T>
void InvertSampleImpl(T* pStart, const SmpLength nLength)
//-------------------------------------------------------
{
    for(SmpLength i = 0; i < nLength; i++)
    {
    	pStart[i] = ~pStart[i];
    }
}

// Invert sample data (flip by 180 degrees)
bool InvertSample(modplug::tracker::modsample_t *pSmp, SmpLength iStart, SmpLength iEnd, module_renderer *pSndFile)
//----------------------------------------------------------------------------------------
{
    if(pSmp->sample_data == nullptr) return false;
    if(iEnd == 0 || iStart > pSmp->length || iEnd > pSmp->length)
    {
    	iStart = 0;
    	iEnd = pSmp->length;
    }
    iStart *= pSmp->GetNumChannels();
    iEnd *= pSmp->GetNumChannels();
    if(pSmp->GetElementarySampleSize() == 2)
    	InvertSampleImpl(reinterpret_cast<int16_t*>(pSmp->sample_data) + iStart, iEnd - iStart);
    else if(pSmp->GetElementarySampleSize() == 1)
    	InvertSampleImpl(reinterpret_cast<int8_t*>(pSmp->sample_data) + iStart, iEnd - iStart);
    else
    	return false;

    AdjustEndOfSample(*pSmp, pSndFile);
    return true;
}


template <class T>
void XFadeSampleImpl(T* pStart, const SmpLength nOffset, SmpLength nFadeLength)
//-----------------------------------------------------------------------------
{
    for(SmpLength i = 0; i <= nFadeLength; i++)
    {
    	double dPercentage = sqrt((double)i / (double)nFadeLength); // linear fades are boring
    	pStart[nOffset + i] = (T)(((double)pStart[nOffset + i]) * (1 - dPercentage) + ((double)pStart[i]) * dPercentage);
    }
}

// X-Fade sample data to create smooth loop transitions
bool XFadeSample(modplug::tracker::modsample_t *pSmp, SmpLength iFadeLength, module_renderer *pSndFile)
//----------------------------------------------------------------------------
{
    if(pSmp->sample_data == nullptr) return false;
    if(pSmp->loop_end <= pSmp->loop_start || pSmp->loop_end > pSmp->length) return false;
    if(pSmp->loop_start < iFadeLength) return false;

    SmpLength iStart = pSmp->loop_start - iFadeLength;
    SmpLength iEnd = pSmp->loop_end - iFadeLength;
    iStart *= pSmp->GetNumChannels();
    iEnd *= pSmp->GetNumChannels();
    iFadeLength *= pSmp->GetNumChannels();

    if(pSmp->GetElementarySampleSize() == 2)
    	XFadeSampleImpl(reinterpret_cast<int16_t*>(pSmp->sample_data) + iStart, iEnd - iStart, iFadeLength);
    else if(pSmp->GetElementarySampleSize() == 1)
    	XFadeSampleImpl(reinterpret_cast<int8_t*>(pSmp->sample_data) + iStart, iEnd - iStart, iFadeLength);
    else
    	return false;

    AdjustEndOfSample(*pSmp, pSndFile);
    return true;
}


template <class T>
void ConvertStereoToMonoImpl(T* pDest, const SmpLength length)
//------------------------------------------------------------
{
    const T* pEnd = pDest + length;
    for(T* pSource = pDest; pDest != pEnd; pDest++, pSource += 2)
    {
    	*pDest = (pSource[0] + pSource[1] + 1) >> 1;
    }
}


// Convert a multichannel sample to mono (currently only implemented for stereo)
bool ConvertToMono(modplug::tracker::modsample_t *pSmp, module_renderer *pSndFile)
//-------------------------------------------------------
{
    if(pSmp->sample_data == nullptr || pSmp->length == 0 || pSmp->GetNumChannels() != 2) return false;

    // Note: Sample is overwritten in-place! Unused data is not deallocated!
    if(pSmp->GetElementarySampleSize() == 2)
    	ConvertStereoToMonoImpl(reinterpret_cast<int16_t*>(pSmp->sample_data), pSmp->length);
    else if(pSmp->GetElementarySampleSize() == 1)
    	ConvertStereoToMonoImpl(reinterpret_cast<int8_t*>(pSmp->sample_data), pSmp->length);
    else
    	return false;

    BEGIN_CRITICAL();
    pSmp->flags &= ~(CHN_STEREO);
    for (CHANNELINDEX i = 0; i < MAX_VIRTUAL_CHANNELS; i++)
    {
    	if(pSndFile->Chn[i].sample_data == pSmp->sample_data)
    	{
    		pSndFile->Chn[i].flags &= ~CHN_STEREO;
    	}
    }
    END_CRITICAL();

    AdjustEndOfSample(*pSmp, pSndFile);
    return true;
}


} // namespace ctrlSmp



namespace ctrlChn
{

void ReplaceSample( modplug::tracker::modchannel_t (&Chn)[MAX_VIRTUAL_CHANNELS],
    				LPCSTR pOldSample,
    				LPSTR pNewSample,
    				const ctrlSmp::SmpLength nNewLength,
    				uint32_t orFlags /* = 0*/,
    				uint32_t andFlags /* = MAXDWORD*/)
{
    for (CHANNELINDEX i = 0; i < MAX_VIRTUAL_CHANNELS; i++)
    {
    	if (Chn[i].sample_data == pOldSample)
    	{
    		Chn[i].sample_data = pNewSample;
    		if (Chn[i].active_sample_data != nullptr)
    			Chn[i].active_sample_data = pNewSample;
    		if (Chn[i].sample_position > nNewLength)
    			Chn[i].sample_position = 0;
    		if (Chn[i].length > 0)
    			Chn[i].length = nNewLength;
    		Chn[i].flags |= orFlags;
    		Chn[i].flags &= andFlags;
    	}
    }
}

} // namespace ctrlChn