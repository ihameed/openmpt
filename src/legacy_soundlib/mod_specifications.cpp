#include <stdafx.h>

#include "mod_specifications.h"

using namespace modplug::tracker;

bool CModSpecifications::HasNote(note_t note) const {
    if (note >= noteMin && note <= noteMax) {
        return true;
    } else if(note >= NoteMinSpecial && note <= NoteMaxSpecial) {
        switch (note) {
        case NoteNoteCut: return hasNoteCut;
        case NoteKeyOff:  return hasNoteOff;
        case NoteFade:    return hasNoteFade;
        default:
            return (memcmp(fileExtension, ModSpecs::mptm.fileExtension, 4) == 0);
        }
    } else if (note == NoteNone) {
        return true;
    }
    return false;
}

bool CModSpecifications::HasVolCommand(volcmd_t volcmd) const {
    if (volcmd >= VolCmdMax) {
        return false;
    }
    if (volcommands[volcmd] == '?') {
        return false;
    }
    return true;
}

bool CModSpecifications::HasCommand(cmd_t cmd) const {
    if (cmd >= CmdMax) {
        return false;
    }
    if (commands[cmd] == '?') {
        return false;
    }
    return true;
}

char CModSpecifications::GetVolEffectLetter(volcmd_t volcmd) const {
    if (volcmd >= VolCmdMax) {
        return '?';
    }
    return volcommands[volcmd];
}

char CModSpecifications::GetEffectLetter(cmd_t cmd) const {
    if(cmd >= CmdMax) {
        return '?';
    }
    return commands[cmd];
}