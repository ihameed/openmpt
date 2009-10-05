/*
 *
 * CleanupSong.cpp
 * ---------------
 * Purpose: Interface for cleaning up modules (rearranging, removing unused items)
 *
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 *
 */

#include "stdafx.h"
#include "moddoc.h"
#include "Mainfrm.h"
#include "modsmp_ctrl.h"
#include "CleanupSong.h"

// Default checkbox state
bool CModCleanupDlg::m_bCheckBoxes[CU_MAX_CLEANUP_OPTIONS] =
{
	true,	true,
	true,	true,
	true,	false,
	true,	false,
};

// Checkbox -> Control ID LUT
WORD const CModCleanupDlg::m_nCleanupIDtoDlgID[CU_MAX_CLEANUP_OPTIONS] =
{
	IDC_CHK_CLEANUP_PATTERNS,		IDC_CHK_REARRANGE_PATTERNS,
	IDC_CHK_CLEANUP_SAMPLES,		IDC_CHK_REARRANGE_SAMPLES,
	IDC_CHK_CLEANUP_INSTRUMENTS,	IDC_CHK_REMOVE_INSTRUMENTS,
	IDC_CHK_CLEANUP_PLUGINS,		IDC_CHK_SAMPLEPACK,
};

///////////////////////////////////////////////////////////////////////
// CModCleanupDlg

BEGIN_MESSAGE_MAP(CModCleanupDlg, CDialog)
	//{{AFX_MSG_MAP(CModTypeDlg)
	/*ON_COMMAND(IDC_CHK_CLEANUP_PATTERNS,		OnCheckCleanupPatterns)
	ON_COMMAND(IDC_CHK_REARRANGE_PATTERNS,		OnCheckRearrangePatterns)
	ON_COMMAND(IDC_CHK_CLEANUP_SAMPLES,			OnCheckCleanupSamples)
	ON_COMMAND(IDC_CHK_REARRANGE_SAMPLES,		OnCheckRearrangeSamples)
	ON_COMMAND(IDC_CHK_CLEANUP_INSTRUMENTS,		OnCheckCleanupInstruments)
	ON_COMMAND(IDC_CHK_REMOVE_INSTRUMENTS,		OnCheckRemoveInstruments)
	ON_COMMAND(IDC_CHK_CLEANUP_PLUGINS,			OnCheckCleanupPlugins)
	ON_COMMAND(IDC_CHK_SAMPLEPACK,				OnCheckCreateSamplepack)*/

	ON_COMMAND(IDC_BTN_CLEANUP_SONG,			OnPresetCleanupSong)
	ON_COMMAND(IDC_BTN_COMPO_CLEANUP,			OnPresetCompoCleanup)
	// -! NEW_FEATURE#0023
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CModCleanupDlg::OnInitDialog()
//---------------------------------
{
	CDialog::OnInitDialog();
	for(int i = 0; i < CU_MAX_CLEANUP_OPTIONS; i++)
	{
		CheckDlgButton(m_nCleanupIDtoDlgID[i], (m_bCheckBoxes[i]) ? MF_CHECKED : MF_UNCHECKED);
	}

	CSoundFile *pSndFile = m_pModDoc->GetSoundFile();
	if(pSndFile == nullptr) return FALSE;

	GetDlgItem(m_nCleanupIDtoDlgID[CU_CLEANUP_INSTRUMENTS])->EnableWindow((pSndFile->m_nInstruments > 0) ? TRUE :FALSE);
	GetDlgItem(m_nCleanupIDtoDlgID[CU_REMOVE_INSTRUMENTS])->EnableWindow((pSndFile->m_nInstruments > 0) ? TRUE :FALSE);
	GetDlgItem(m_nCleanupIDtoDlgID[CU_REARRANGE_SAMPLES])->EnableWindow((pSndFile->m_nSamples > 1) ? TRUE :FALSE);
	return TRUE;
}

