#ifndef MOD_SPECIFICATIONS_H
#define MOD_SPECIFICATIONS_H

#include "../tracker/types.hpp"
#include "../tracker/modevent.hpp"
#include "../tracker/tracker.hpp"
#include "../SoundFilePlayConfig.h" // mixlevel constants.

struct CModSpecifications
//=======================
{
    // Return true if format supports given note.
    bool HasNote(modplug::tracker::note_t note) const;
    bool HasVolCommand(modplug::tracker::volcmd_t volcmd) const;
    bool HasCommand(modplug::tracker::cmd_t cmd) const;
    // Return corresponding effect letter for this format
    char GetEffectLetter(modplug::tracker::cmd_t cmd) const;
    char GetVolEffectLetter(modplug::tracker::volcmd_t cmd) const;

    // NOTE: If changing order, update all initializations below.
    char fileExtension[6];          // File extension without dot.
    modplug::tracker::note_t noteMin; // Minimum note index (index starts from 1)
    modplug::tracker::note_t noteMax; // Maximum note index (index starts from 1)
    bool hasNoteCut;                  // True if format has notecut.
    bool hasNoteOff;                  // True if format has noteoff.
    bool hasNoteFade;                  // True if format has notefade.
    modplug::tracker::patternindex_t patternsMax;
    modplug::tracker::orderindex_t ordersMax;
    modplug::tracker::chnindex_t channelsMin; // Minimum number of editable channels in pattern.
    modplug::tracker::chnindex_t channelsMax; // Maximum number of editable channels in pattern.
    TEMPO tempoMin;
    TEMPO tempoMax;
    modplug::tracker::rowindex_t patternRowsMin;
    modplug::tracker::rowindex_t patternRowsMax;
    uint16_t modNameLengthMax;                // Meaning 'usable letters', possible null character is not included.
    uint16_t sampleNameLengthMax;                // Dito
    uint16_t sampleFilenameLengthMax;        // Dito
    uint16_t instrNameLengthMax;                // Dito
    uint16_t instrFilenameLengthMax;        // Dito
    modplug::tracker::sampleindex_t samplesMax;
    modplug::tracker::instrumentindex_t instrumentsMax;
    uint8_t defaultMixLevels;
    uint8_t MIDIMappingDirectivesMax;
    uint32_t speedMin;                                        // Minimum ticks per frame
    uint32_t speedMax;                                        // Maximum ticks per frame
    bool hasComments;                                // True if format has a comments field
    uint32_t envelopePointsMax;                        // Maximum number of points of each envelope
    bool hasReleaseNode;                        // Envelope release node
    char commands[modplug::tracker::CmdMax + 1]; // An array holding all commands this format supports; commands that are not supported are marked with "?"
    char volcommands[modplug::tracker::VolCmdMax + 1]; // dito, but for volume column
    bool hasIgnoreIndex;                        // Does "+++" pattern exist?
    bool hasRestartPos;
    bool supportsPlugins;
    bool hasPatternSignatures;                // Can patterns have a custom time signature?
};


