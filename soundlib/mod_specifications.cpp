#include <stdafx.h>
#include "mod_specifications.h"


bool CModSpecifications::HasNote(modplug::tracker::modcommand_t::NOTE note) const
//------------------------------------------------------------
{
    if(note >= noteMin && note <= noteMax)
    	return true;
    else if(note >= NOTE_MIN_SPECIAL && note <= NOTE_MAX_SPECIAL)
    {
    	if(note == NOTE_NOTECUT)
    		return hasNoteCut;
    	else if(note == NOTE_KEYOFF)
    		return hasNoteOff;
    	else if(note == NOTE_FADE)
    		return hasNoteFade;
    	else
    		return (memcmp(fileExtension, ModSpecs::mptm.fileExtension, 4) == 0);
    } else if(note == NOTE_NONE)
    	return true;
    return false;
}

bool CModSpecifications::HasVolCommand(modplug::tracker::modcommand_t::VOLCMD volcmd) const
//---------------------------------------------------------------------
{
    if(volcmd >= MAX_VOLCMDS) return false;
    if(volcommands[volcmd] == '?') return false;
    return true;
}

bool CModSpecifications::HasCommand(modplug::tracker::modcommand_t::COMMAND cmd) const
//----------------------------------------------------------------
{
    if(cmd >= MAX_EFFECTS) return false;
    if(commands[cmd] == '?') return false;
    return true;
}

char CModSpecifications::GetVolEffectLetter(modplug::tracker::modcommand_t::VOLCMD volcmd) const
//--------------------------------------------------------------------------
{
    if(volcmd >= MAX_VOLCMDS) return '?';
    return volcommands[volcmd];
}

char CModSpecifications::GetEffectLetter(modplug::tracker::modcommand_t::COMMAND cmd) const
//---------------------------------------------------------------------
{
    if(cmd >= MAX_EFFECTS) return '?';
    return commands[cmd];
}