void CModCleanupDlg::OnOK()
//-------------------------
{
	for(int i = 0; i < CU_MAX_CLEANUP_OPTIONS; i++)
	{
		m_bCheckBoxes[i] = IsDlgButtonChecked(m_nCleanupIDtoDlgID[i]) ? true : false;
	}

	bool bModified = false;
	m_pModDoc->ClearLog();

	// Patterns
	if(m_bCheckBoxes[CU_CLEANUP_PATTERNS]) bModified |= RemoveUnusedPatterns();
	if(m_bCheckBoxes[CU_REARRANGE_PATTERNS]) bModified |= RemoveUnusedPatterns(false);
	// Instruments
	if(m_pModDoc->GetSoundFile()->m_nInstruments > 0)
	{
		if(m_bCheckBoxes[CU_CLEANUP_INSTRUMENTS]) bModified |= RemoveUnusedInstruments();
		if(m_bCheckBoxes[CU_REMOVE_INSTRUMENTS]) bModified |= RemoveAllInstruments();
	}
	// Samples
	if(m_bCheckBoxes[CU_CLEANUP_SAMPLES]) bModified |= RemoveUnusedSamples();
	if(m_pModDoc->GetSoundFile()->m_nSamples > 1)
	{
		if(m_bCheckBoxes[CU_REARRANGE_SAMPLES]) bModified |= RearrangeSamples();
	}
	// Plugins
	if(m_bCheckBoxes[CU_CLEANUP_PLUGINS]) bModified |= RemoveUnusedPlugins();
	// Create samplepack
	if(m_bCheckBoxes[CU_CREATE_SAMPLEPACK]) bModified |= CreateSamplepack();

	if(bModified) m_pModDoc->SetModified();
	m_pModDoc->UpdateAllViews(NULL, HINT_MODTYPE|HINT_MODSEQUENCE|HINT_MODGENERAL);
	m_pModDoc->ShowLog("Cleanup", this);
	CDialog::OnOK();
}


void CModCleanupDlg::OnCancel()
//-------------------------
{
	CDialog::OnCancel();
}



void CModCleanupDlg::OnPresetCleanupSong()
//----------------------------------------
{
	CheckDlgButton(IDC_CHK_CLEANUP_PATTERNS, MF_CHECKED);
	CheckDlgButton(IDC_CHK_REARRANGE_PATTERNS, MF_CHECKED);
	CheckDlgButton(IDC_CHK_CLEANUP_SAMPLES, MF_CHECKED);
	CheckDlgButton(IDC_CHK_REARRANGE_SAMPLES, MF_CHECKED);
	CheckDlgButton(IDC_CHK_CLEANUP_INSTRUMENTS, MF_CHECKED);
	CheckDlgButton(IDC_CHK_REMOVE_INSTRUMENTS, MF_UNCHECKED);
	CheckDlgButton(IDC_CHK_CLEANUP_PLUGINS, MF_CHECKED);
	CheckDlgButton(IDC_CHK_SAMPLEPACK, MF_UNCHECKED);
}

void CModCleanupDlg::OnPresetCompoCleanup()
//----------------------------------------
{
	CheckDlgButton(IDC_CHK_CLEANUP_PATTERNS, MF_UNCHECKED);
	CheckDlgButton(IDC_CHK_REARRANGE_PATTERNS, MF_UNCHECKED);
	CheckDlgButton(IDC_CHK_CLEANUP_SAMPLES, MF_UNCHECKED);
	CheckDlgButton(IDC_CHK_REARRANGE_SAMPLES, MF_CHECKED);
	CheckDlgButton(IDC_CHK_CLEANUP_INSTRUMENTS, MF_UNCHECKED);
	CheckDlgButton(IDC_CHK_REMOVE_INSTRUMENTS, MF_UNCHECKED);
	CheckDlgButton(IDC_CHK_CLEANUP_PLUGINS, MF_UNCHECKED);
	CheckDlgButton(IDC_CHK_SAMPLEPACK, MF_CHECKED);
}


///////////////////////////////////////////////////////////////////////
// Actual cleanup implementations