namespace ModSpecs
{

const CModSpecifications mptm =
{
    /*
    TODO: Proper, less arbitrarily chosen values here.
    NOTE: If changing limits, see whether:
                    -savefile format and GUI methods can handle new values(might not be a small task :).
     */
    "mptm",                                 // File extension
    modplug::tracker::NoteMin,              // Minimum note index
    modplug::tracker::NoteMax,              // Maximum note index
    true,                                   // Has notecut.
    true,                                   // Has noteoff.
    true,                                   // Has notefade.
    4000,                                   // Pattern bad_max.
    modplug::tracker::orderindex_t(4000),   // Order bad_max.
    1,                                      // Channel bad_min
    127,                                    // Channel bad_max
    32,                                     // Min tempo
    512,                                    // Max tempo
    1,                                      // Min pattern rows
    1024,                                   // Max pattern rows
    25,                                     // Max mod name length
    26,                                     // Max sample name length
    12,                                     // Max sample filename length
    26,                                     // Max instrument name length
    12,                                     // Max instrument filename length
    4000,                                   // SamplesMax
    256,                                    // instrumentMax
    mixLevels_117RC3,                       // defaultMixLevels
    200,                                    // Max MIDI mapping directives
    1,                                      // Min Speed
    255,                                    // Max Speed
    true,                                   // Has song comments
    MAX_ENVPOINTS,                          // Envelope point count
    true,                                   // Has envelope release node
    " JFEGHLKRXODB?CQATI?SMNVW?UY?P?Z\\:#", // Supported Effects
    " vpcdabuhlrgfe?o",                     // Supported Volume Column commands
    true,                                   // Has "+++" pattern
    true,                                   // Has restart position (order)
    true,                                   // Supports plugins
    true,                                   // Custom pattern time signatures
};




const CModSpecifications mod =
{
    // TODO: Set correct values.
    "mod",                                 // File extension
    modplug::tracker::note_t(37),          // Minimum note index
    modplug::tracker::note_t(108),         // Maximum note index
    false,                                 // No notecut.
    false,                                 // No noteoff.
    false,                                 // No notefade.
    128,                                   // Pattern bad_max.
    modplug::tracker::orderindex_t(128),   // Order bad_max.
    4,                                     // Channel bad_min
    32,                                    // Channel bad_max
    32,                                    // Min tempo
    255,                                   // Max tempo
    64,                                    // Min pattern rows
    64,                                    // Max pattern rows
    20,                                    // Max mod name length
    22,                                    // Max sample name length
    0,                                     // Max sample filename length
    0,                                     // Max instrument name length
    0,                                     // Max instrument filename length
    31,                                    // SamplesMax
    0,                                     // instrumentMax
    mixLevels_compatible,                  // defaultMixLevels
    0,                                     // Max MIDI mapping directives
    1,                                     // Min Speed
    32,                                    // Max Speed
    false,                                 // No song comments
    0,                                     // No instrument envelopes
    false,                                 // No envelope release node
    " 0123456789ABCD?FF?E???????????????", // Supported Effects
    " ???????????????",                    // Supported Volume Column commands
    false,                                 // Doesn't have "+++" pattern
    true,                                  // Has restart position (order)
    false,                                 // Doesn't support plugins
    false,                                 // No custom pattern time signatures
};

// MOD with MPT extensions.
const CModSpecifications modEx =
{
    // TODO: Set correct values.
    "mod",                                 // File extension
    modplug::tracker::note_t(37),          // Minimum note index
    modplug::tracker::note_t(108),         // Maximum note index
    false,                                 // No notecut.
    false,                                 // No noteoff.
    false,                                 // No notefade.
    128,                                   // Pattern bad_max.
    modplug::tracker::orderindex_t(128),   // Order bad_max.
    4,                                     // Channel bad_min
    32,                                    // Channel bad_max
    32,                                    // Min tempo
    255,                                   // Max tempo
    64,                                    // Min pattern rows
    64,                                    // Max pattern rows
    20,                                    // Max mod name length
    22,                                    // Max sample name length
    0,                                     // Max sample filename length
    0,                                     // Max instrument name length
    0,                                     // Max instrument filename length
    31,                                    // SamplesMax
    0,                                     // instrumentMax
    mixLevels_compatible,                  // defaultMixLevels
    0,                                     // Max MIDI mapping directives
    1,                                     // Min Speed
    32,                                    // Max Speed
    false,                                 // No song comments
    0,                                     // No instrument envelopes
    false,                                 // No envelope release node
    " 0123456789ABCD?FF?E???????????????", // Supported Effects
    " ???????????????",                    // Supported Volume Column commands
    false,                                 // Doesn't have "+++" pattern
    true,                                  // Has restart position (order)
    false,                                 // Doesn't support plugins
    false,                                 // No custom pattern time signatures
};

const CModSpecifications xm =
{
    // TODO: Set correct values.
    "xm",                                  // File extension
    modplug::tracker::note_t(13),          // Minimum note index
    modplug::tracker::note_t(108),         // Maximum note index
    false,                                 // No notecut.
    true,                                  // Has noteoff.
    false,                                 // No notefade.
    256,                                   // Pattern bad_max.
    modplug::tracker::orderindex_t(255),   // Order bad_max.
    1,                                     // Channel bad_min
    32,                                    // Channel bad_max
    32,                                    // Min tempo
    255,                                   // Max tempo
    1,                                     // Min pattern rows
    256,                                   // Max pattern rows
    20,                                    // Max mod name length
    22,                                    // Max sample name length
    0,                                     // Max sample filename length
    22,                                    // Max instrument name length
    0,                                     // Max instrument filename length
    128 * 16,                              // SamplesMax (actually 16 per instrument)
    128,                                   // instrumentMax
    mixLevels_compatible,                  // defaultMixLevels
    0,                                     // Max MIDI mapping directives
    1,                                     // Min Speed
    31,                                    // Max Speed
    false,                                 // No song comments
    12,                                    // Envelope point count
    false,                                 // No envelope release node
    " 0123456789ABCDRFFTE???GHK??XPL????", // Supported Effects
    " vpcdabuhlrg????",                    // Supported Volume Column commands
    false,                                 // Doesn't have "+++" pattern
    true,                                  // Has restart position (order)
    false,                                 // Doesn't support plugins
    false,                                 // No custom pattern time signatures
};

// XM with MPT extensions
const CModSpecifications xmEx =
{
    // TODO: Set correct values.
    "xm",                                   // File extension
    modplug::tracker::note_t(13),           // Minimum note index
    modplug::tracker::note_t(108),          // Maximum note index
    false,                                  // No notecut.
    true,                                   // Has noteoff.
    false,                                  // No notefade.
    256,                                    // Pattern bad_max.
    modplug::tracker::orderindex_t(255),    // Order bad_max.
    1,                                      // Channel bad_min
    127,                                    // Channel bad_max
    32,                                     // Min tempo
    512,                                    // Max tempo
    1,                                      // Min pattern rows
    1024,                                   // Max pattern rows
    20,                                     // Max mod name length
    22,                                     // Max sample name length
    0,                                      // Max sample filename length
    22,                                     // Max instrument name length
    0,                                      // Max instrument filename length
    4000,                                   // SamplesMax (actually 16 per instrument(256*16=4096), but limited to MAX_SAMPLES=4000)
    256,                                    // instrumentMax
    mixLevels_compatible,                   // defaultMixLevels
    200,                                    // Max MIDI mapping directives
    1,                                      // Min Speed
    31,                                     // Max Speed
    true,                                   // Has song comments
    12,                                     // Envelope point count
    false,                                  // No envelope release node
    " 0123456789ABCDRFFTE???GHK?YXPLZ\\?#", // Supported Effects
    " vpcdabuhlrgfe?o",                     // Supported Volume Column commands
    false,                                  // Doesn't have "+++" pattern
    true,                                   // Has restart position (order)
    true,                                   // Supports plugins
    false,                                  // No custom pattern time signatures
};

const CModSpecifications s3m =
{
    // TODO: Set correct values.
    "s3m",                                 // File extension
    modplug::tracker::note_t(13),          // Minimum note index
    modplug::tracker::note_t(108),         // Maximum note index
    true,                                  // Has notecut.
    false,                                 // No noteoff.
    false,                                 // No notefade.
    100,                                   // Pattern bad_max.
    modplug::tracker::orderindex_t(255),   // Order bad_max.
    1,                                     // Channel bad_min
    32,                                    // Channel bad_max
    33,                                    // Min tempo
    255,                                   // Max tempo
    64,                                    // Min pattern rows
    64,                                    // Max pattern rows
    27,                                    // Max mod name length
    27,                                    // Max sample name length
    12,                                    // Max sample filename length
    0,                                     // Max instrument name length
    0,                                     // Max instrument filename length
    99,                                    // SamplesMax
    0,                                     // instrumentMax
    mixLevels_compatible,                  // defaultMixLevels
    0,                                     // Max MIDI mapping directives
    1,                                     // Min Speed
    255,                                   // Max Speed
    false,                                 // No song comments
    0,                                     // No instrument envelopes
    false,                                 // No envelope release node
    " JFEGHLKRXODB?CQATI?SMNVW?U????????", // Supported Effects
    " vp?????????????",                    // Supported Volume Column commands
    true,                                  // Has "+++" pattern
    false,                                 // Doesn't have restart position (order)
    false,                                 // Doesn't support plugins
    false,                                 // No custom pattern time signatures
};

// S3M with MPT extensions
const CModSpecifications s3mEx =
{
    // TODO: Set correct values.
    "s3m",                                  // File extension
    modplug::tracker::note_t(13),           // Minimum note index
    modplug::tracker::note_t(108),          // Maximum note index
    true,                                   // Has notecut.
    false,                                  // No noteoff.
    false,                                  // No notefade.
    100,                                    // Pattern bad_max.
    modplug::tracker::orderindex_t(255),    // Order bad_max.
    1,                                      // Channel bad_min
    32,                                     // Channel bad_max
    33,                                     // Min tempo
    255,                                    // Max tempo
    64,                                     // Min pattern rows
    64,                                     // Max pattern rows
    27,                                     // Max mod name length
    27,                                     // Max sample name length
    12,                                     // Max sample filename length
    0,                                      // Max instrument name length
    0,                                      // Max instrument filename length
    99,                                     // SamplesMax
    0,                                      // instrumentMax
    mixLevels_compatible,                   // defaultMixLevels
    0,                                      // Max MIDI mapping directives
    1,                                      // Min Speed
    255,                                    // Max Speed
    false,                                  // No song comments
    0,                                      // No instrument envelopes
    false,                                  // No envelope release node
    " JFEGHLKRXODB?CQATI?SMNVW?UY?P?Z\\??", // Supported Effects
    " vp?????????????",                     // Supported Volume Column commands
    true,                                   // Has "+++" pattern
    false,                                  // Doesn't have restart position (order)
    false,                                  // Doesn't support plugins
    false,                                  // No custom pattern time signatures
};

const CModSpecifications it =
{
    // TODO: Set correct values.
    "it",                                  // File extension
    modplug::tracker::note_t(1),           // Minimum note index
    modplug::tracker::note_t(120),         // Maximum note index
    true,                                  // Has notecut.
    true,                                  // Has noteoff.
    true,                                  // Has notefade.
    200,                                   // Pattern bad_max.
    modplug::tracker::orderindex_t(256),   // Order bad_max.
    1,                                     // Channel bad_min
    64,                                    // Channel bad_max
    32,                                    // Min tempo
    255,                                   // Max tempo
    1,                                     // Min pattern rows
    200,                                   // Max pattern rows
    25,                                    // Max mod name length
    25,                                    // Max sample name length
    12,                                    // Max sample filename length
    25,                                    // Max instrument name length
    12,                                    // Max instrument filename length
    99,                                    // SamplesMax
    99,                                    // instrumentMax
    mixLevels_compatible,                  // defaultMixLevels
    0,                                     // Max MIDI mapping directives
    1,                                     // Min Speed
    255,                                   // Max Speed
    true,                                  // Has song comments
    25,                                    // Envelope point count
    false,                                 // No envelope release node
    " JFEGHLKRXODB?CQATI?SMNVW?UY?P?Z???", // Supported Effects
    " vpcdab?h??gfe??",                    // Supported Volume Column commands
    true,                                  // Has "+++" pattern
    false,                                 // Doesn't have restart position (order)
    false,                                 // Doesn't support plugins
    false,                                 // No custom pattern time signatures
};

const CModSpecifications itEx =
{
    // TODO: Set correct values.
    "it",                                   // File extension
    modplug::tracker::note_t(1),            // Minimum note index
    modplug::tracker::note_t(120),          // Maximum note index
    true,                                   // Has notecut.
    true,                                   // Has noteoff.
    true,                                   // Has notefade.
    240,                                    // Pattern bad_max.
    modplug::tracker::orderindex_t(256),    // Order bad_max.
    1,                                      // Channel bad_min
    127,                                    // Channel bad_max
    32,                                     // Min tempo
    512,                                    // Max tempo
    1,                                      // Min pattern rows
    1024,                                   // Max pattern rows
    25,                                     // Max mod name length
    25,                                     // Max sample name length
    12,                                     // Max sample filename length
    25,                                     // Max instrument name length
    12,                                     // Max instrument filename length
    4000,                                   // SamplesMax
    256,                                    // instrumentMax
    mixLevels_compatible,                   // defaultMixLevels
    200,                                    // Max MIDI mapping directives
    1,                                      // Min Speed
    255,                                    // Max Speed
    true,                                   // Has song comments
    25,                                     // Envelope point count
    false,                                  // No envelope release node
    " JFEGHLKRXODB?CQATI?SMNVW?UY?P?Z\\?#", // Supported Effects
    " vpcdab?h??gfe?o",                     // Supported Volume Column commands
    true,                                   // Has "+++" pattern
    true,                                   // Has restart position (order)
    true,                                   // Supports plugins
    false,                                  // No custom pattern time signatures
};

} // namespace ModSpecs



#endif
