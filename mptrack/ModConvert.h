/*
 * ModConvert.h
 * ------------
 * Purpose: Headers for module conversion code.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 *
 */


// Warning types
enum enmWarnings
{
	wInstrumentsToSamples = 0,
	wResizedPatterns,
	wSampleBidiLoops,
	wSampleSustainLoops,
	wSampleAutoVibrato,
	wMODSampleFrequency,
	wBrokenNoteMap,
	wInstrumentSustainLoops,
	wInstrumentTuning,
	wMODGlobalVars,
	wMOD31Samples,
	wRestartPos,
	wChannelVolSurround,
	wChannelPanning,
	wPatternSignatures,
	wLinearSlides,
	wTrimmedEnvelopes,
	wReleaseNode,
};