// Remove unused patterns / rearrange patterns
bool CModCleanupDlg::RemoveUnusedPatterns(bool bRemove)
//-----------------------------------------------------
{
	CSoundFile *pSndFile = m_pModDoc->GetSoundFile();
	if(pSndFile == nullptr) return false;

	if(pSndFile->GetType() == MOD_TYPE_MPT && pSndFile->Order.GetNumSequences() > 1)
	{   // Multiple sequences are not taken into account in the code below. For now just make
		// removing unused patterns disabled in this case.
		AfxMessageBox(IDS_PATTERN_CLEANUP_UNAVAILABLE, MB_ICONINFORMATION);
		return false;
	}
	const PATTERNINDEX maxPatIndex = pSndFile->Patterns.Size();
	const ORDERINDEX maxOrdIndex = pSndFile->Order.size();
	vector<PATTERNINDEX> nPatMap(maxPatIndex, 0);
	vector<ROWINDEX> nPatRows(maxPatIndex, 0);
	vector<MODCOMMAND*> pPatterns(maxPatIndex, nullptr);
	vector<bool> bPatUsed(maxPatIndex, false);

	bool bSubtunesDetected = false;
	// detect subtunes (separated by "---")
	for(SEQUENCEINDEX nSeq = 0; nSeq < pSndFile->Order.GetNumSequences(); nSeq++)
	{
		if(pSndFile->Order.GetSequence(nSeq).GetLengthFirstEmpty() != pSndFile->Order.GetSequence(nSeq).GetLengthTailTrimmed())
			bSubtunesDetected = true;
	}

	// Flag to tell whether keeping sequence items which are after the first empty('---') order.
	bool bKeepSubSequences = false;

	if(bSubtunesDetected)
	{   // There are used sequence items after first '---'; ask user whether to remove those.
		if (m_wParent->MessageBox(
			_TEXT("Do you want to remove sequence items which are after the first '---' item?"),
			_TEXT("Sequence Cleanup"), MB_YESNO) != IDYES
			)
			bKeepSubSequences = true;
	}

	CHAR s[512];
	bool bEnd = false, bReordered = false;
	UINT nPatRemoved = 0, nMinToRemove;
	PATTERNINDEX nPats;

	BeginWaitCursor();
	PATTERNINDEX maxpat = 0;
	for (ORDERINDEX nOrd = 0; nOrd < maxOrdIndex; nOrd++)
	{
		PATTERNINDEX n = pSndFile->Order[nOrd];
		if (n < maxPatIndex)
		{
			if (n >= maxpat) maxpat = n + 1;
			if (!bEnd || bKeepSubSequences) bPatUsed[n] = true;
		} else if (n == pSndFile->Order.GetInvalidPatIndex()) bEnd = true;
	}
	nMinToRemove = 0;
	if (!bRemove)
	{
		PATTERNINDEX imax = maxPatIndex;
		while (imax > 0)
		{
			imax--;
			if ((pSndFile->Patterns[imax]) && (bPatUsed[imax])) break;
		}
		nMinToRemove = imax + 1;
	}
	for (PATTERNINDEX nPat = maxpat; nPat < maxPatIndex; nPat++) if ((pSndFile->Patterns[nPat]) && (nPat >= nMinToRemove))
	{
		MODCOMMAND *m = pSndFile->Patterns[nPat];
		UINT ncmd = pSndFile->m_nChannels * pSndFile->PatternSize[nPat];
		for (UINT i=0; i<ncmd; i++)
		{
			if ((m[i].note) || (m[i].instr) || (m[i].volcmd) || (m[i].command)) goto NotEmpty;
		}
		pSndFile->Patterns.Remove(nPat);
		nPatRemoved++;
NotEmpty:
		;
	}
	UINT bWaste = 0;
	for (UINT ichk=0; ichk < maxPatIndex; ichk++)
	{
		if ((pSndFile->Patterns[ichk]) && (!bPatUsed[ichk])) bWaste++;
	}
	if ((bRemove) && (bWaste))
	{
		EndWaitCursor();
		wsprintf(s, "%d pattern(s) present in file, but not used in the song\nDo you want to reorder the sequence list and remove these patterns?", bWaste);
		if (m_wParent->MessageBox(s, "Pattern Cleanup", MB_YESNO) != IDYES) return false;
		BeginWaitCursor();
	}

	for (UINT i = 0; i < maxPatIndex; i++) nPatMap[i] = PATTERNINDEX_INVALID;
	nPats = 0;
	ORDERINDEX imap = 0;
	for (imap = 0; imap < maxOrdIndex; imap++)
	{
		PATTERNINDEX n = pSndFile->Order[imap];
		if (n < maxPatIndex)
		{
			if (nPatMap[n] > maxPatIndex) nPatMap[n] = nPats++;
			pSndFile->Order[imap] = nPatMap[n];
		} else if (n == pSndFile->Order.GetInvalidPatIndex() && (bKeepSubSequences == false)) break;
	}
	// Add unused patterns at the end
	if ((!bRemove) || (!bWaste))
	{
		for (UINT iadd=0; iadd<maxPatIndex; iadd++)
		{
			if ((pSndFile->Patterns[iadd]) && (nPatMap[iadd] >= maxPatIndex))
			{
				nPatMap[iadd] = nPats++;
			}
		}
	}
	while (imap < maxOrdIndex)
	{
		pSndFile->Order[imap++] = pSndFile->Order.GetInvalidPatIndex();
	}
	// Reorder patterns & Delete unused patterns
	BEGIN_CRITICAL();
	{
		UINT npatnames = pSndFile->m_nPatternNames;
		LPSTR lpszpatnames = pSndFile->m_lpszPatternNames;
		pSndFile->m_nPatternNames = 0;
		pSndFile->m_lpszPatternNames = NULL;
		for (PATTERNINDEX i = 0; i < maxPatIndex; i++)
		{
			PATTERNINDEX k = nPatMap[i];
			if (k < maxPatIndex)
			{
				if (i != k) bReordered = true;
				// Remap pattern names
				if (i < npatnames)
				{
					UINT noldpatnames = pSndFile->m_nPatternNames;
					LPSTR lpszoldpatnames = pSndFile->m_lpszPatternNames;
					pSndFile->m_nPatternNames = npatnames;
					pSndFile->m_lpszPatternNames = lpszpatnames;
					pSndFile->GetPatternName(i, s);
					pSndFile->m_nPatternNames = noldpatnames;
					pSndFile->m_lpszPatternNames = lpszoldpatnames;
					if (s[0]) pSndFile->SetPatternName(k, s);
				}
				nPatRows[k] = pSndFile->PatternSize[i];
				pPatterns[k] = pSndFile->Patterns[i];
			} else
				if (pSndFile->Patterns[i])
				{
					pSndFile->Patterns.Remove(i);
					nPatRemoved++;
				}
		}
		for (PATTERNINDEX j = 0; j < maxPatIndex;j++)
		{
			pSndFile->Patterns[j].SetData(pPatterns[j], nPatRows[j]);
		}
	}
	END_CRITICAL();
	EndWaitCursor();
	if ((nPatRemoved) || (bReordered))
	{
		m_pModDoc->ClearUndo();
		if (nPatRemoved)
		{
			wsprintf(s, "%d pattern(s) removed.\n", nPatRemoved);
			m_pModDoc->AddToLog(s);
		}
		return true;
	}
	return false;
}


