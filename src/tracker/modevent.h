#pragma once

#include <algorithm>
#include <cstdint>


namespace modplug {
namespace tracker {

typedef uint8_t note_t;
typedef uint8_t instr_t;
typedef uint8_t vol_t;
typedef uint8_t param_t;

enum volcmd_t {
    VolCmdMin            = 0,

    VolCmdNone           = VolCmdMin,
    VolCmdVol            = 1,
    VolCmdPan            = 2,
    VolCmdSlideUp        = 3,
    VolCmdSlideDown      = 4,
    VolCmdFineUp         = 5,
    VolCmdFineDown       = 6,
    VolCmdVibratoSpeed   = 7,
    VolCmdVibratoDepth   = 8,
    VolCmdPanSlideLeft   = 9,
    VolCmdPanSlideRight  = 10,
    VolCmdPortamento     = 11,
    VolCmdPortamentoUp   = 12,
    VolCmdPortamentoDown = 13,
    VolCmdDelayCut       = 14,
    VolCmdOffset         = 15,

    VolCmdMax
};

enum cmd_t {
    CmdMin = 0,

    CmdNone                 = CmdMin,
    CmdArpeggio             = 1,
    CmdPortaUp              = 2,
    CmdPortaDown            = 3,
    CmdPorta                = 4,
    CmdVibrato              = 5,
    CmdPortaVolSlide        = 6,
    CmdVibratoVolSlide      = 7,
    CmdTremolo              = 8,
    CmdPanning8             = 9,
    CmdOffset               = 10,
    CmdVolSlide             = 11,
    CmdPositionJump         = 12,
    CmdVol                  = 13,
    CmdPatternBreak         = 14,
    CmdRetrig               = 15,
    CmdSpeed                = 16,
    CmdTempo                = 17,
    CmdTremor               = 18,
    CmdModCmdEx             = 19,
    CmdS3mCmdEx             = 20,
    CmdChannelVol           = 21,
    CmdChannelVolSlide      = 22,
    CmdGlobalVol            = 23,
    CmdGlobalVolSlide       = 24,
    CmdKeyOff               = 25,
    CmdFineVibrato          = 26,
    CmdPanbrello            = 27,
    CmdExtraFinePortaUpDown = 28,
    CmdPanningSlide         = 29,
    CmdSetEnvelopePosition  = 30,
    CmdMidi                 = 31,
    CmdSmoothMidi           = 32,
    CmdDelayCut             = 33,
    CmdExtendedParameter    = 34, // extended pattern effect parameter mechanism
    CmdNoteSlideUp          = 35, // IMF Gxy
    CmdNoteSlideDown        = 36, // IMF Hxy

    CmdMax
};

const note_t NoteNone    = 0;
const note_t NoteMiddleC = 5 * 12 + 1;
const note_t NoteKeyOff  = 255;
const note_t NoteNoteCut = 254;

// 253, IT's action for illegal notes
// DO NOT SAVE AS 253 as this is IT's internal representation of "no note"!
const note_t NoteFade = 253;

// 252, Param Control 'note'. Changes param value on first tick.
const note_t NotePc = 252;

// 251, Param Control(Smooth) 'note'. Changes param value during the whole row.
const note_t NotePcSmooth = 251;

const note_t NoteMin = 1;
const note_t NoteMax = 120;
const size_t NoteCount = 120;

const note_t NoteMaxSpecial = NoteKeyOff;
const note_t NoteMinSpecial = NotePcSmooth;

// Checks whether a number represents a valid note
// (a "normal" note or no note, but not something like note off)
inline bool note_is_valid(note_t note) {
    return note == NoteNone || (note >= NoteMin && note <= NoteMax);
}


struct modevent_t {
    note_t   note;
    instr_t  instr;
    volcmd_t volcmd;
    cmd_t    command;
    vol_t    vol;
    param_t  param;

    // Defines the maximum value for column data when interpreted as 2-byte
    // value (for example volcmd and vol). The valid value range is
    // [0, maxColumnValue].
    enum { MaxColumnValue = 999 };

    static modevent_t empty() {
        modevent_t ret = { note_t(0), 0, VolCmdNone, CmdNone, 0, 0 };
        return ret;
    }

    bool operator == (const modevent_t& mc) const {
        return memcmp(this, &mc, sizeof(modevent_t)) == 0;
    }

    bool operator != (const modevent_t& mc) const {
        return !(*this == mc);
    }

    void Set(note_t n, instr_t ins, uint16_t volcol, uint16_t effectcol) {
        note = n;
        instr = ins;
        SetValueVolCol(volcol);
        SetValueEffectCol(effectcol);
    }

    uint16_t GetValueVolCol() const {
        return GetValueVolCol(volcmd, vol);
    }

    static uint16_t GetValueVolCol(uint8_t volcmd, uint8_t vol) {
        return (volcmd << 8) + vol;
    }

    //XXXih: gross
    void SetValueVolCol(const uint16_t val) {
        volcmd = static_cast<volcmd_t>(val >> 8);
        vol = static_cast<uint8_t>(val & 0xFF);
    }

    uint16_t GetValueEffectCol() const {
        return GetValueEffectCol(command, param);
    }

    static uint16_t GetValueEffectCol(uint8_t command, uint8_t param) {
        return (command << 8) + param;
    }

    //XXXih: gross
    void SetValueEffectCol(const uint16_t val) {
        command = static_cast<cmd_t>(val >> 8);
        param = static_cast<uint8_t>(val & 0xFF);
    }

    // Clears modcommand.
    void Clear() {
        memset(this, 0, sizeof(modevent_t));
    }

    // Returns true if modcommand is empty, false otherwise.
    // If ignoreEffectValues is true (default), effect values are ignored
    // if there is no effect command present.
    bool IsEmpty(const bool ignoreEffectValues = true) const {
        if (ignoreEffectValues) {
            return this->note == 0
                && this->instr == 0
                && this->volcmd == VolCmdNone
                && this->command == CmdNone;
        } else {
            return *this == empty();
        }
    }

    // Returns true if instrument column represents plugin index.
    bool IsInstrPlug() const {
        return IsPcNote();
    }

    // Returns true if and only if note is NOTE_PC or NOTE_PCS.
    bool IsPcNote() const {
        return note == NotePc || note == NotePcSmooth;
    }

    static bool IsPcNote(const note_t note_id) {
        return note_id == NotePc || note_id == NotePcSmooth;
    }

    // Swap volume and effect column (doesn't do any conversion as it's mainly
    // for importing formats with multiple effect columns, so beware!)
    //XXXih: gross
    void SwapEffects() {
        int tmp = command;
        command  = (cmd_t) ((int) volcmd);
        volcmd   = (volcmd_t) tmp;

        std::swap(vol, param);
    }
};


}
}