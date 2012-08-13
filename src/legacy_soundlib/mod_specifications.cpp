#include <stdafx.h>

#include "mod_specifications.h"

using namespace modplug::tracker;

bool CModSpecifications::HasNote(note_t note) const {
    if (note >= noteMin && note <= noteMax) {
        return true;
    } else if(note >= NOTE_MIN_SPECIAL && note <= NOTE_MAX_SPECIAL) {
        switch (note) {
        case NOTE_NOTECUT: return hasNoteCut;
        case NOTE_KEYOFF:  return hasNoteOff;
        case NOTE_FADE:    return hasNoteFade;
        default:
            return (memcmp(fileExtension, ModSpecs::mptm.fileExtension, 4) == 0);
        }
    } else if (note == NOTE_NONE) {
        return true;
    }
    return false;
}

bool CModSpecifications::HasVolCommand(volcmd_t volcmd) const {
    if (volcmd >= MAX_VOLCMDS) {
        return false;
    }
    if (volcommands[volcmd] == '?') {
        return false;
    }
    return true;
}

bool CModSpecifications::HasCommand(cmd_t cmd) const {
    if (cmd >= MAX_EFFECTS) {
        return false;
    }
    if (commands[cmd] == '?') {
        return false;
    }
    return true;
}

char CModSpecifications::GetVolEffectLetter(volcmd_t volcmd) const {
    if (volcmd >= MAX_VOLCMDS) {
        return '?';
    }
    return volcommands[volcmd];
}

char CModSpecifications::GetEffectLetter(cmd_t cmd) const {
    if(cmd >= MAX_EFFECTS) {
        return '?';
    }
    return commands[cmd];
}