// Remove unused samples
bool CModCleanupDlg::RemoveUnusedSamples()
//---------------------------------
{
	CSoundFile *pSndFile = m_pModDoc->GetSoundFile();
	if(pSndFile == nullptr) return false;

	CHAR s[512];
	BOOL bIns[MAX_SAMPLES];
	UINT nExt = 0, nLoopOpt = 0;
	UINT nRemoved = 0;

	BeginWaitCursor();
	for (SAMPLEINDEX nSmp = pSndFile->m_nSamples; nSmp >= 1; nSmp--) if (pSndFile->Samples[nSmp].pSample)
	{
		if (!pSndFile->IsSampleUsed(nSmp))
		{
			BEGIN_CRITICAL();
			pSndFile->DestroySample(nSmp);
			if ((nSmp == pSndFile->m_nSamples) && (nSmp > 1)) pSndFile->m_nSamples--;
			END_CRITICAL();
			nRemoved++;
		}
	}
	if (pSndFile->m_nInstruments)
	{
		memset(bIns, 0, sizeof(bIns));
		for (UINT ipat=0; ipat<pSndFile->Patterns.Size(); ipat++)
		{
			MODCOMMAND *p = pSndFile->Patterns[ipat];
			if (p)
			{
				UINT jmax = pSndFile->PatternSize[ipat] * pSndFile->m_nChannels;
				for (UINT j=0; j<jmax; j++, p++)
				{
					if ((p->note) && (p->note <= NOTE_MAX))
					{
						if ((p->instr) && (p->instr < MAX_INSTRUMENTS))
						{
							MODINSTRUMENT *pIns = pSndFile->Instruments[p->instr];
							if (pIns)
							{
								UINT n = pIns->Keyboard[p->note-1];
								if (n < MAX_SAMPLES) bIns[n] = TRUE;
							}
						} else
						{
							for (UINT k=1; k<=pSndFile->m_nInstruments; k++)
							{
								MODINSTRUMENT *pIns = pSndFile->Instruments[k];
								if (pIns)
								{
									UINT n = pIns->Keyboard[p->note-1];
									if (n < MAX_SAMPLES) bIns[n] = TRUE;
								}
							}
						}
					}
				}
			}
		}
		for (UINT ichk=1; ichk<MAX_SAMPLES; ichk++)
		{
			if ((!bIns[ichk]) && (pSndFile->Samples[ichk].pSample)) nExt++;
		}
	}
	EndWaitCursor();
	if (nExt &&  !((pSndFile->m_nType & MOD_TYPE_IT) && (pSndFile->m_dwSongFlags&SONG_ITPROJECT)))
	{	//We don't remove an instrument's unused samples in an ITP.
		wsprintf(s, "OpenMPT detected %d sample(s) referenced by an instrument,\n"
			"but not used in the song. Do you want to remove them ?", nExt);
		if (::MessageBox(NULL, s, "Sample Cleanup", MB_YESNO | MB_ICONQUESTION) == IDYES)
		{
			for (SAMPLEINDEX j = 1; j < MAX_SAMPLES; j++)
			{
				if ((!bIns[j]) && (pSndFile->Samples[j].pSample))
				{
					BEGIN_CRITICAL();
					pSndFile->DestroySample(j);
					if ((j == pSndFile->m_nSamples) && (j > 1)) pSndFile->m_nSamples--;
					END_CRITICAL();
					nRemoved++;
				}
			}
		}
	}
	for (SAMPLEINDEX ilo=1; ilo<=pSndFile->m_nSamples; ilo++) if (pSndFile->Samples[ilo].pSample)
	{
		if ((pSndFile->Samples[ilo].uFlags & CHN_LOOP)
			&& (pSndFile->Samples[ilo].nLength > pSndFile->Samples[ilo].nLoopEnd + 2)) nLoopOpt++;
	}
	if (nLoopOpt)
	{
		wsprintf(s, "OpenMPT detected %d sample(s) with unused data after the loop end point,\n"
			"Do you want to optimize it, and remove this unused data ?", nLoopOpt);
		if (::MessageBox(NULL, s, "Sample Cleanup", MB_YESNO | MB_ICONQUESTION) == IDYES)
		{
			for (UINT j=1; j<=pSndFile->m_nSamples; j++)
			{
				if ((pSndFile->Samples[j].uFlags & CHN_LOOP)
					&& (pSndFile->Samples[j].nLength > pSndFile->Samples[j].nLoopEnd + 2))
				{
					UINT lmax = pSndFile->Samples[j].nLoopEnd + 2;
					if ((lmax < pSndFile->Samples[j].nLength) && (lmax >= 16)) pSndFile->Samples[j].nLength = lmax;
				}
			}
		} else nLoopOpt = 0;
	}
	if ((nRemoved) || (nLoopOpt))
	{
		if (nRemoved)
		{
			wsprintf(s, "%d unused sample(s) removed\n" ,nRemoved);
			m_pModDoc->AddToLog(s);
		}
		if (nLoopOpt)
		{
			wsprintf(s, "%d sample loop(s) optimized\n" ,nLoopOpt);
			m_pModDoc->AddToLog(s);
		}
		return true;
	}
	return false;
}


// Rearrange sample list
bool CModCleanupDlg::RearrangeSamples()
//-------------------------------------
{
	CSoundFile *pSndFile = m_pModDoc->GetSoundFile();
	if(pSndFile == nullptr) return false;

	if(pSndFile->m_nSamples < 2)
		return false;

	SAMPLEINDEX nRemap = 0; // remap count
	SAMPLEINDEX nSampleMap[MAX_SAMPLES + 1]; // map old => new
	for(SAMPLEINDEX i = 0; i <= MAX_SAMPLES; i++)
		nSampleMap[i] = i;

	// First, find out which sample slots are unused and create the new sample map
	for(SAMPLEINDEX i = 1 ; i <= pSndFile->m_nSamples; i++) {
		if(!pSndFile->Samples[i].pSample)
		{
			// Move all following samples
			nRemap++;
			nSampleMap[i] = 0;
			for(UINT j = i + 1; j <= pSndFile->m_nSamples; j++)
				nSampleMap[j]--;
		}
	}

	if(!nRemap)
		return false;

	BEGIN_CRITICAL();

	// Now, move everything around
	for(SAMPLEINDEX i = 1; i <= pSndFile->m_nSamples; i++)
	{
		if(nSampleMap[i] != i)
		{
			// This gotta be moved
			pSndFile->MoveSample(i, nSampleMap[i]);
			pSndFile->Samples[i].pSample = nullptr;
			if(nSampleMap[i] > 0) strcpy(pSndFile->m_szNames[nSampleMap[i]], pSndFile->m_szNames[i]);
			memset(pSndFile->m_szNames[i], 0, sizeof(pSndFile->m_szNames[i]));

			// Also update instrument mapping (if module is in instrument mode)
			for(INSTRUMENTINDEX iInstr = 1; iInstr <= pSndFile->m_nInstruments; iInstr++){
				if(pSndFile->Instruments[iInstr]){
					MODINSTRUMENT *p = pSndFile->Instruments[iInstr];
					for(WORD iNote = 0; iNote < 128; iNote++)
						if(p->Keyboard[iNote] == i) p->Keyboard[iNote] = nSampleMap[i];
				}
			}
		}
	}

	// Go through the patterns and remap samples (if module is in sample mode)
	if(!pSndFile->m_nInstruments)
	{
		for (PATTERNINDEX nPat = 0; nPat < pSndFile->Patterns.Size(); nPat++) if (pSndFile->Patterns[nPat])
		{
			MODCOMMAND *m = pSndFile->Patterns[nPat];
			for(UINT len = pSndFile->PatternSize[nPat] * pSndFile->m_nChannels; len; m++, len--)
			{
				if(m->instr <= pSndFile->m_nSamples) m->instr = (BYTE)nSampleMap[m->instr];
			}
		}
	}

	pSndFile->m_nSamples -= nRemap;

	END_CRITICAL();

	return true;

}


// Remove unused instruments
bool CModCleanupDlg::RemoveUnusedInstruments()
//--------------------------------------------
{
	CSoundFile *pSndFile = m_pModDoc->GetSoundFile();
	if(pSndFile == nullptr) return false;

	INSTRUMENTINDEX usedmap[MAX_INSTRUMENTS];
	INSTRUMENTINDEX swapmap[MAX_INSTRUMENTS];
	INSTRUMENTINDEX swapdest[MAX_INSTRUMENTS];
	CHAR s[512];
	UINT nRemoved = 0;
	INSTRUMENTINDEX nSwap, nIndex;
	bool bReorg = false;

	if (!pSndFile->m_nInstruments) return false;

	char removeSamples = -1;
	if ( !((pSndFile->m_nType & MOD_TYPE_IT) && (pSndFile->m_dwSongFlags&SONG_ITPROJECT))) { //never remove an instrument's samples in ITP.
		if(::MessageBox(NULL, "Remove samples associated with an instrument if they are unused?", "Removing unused instruments", MB_YESNO | MB_ICONQUESTION) == IDYES) {
			removeSamples = 1;
		}
	} else {
		::MessageBox(NULL, "This is an IT project file, so no samples associated with a used instrument will be removed.", "Removing unused instruments", MB_OK | MB_ICONINFORMATION);
	}

	BeginWaitCursor();
	memset(usedmap, 0, sizeof(usedmap));

	for(INSTRUMENTINDEX i = pSndFile->m_nInstruments; i >= 1; i--)
	{
		if (!pSndFile->IsInstrumentUsed(i))
		{
			BEGIN_CRITICAL();
			// -> CODE#0003
			// -> DESC="remove instrument's samples"
			//			pSndFile->DestroyInstrument(i);
			pSndFile->DestroyInstrument(i, removeSamples);
			// -! BEHAVIOUR_CHANGE#0003
			if ((i == pSndFile->m_nInstruments) && (i>1)) pSndFile->m_nInstruments--; else bReorg = true;
			END_CRITICAL();
			nRemoved++;
		} else
		{
			usedmap[i] = 1;
		}
	}
	EndWaitCursor();
	if ((bReorg) && (pSndFile->m_nInstruments > 1)
		&& (::MessageBox(NULL, "Do you want to reorganize the remaining instruments?", "Removing unused instruments", MB_YESNO | MB_ICONQUESTION) == IDYES))
	{
		BeginWaitCursor();
		BEGIN_CRITICAL();
		nSwap = 0;
		nIndex = 1;
		for (INSTRUMENTINDEX nIns = 1; nIns <= pSndFile->m_nInstruments; nIns++)
		{
			if (usedmap[nIns])
			{
				while (nIndex<nIns)
				{
					if ((!usedmap[nIndex]) && (!pSndFile->Instruments[nIndex]))
					{
						swapmap[nSwap] = nIns;
						swapdest[nSwap] = nIndex;
						pSndFile->Instruments[nIndex] = pSndFile->Instruments[nIns];
						pSndFile->Instruments[nIns] = nullptr;
						usedmap[nIndex] = 1;
						usedmap[nIns] = 0;
						nSwap++;
						nIndex++;
						break;
					}
					nIndex++;
				}
			}
		}
		while ((pSndFile->m_nInstruments > 1) && (!pSndFile->Instruments[pSndFile->m_nInstruments])) pSndFile->m_nInstruments--;
		END_CRITICAL();
		if (nSwap > 0)
		{
			for (PATTERNINDEX iPat = 0; iPat < pSndFile->Patterns.Size(); iPat++) if (pSndFile->Patterns[iPat])
			{
				MODCOMMAND *p = pSndFile->Patterns[iPat];
				UINT nLen = pSndFile->m_nChannels * pSndFile->PatternSize[iPat];
				while (nLen--)
				{
					if (p->instr)
					{
						for (UINT k=0; k<nSwap; k++)
						{
							if (p->instr == swapmap[k]) p->instr = (BYTE)swapdest[k];
						}
					}
					p++;
				}
			}
		}
		EndWaitCursor();
	}
	if (nRemoved)
	{
		wsprintf(s, "%d unused instrument(s) removed\n", nRemoved);
		m_pModDoc->AddToLog(s);
		return true;
	}
	return false;
}


// Remove all instruments
bool CModCleanupDlg::RemoveAllInstruments(bool bConfirm)
//-----------------------------------------------
{
	CSoundFile *pSndFile = m_pModDoc->GetSoundFile();
	if(pSndFile == nullptr) return false;

	if (!pSndFile->m_nInstruments)
		return false;

	char removeSamples = -1;
	if(bConfirm)
	{
		if (CMainFrame::GetMainFrame()->MessageBox("This will remove all the instruments in the song,\n"
			"Do you want to continue?", "Removing all instruments", MB_YESNO | MB_ICONQUESTION) != IDYES) return false;
		if (CMainFrame::GetMainFrame()->MessageBox("Do you want to convert all instruments to samples ?\n",
			"Removing all instruments", MB_YESNO | MB_ICONQUESTION) == IDYES)
		{
			m_pModDoc->ConvertInstrumentsToSamples();
		}

		if (::MessageBox(NULL, "Remove samples associated with an instrument if they are unused?", "Removing all instruments", MB_YESNO | MB_ICONQUESTION) == IDYES) {
			removeSamples = 1;
		}
	}

	for (INSTRUMENTINDEX i = 1; i <= pSndFile->m_nInstruments; i++)
	{
		pSndFile->DestroyInstrument(i, removeSamples);
	}

	pSndFile->m_nInstruments = 0;
	return true;
}


// Remove ununsed plugins
bool CModCleanupDlg::RemoveUnusedPlugins()
//--------------------------------------
{
	CSoundFile *pSndFile = m_pModDoc->GetSoundFile();
	if(pSndFile == nullptr) return false;

	bool usedmap[MAX_MIXPLUGINS];
	memset(usedmap, false, sizeof(usedmap));
	
	for (PLUGINDEX nPlug = 0; nPlug < MAX_MIXPLUGINS; nPlug++) {

		//Is the plugin assigned to a channel?
		for (CHANNELINDEX nChn = 0; nChn < pSndFile->GetNumChannels(); nChn++) {
			if (pSndFile->ChnSettings[nChn].nMixPlugin == nPlug + 1u) {
				usedmap[nPlug] = true;
				break;
			}
		}

		//Is the plugin used by an instrument?
		for (INSTRUMENTINDEX nIns=1; nIns<=pSndFile->GetNumInstruments(); nIns++) {
			if (pSndFile->Instruments[nIns] && (pSndFile->Instruments[nIns]->nMixPlug == nPlug+1)) {
				usedmap[nPlug] = true;
				break;
			}
		}

		//Is the plugin assigned to master?
		if (pSndFile->m_MixPlugins[nPlug].Info.dwInputRouting & MIXPLUG_INPUTF_MASTEREFFECT) {
			usedmap[nPlug] = true;
		}

		//all outputs of used plugins count as used
		if (usedmap[nPlug]!=0) {
			if (pSndFile->m_MixPlugins[nPlug].Info.dwOutputRouting & 0x80) {
				int output = pSndFile->m_MixPlugins[nPlug].Info.dwOutputRouting & 0x7f;
				usedmap[output] = true;
			}
		}

	}

	UINT nRemoved = m_pModDoc->RemovePlugs(usedmap);

	return (nRemoved > 0) ? true : false;
}


// Turn module into samplepack (convert to IT, remove patterns, etc.)
bool CModCleanupDlg::CreateSamplepack()
//-------------------------------------
{
	CSoundFile *pSndFile = m_pModDoc->GetSoundFile();
	if(pSndFile == nullptr) return false;

	//jojo.compocleanup
	if(::MessageBox(NULL, TEXT("WARNING: Turning this module into as samplepack means that OpenMPT will convert the module to IT format, remove all patterns and reset song, sample and instrument attributes to default values. Continue?"), TEXT("Samplepack creation"), MB_YESNO | MB_ICONWARNING) == IDNO)
		return false;

	// Stop play.
	CMainFrame::GetMainFrame()->StopMod(m_pModDoc);

	BeginWaitCursor();
	BEGIN_CRITICAL();

	// convert to IT...
	m_pModDoc->ChangeModType(MOD_TYPE_IT);
	pSndFile->m_nMixLevels = mixLevels_original;
	pSndFile->m_nTempoMode = tempo_mode_classic;
	pSndFile->m_dwSongFlags = SONG_LINEARSLIDES | SONG_EXFILTERRANGE;
	
	// clear order list
	pSndFile->Order.Init();
	pSndFile->Order[0] = 0;

	// remove all patterns
	pSndFile->Patterns.Init();
	pSndFile->Patterns.Insert(0, 64);
	pSndFile->SetCurrentOrder(0);

	// Global vars
	pSndFile->m_nDefaultTempo = 125;
	pSndFile->m_nDefaultSpeed = 6;
	pSndFile->m_nDefaultGlobalVolume = 256;
	pSndFile->m_nSamplePreAmp = 48;
	pSndFile->m_nVSTiVolume = 48;
	pSndFile->m_nRestartPos = 0;

	// Set 4 default channels.
	pSndFile->ReArrangeChannels(vector<CHANNELINDEX>(4, MAX_BASECHANNELS));

	// remove plugs
	bool keepMask[MAX_MIXPLUGINS]; memset(keepMask, 0, sizeof(keepMask));
	m_pModDoc->RemovePlugs(keepMask);

	// instruments
	if(pSndFile->m_nInstruments && ::MessageBox(NULL, "Keep instruments?", TEXT("Samplepack creation"), MB_YESNO | MB_ICONQUESTION) == IDNO)
	{
		// remove instruments
		RemoveAllInstruments(false);
	}
	else
	{
		// reset instruments (if there are any)
		for(INSTRUMENTINDEX i = 1; i <= pSndFile->m_nInstruments; i++)
		{
			pSndFile->Instruments[i]->nFadeOut = 256;
			pSndFile->Instruments[i]->nGlobalVol = 64;
			pSndFile->Instruments[i]->nPan = 128;
			pSndFile->Instruments[i]->dwFlags &= ~ENV_SETPANNING;
			pSndFile->Instruments[i]->nMixPlug = 0;

			pSndFile->Instruments[i]->nVolSwing = 0;
			pSndFile->Instruments[i]->nPanSwing = 0;
			pSndFile->Instruments[i]->nCutSwing = 0;
			pSndFile->Instruments[i]->nResSwing = 0;

			//might be a good idea to leave those enabled...
			/*
			pSndFile->Instruments[i]->dwFlags &= ~ENV_VOLUME;
			pSndFile->Instruments[i]->dwFlags &= ~ENV_PANNING;
			pSndFile->Instruments[i]->dwFlags &= ~ENV_PITCH;
			pSndFile->Instruments[i]->dwFlags &= ~ENV_FILTER;
			*/
		}
	}

	// reset samples
	ctrlSmp::ResetSamples(*pSndFile, ctrlSmp::SmpResetCompo);

	// Set modflags.
	pSndFile->SetModFlag(MSF_MIDICC_BUGEMULATION, false);
	pSndFile->SetModFlag(MSF_OLDVOLSWING, false);
	pSndFile->SetModFlag(MSF_COMPATIBLE_PLAY, true);

	END_CRITICAL();
	EndWaitCursor();

	return true;
}