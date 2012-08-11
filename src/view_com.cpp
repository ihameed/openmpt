#include "stdafx.h"
#include "mptrack.h"
#include "mainfrm.h"
#include "childfrm.h"
#include "moddoc.h"
#include "globals.h"
#include "ctrl_com.h"
#include "view_com.h"
#include "ChannelManagerDlg.h"

#define DETAILS_TOOLBAR_CY    28

typedef struct LISTCOLHDR
{
    LPCSTR pszName;
    UINT cx;
} LISTCOLHDR, *PLISTCOLHDR;


enum {
    SMPLIST_SAMPLENAME=0,
    SMPLIST_SAMPLENO,
    SMPLIST_SIZE,
    SMPLIST_TYPE,
    SMPLIST_MIDDLEC,
    SMPLIST_INSTR,
    SMPLIST_FILENAME,
    SMPLIST_COLUMNS
};


enum {
    INSLIST_INSTRUMENTNAME=0,
    INSLIST_INSTRUMENTNO,
    INSLIST_SAMPLES,
    INSLIST_ENVELOPES,
    INSLIST_FILENAME,
    INSLIST_PLUGIN,
// -> CODE#0023
// -> DESC="IT project files (.itp)"
    INSLIST_PATH,
// -! NEW_FEATURE#0023
    INSLIST_COLUMNS
};


enum {
    PATLIST_PATTERNNAME=0,
    PATLIST_PATTERNNO,
    PATLIST_COLUMNS
};


LISTCOLHDR gSampleHeaders[SMPLIST_COLUMNS] =
{
    {"Sample Name", 192},
    {"Num", 36},
    {"Size", 72},
    {"Type", 40},
    {"C-5 Freq", 80},
    {"Instr", 64},
    {"File Name", 128},
};

LISTCOLHDR gInstrumentHeaders[INSLIST_COLUMNS] =
{
    {"Instrument Name", 192},
    {"Num", 36},
    {"Samples", 64},
    {"Envelopes", 128},
    {"File Name", 128},
    {"Plugin", 128},
// -> CODE#0023
// -> DESC="IT project files (.itp)"
    {"Path", 128},
// -! NEW_FEATURE#0023
};


IMPLEMENT_SERIAL(CViewComments, CModScrollView, 0)

BEGIN_MESSAGE_MAP(CViewComments, CModScrollView)
    //{{AFX_MSG_MAP(CViewComments)
    ON_WM_SIZE()
    ON_WM_DESTROY()
    ON_COMMAND(IDC_LIST_SAMPLES,		OnShowSamples)
    ON_COMMAND(IDC_LIST_INSTRUMENTS,	OnShowInstruments)
    ON_COMMAND(IDC_LIST_PATTERNS,		OnShowPatterns)
    ON_NOTIFY(LVN_ENDLABELEDIT,	IDC_LIST_DETAILS,	OnEndLabelEdit)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST_DETAILS,	OnDblClickListItem)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()


///////////////////////////////////////////////////////////////
// CViewComments operations

CViewComments::CViewComments()
//----------------------------
{
    m_nCurrentListId = 0;
    m_nListId = 0;//IDC_LIST_SAMPLES;
}


void CViewComments::OnInitialUpdate()
//-----------------------------------
{
    if(m_nListId == 0)
    {
    	m_nListId = IDC_LIST_SAMPLES;

    	// For XM, set the instrument list as the default list
    	CModDoc *pModDoc = GetDocument();
    	module_renderer *pSndFile;
    	if(pModDoc)
    	{
    		pSndFile= pModDoc->GetSoundFile();
    		if(pSndFile && (pSndFile->m_nType & MOD_TYPE_XM) && pSndFile->m_nInstruments > 0)
    		{
    			m_nListId = IDC_LIST_INSTRUMENTS;
    		}
    	}
    }

    CChildFrame *pFrame = (CChildFrame *)GetParentFrame();
    CRect rect;

    if (pFrame)
    {
    	COMMENTVIEWSTATE *pState = pFrame->GetCommentViewState();
    	if (pState->cbStruct == sizeof(COMMENTVIEWSTATE))
    	{
    		m_nListId = pState->nId;
    	}
    }
    GetClientRect(&rect);
    m_ToolBar.Create(WS_CHILD|WS_VISIBLE|CCS_NOPARENTALIGN, rect, this, IDC_TOOLBAR_DETAILS);
    m_ToolBar.Init();
    m_ItemList.Create(WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL|LVS_EDITLABELS|LVS_NOSORTHEADER, rect, this, IDC_LIST_DETAILS);
    m_ItemList.ModifyStyleEx(0, WS_EX_STATICEDGE);
    // Add ToolBar Buttons
    m_ToolBar.AddButton(IDC_LIST_SAMPLES, TIMAGE_TAB_SAMPLES);
    m_ToolBar.AddButton(IDC_LIST_INSTRUMENTS, TIMAGE_TAB_INSTRUMENTS);
    //m_ToolBar.AddButton(IDC_LIST_PATTERNS, TIMAGE_TAB_PATTERNS);
    m_ToolBar.SetIndent(4);
    UpdateButtonState();
    OnUpdate(NULL, HINT_MODTYPE, NULL);
}


void CViewComments::OnDestroy()
//-----------------------------
{
    CChildFrame *pFrame = (CChildFrame *)GetParentFrame();
    if (pFrame)
    {
    	COMMENTVIEWSTATE *pState = pFrame->GetCommentViewState();
    	pState->cbStruct = sizeof(COMMENTVIEWSTATE);
    	pState->nId = m_nListId;
    }
    CView::OnDestroy();
}


///////////////////////////////////////////////////////////////
// CViewComments drawing

void CViewComments::OnUpdate(CView *pSender, LPARAM lHint, CObject *)
//-------------------------------------------------------------------
{
    //CHAR s[256], stmp[256];
    CHAR s[512], stmp[256]; //rewbs.fix3082
    CModDoc *pModDoc = GetDocument();
    module_renderer *pSndFile;
    LV_COLUMN lvc;
    LV_ITEM lvi, lvi2;

    if ((!pModDoc) || (pSender == this) || (!(m_ItemList.m_hWnd))) return;
    if (lHint & HINT_MPTOPTIONS)
    {
    	m_ToolBar.UpdateStyle();
    	lHint &= ~HINT_MPTOPTIONS;
    }
    lHint &= (HINT_MODTYPE
    	|HINT_SMPNAMES|HINT_SAMPLEINFO
    	|HINT_INSNAMES|HINT_INSTRUMENT
    	/*|HINT_PATNAMES|HINT_PATTERNROW*/); // pattern stuff currently unused
    if (!lHint) return;
    m_ItemList.SetRedraw(FALSE);
    // Add sample headers
    if ((m_nListId != m_nCurrentListId) || (lHint & HINT_MODTYPE))
    {
    	UINT ichk = 0;
    	m_ItemList.DeleteAllItems();
    	while ((m_ItemList.DeleteColumn(0)) && (ichk < 25)) ichk++;
    	m_nCurrentListId = m_nListId;
    	// Add Sample Headers
    	if (m_nCurrentListId == IDC_LIST_SAMPLES)
    	{
    		UINT nCol = 0;
    		for (UINT iSmp=0; iSmp<SMPLIST_COLUMNS; iSmp++)
    		{
    			memset(&lvc, 0, sizeof(LV_COLUMN));
    			lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    			lvc.fmt = (iSmp) ? LVCFMT_RIGHT : LVCFMT_LEFT;
    			lvc.pszText = (LPSTR)gSampleHeaders[iSmp].pszName;
    			lvc.cx = gSampleHeaders[iSmp].cx;
    			lvc.iSubItem = iSmp;
    			m_ItemList.InsertColumn(nCol, &lvc);
    			nCol++;
    		}
    	} else
    	// Add Instrument Headers
    	if (m_nCurrentListId == IDC_LIST_INSTRUMENTS)
    	{
    		UINT nCol = 0;
    		for (UINT i=0; i<INSLIST_COLUMNS; i++)
    		{
    			memset(&lvc, 0, sizeof(LV_COLUMN));
    			lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    			lvc.fmt = (i) ? LVCFMT_RIGHT : LVCFMT_LEFT;
    			lvc.pszText = (LPSTR)gInstrumentHeaders[i].pszName;
    			lvc.cx = gInstrumentHeaders[i].cx;
    			lvc.iSubItem = i;
    			m_ItemList.InsertColumn(nCol, &lvc);
    			nCol++;
    		}
    	} else
    	lHint |= HINT_MODTYPE;
    }
    // Add Items
    UINT nCount = m_ItemList.GetItemCount();
    pSndFile = pModDoc->GetSoundFile();
    // Add Samples
    if ((m_nCurrentListId == IDC_LIST_SAMPLES) && (lHint & (HINT_MODTYPE|HINT_SMPNAMES|HINT_SAMPLEINFO)))
    {
    	UINT nMax = nCount;
    	if (nMax < pSndFile->m_nSamples) nMax = pSndFile->m_nSamples;
    	for (UINT iSmp=0; iSmp<nMax; iSmp++)
    	{
    		if (iSmp < pSndFile->m_nSamples)
    		{
    			UINT nCol = 0;
    			for (UINT iCol=0; iCol<SMPLIST_COLUMNS; iCol++)
    			{
    				modplug::tracker::modsample_t *pSmp = &pSndFile->Samples[iSmp+1];
    				s[0] = 0;
    				switch(iCol)
    				{
    				case SMPLIST_SAMPLENAME:
    					lstrcpyn(s, pSndFile->m_szNames[iSmp+1], MAX_SAMPLENAME);
    					break;
    				case SMPLIST_SAMPLENO:
    					wsprintf(s, "%02d", iSmp+1);
    					break;
    				case SMPLIST_SIZE:
    					if (pSmp->length)
    					{
    						if(pSmp->GetSampleSizeInBytes() >= 1024)
    							wsprintf(s, "%d KB", pSmp->GetSampleSizeInBytes() >> 10);
    						else
    							wsprintf(s, "%d B", pSmp->GetSampleSizeInBytes());
    					}
    					break;
    				case SMPLIST_TYPE:
    					if (pSmp->length)
    					{
    						wsprintf(s, "%d Bit", pSmp->GetElementarySampleSize() << 3);
    					}
    					break;
    				case SMPLIST_INSTR:
    					if (pSndFile->m_nInstruments)
    					{
    						UINT k = 0;
    						for (UINT i=0; i<pSndFile->m_nInstruments; i++) if (pSndFile->Instruments[i+1])
    						{
    							modplug::tracker::modinstrument_t *pIns = pSndFile->Instruments[i+1];
    							for (UINT j=0; j<NOTE_MAX; j++)
    							{
    								if ((UINT)pIns->Keyboard[j] == (iSmp+1))
    								{
    									if (k) strcat(s, ",");
    									wsprintf(stmp, "%d", i+1);
    									strcat(s, stmp);
    									k++;
    									break;
    								}
    							}
    							if (strlen(s) > sizeof(s)-10)
    							{
    								strcat(s, "...");
    								break;
    							}
    						}
    					}
    					break;
    				case SMPLIST_MIDDLEC:
    					if (pSmp->length)
    					{
    						wsprintf(s, "%d Hz",
    							pSndFile->GetFreqFromPeriod(
    								pSndFile->GetPeriodFromNote(NOTE_MIDDLEC + pSmp->RelativeTone, pSmp->nFineTune, pSmp->c5_samplerate),
    								pSmp->c5_samplerate));
    					}
    					break;
    				case SMPLIST_FILENAME:
    					memcpy(s, pSmp->legacy_filename, sizeof(pSmp->legacy_filename));
    					s[sizeof(pSmp->legacy_filename)] = 0;
    					break;
    				}
    				lvi.mask = LVIF_TEXT;
    				lvi.iItem = iSmp;
    				lvi.iSubItem = nCol;
    				lvi.pszText = (LPTSTR)s;
    				if ((iCol) || (iSmp < nCount))
    				{
    					BOOL bOk = TRUE;
    					if (iSmp < nCount)
    					{
    						lvi2 = lvi;
    						lvi2.pszText = (LPTSTR)stmp;
    						lvi2.cchTextMax = sizeof(stmp);
    						stmp[0] = 0;
    						m_ItemList.GetItem(&lvi2);
    						if (!strcmp(s, stmp)) bOk = FALSE;
    					}
    					if (bOk) m_ItemList.SetItem(&lvi);
    				} else
    				{
    					m_ItemList.InsertItem(&lvi);
    				}
    				nCol++;
    			}
    		} else
    		{
    			m_ItemList.DeleteItem(iSmp);
    		}
    	}
    } else
    // Add Instruments
    if ((m_nCurrentListId == IDC_LIST_INSTRUMENTS) && (lHint & (HINT_MODTYPE|HINT_INSNAMES|HINT_INSTRUMENT)))
    {
    	UINT nMax = nCount;
    	if (nMax < pSndFile->m_nInstruments) nMax = pSndFile->m_nInstruments;
    	for (UINT iIns=0; iIns<nMax; iIns++)
    	{
    		if (iIns < pSndFile->m_nInstruments)
    		{
    			UINT nCol = 0;
    			for (UINT iCol=0; iCol<INSLIST_COLUMNS; iCol++)
    			{
    				modplug::tracker::modinstrument_t *pIns = pSndFile->Instruments[iIns+1];
    				s[0] = 0;
    				switch(iCol)
    				{
    				case INSLIST_INSTRUMENTNAME:
    					if (pIns) lstrcpyn(s, pIns->name, sizeof(pIns->name));
    					break;
    				case INSLIST_INSTRUMENTNO:
    					wsprintf(s, "%02d", iIns+1);
    					break;
    				case INSLIST_SAMPLES:
    					if (pIns)
    					{
    						uint8_t smp_tb[(MAX_SAMPLES+7)/8];
    						memset(smp_tb, 0, sizeof(smp_tb));
    						for (UINT i=0; i<NOTE_MAX; i++)
    						{
    							UINT n = pIns->Keyboard[i];
    							if ((n) && (n < MAX_SAMPLES)) smp_tb[n>>3] |= (1<<(n&7));
    						}
    						UINT k = 0;
    						for (UINT j=1; j<MAX_SAMPLES; j++) if (smp_tb[j>>3] & (1<<(j&7)))
    						{
    							if (k) strcat(s, ",");
    							UINT l = strlen(s);
    							if (l >= sizeof(s)-8)
    							{
    								strcat(s, "...");
    								break;
    							}
    							wsprintf(s+l, "%d", j);
    							k++;
    						}
    					}
    					break;
    				case INSLIST_ENVELOPES:
    					if (pIns)
    					{
    						if (pIns->volume_envelope.flags & ENV_ENABLED) strcat(s, "Vol");
    						if (pIns->panning_envelope.flags & ENV_ENABLED) { if (s[0]) strcat(s, ", "); strcat(s, "Pan"); }
    						if (pIns->pitch_envelope.flags & ENV_ENABLED) { if (s[0]) strcat(s, ", "); strcat(s, (pIns->pitch_envelope.flags & ENV_FILTER) ? "Filter" : "Pitch"); }
    					}
    					break;
    				case INSLIST_FILENAME:
    					if (pIns)
    					{
    						memcpy(s, pIns->legacy_filename, sizeof(pIns->legacy_filename));
    						s[sizeof(pIns->legacy_filename)] = 0;
    					}
    					break;
    				case INSLIST_PLUGIN:
    					if (pIns != nullptr && pIns->nMixPlug > 0 && pSndFile->m_MixPlugins[pIns->nMixPlug - 1].pMixPlugin != nullptr)
    					{
    						wsprintf(s, "FX%02d: %s", pIns->nMixPlug, pSndFile->m_MixPlugins[pIns->nMixPlug - 1].Info.szLibraryName);
    					}
    					break;
// -> CODE#0023
// -> DESC="IT project files (.itp)"
    				case INSLIST_PATH:
    					if (pIns)
    					{
    						memcpy(s, pSndFile->m_szInstrumentPath[iIns], _MAX_PATH);
    						s[_MAX_PATH] = 0;
    					}
    					break;
// -! NEW_FEATURE#0023
    				}
    				lvi.mask = LVIF_TEXT;
    				lvi.iItem = iIns;
    				lvi.iSubItem = nCol;
    				lvi.pszText = (LPTSTR)s;
    				if ((iCol) || (iIns < nCount))
    				{
    					BOOL bOk = TRUE;
    					if (iIns < nCount)
    					{
    						lvi2 = lvi;
    						lvi2.pszText = (LPTSTR)stmp;
    						lvi2.cchTextMax = sizeof(stmp);
    						stmp[0] = 0;
    						m_ItemList.GetItem(&lvi2);
    						if (!strcmp(s, stmp)) bOk = FALSE;
    					}
    					if (bOk) m_ItemList.SetItem(&lvi);
    				} else
    				{
    					m_ItemList.InsertItem(&lvi);
    				}
    				nCol++;
    			}
    		} else
    		{
    			m_ItemList.DeleteItem(iIns);
    		}
    	}
    } else
    // Add Patterns
    if ((m_nCurrentListId == IDC_LIST_PATTERNS) && (lHint & (HINT_MODTYPE|HINT_PATNAMES|HINT_PATTERNROW)))
    {
    }
    m_ItemList.SetRedraw(TRUE);
}


VOID CViewComments::RecalcLayout()
//--------------------------------
{
    CRect rect;

    if (!m_hWnd) return;
    GetClientRect(&rect);
    m_ToolBar.SetWindowPos(NULL, 0, 0, rect.Width(), DETAILS_TOOLBAR_CY, SWP_NOZORDER|SWP_NOACTIVATE);
    m_ItemList.SetWindowPos(NULL, -1, DETAILS_TOOLBAR_CY, rect.Width()+2, rect.Height() - DETAILS_TOOLBAR_CY + 1, SWP_NOZORDER|SWP_NOACTIVATE);
}


VOID CViewComments::UpdateButtonState()
//-------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if (pModDoc)
    {
    	module_renderer *pSndFile = pModDoc->GetSoundFile();
    	m_ToolBar.SetState(IDC_LIST_SAMPLES, ((m_nListId == IDC_LIST_SAMPLES) ? TBSTATE_CHECKED : 0)|TBSTATE_ENABLED);
    	m_ToolBar.SetState(IDC_LIST_INSTRUMENTS, ((m_nListId == IDC_LIST_INSTRUMENTS) ? TBSTATE_CHECKED : 0)|TBSTATE_ENABLED);
    	m_ToolBar.SetState(IDC_LIST_PATTERNS, ((m_nListId == IDC_LIST_PATTERNS) ? TBSTATE_CHECKED : 0)|TBSTATE_ENABLED);
    	m_ToolBar.EnableButton(IDC_LIST_INSTRUMENTS, (pSndFile->m_nInstruments) ? TRUE : FALSE);
    }
}


VOID CViewComments::OnEndLabelEdit(LPNMHDR pnmhdr, LRESULT *)
//-----------------------------------------------------------
{
    LV_DISPINFO *plvDispInfo = (LV_DISPINFO *)pnmhdr;
    LV_ITEM *plvItem = &plvDispInfo->item;
    CModDoc *pModDoc = GetDocument();
    CHAR s[512]; //rewbs.fix3082

    if ((plvItem->pszText != NULL) && (!plvItem->iSubItem) && (pModDoc))
    {
    	UINT iItem = plvItem->iItem;
    	module_renderer *pSndFile = pModDoc->GetSoundFile();
    	lstrcpyn(s, plvItem->pszText, sizeof(s));

    	size_t maxStrLen = (m_nListId == IDC_LIST_SAMPLES) ? pSndFile->GetModSpecifications().sampleNameLengthMax : pSndFile->GetModSpecifications().instrNameLengthMax;

    	for (size_t i = min(maxStrLen, strlen(s)); i < sizeof(s); i++)
    		s[i] = 0;

    	if (m_nListId == IDC_LIST_SAMPLES)
    	{
    		if (iItem < pSndFile->m_nSamples)
    		{
    			memcpy(pSndFile->m_szNames[iItem + 1], s, sizeof(pSndFile->m_szNames[iItem + 1]));
    			SetNullTerminator(pSndFile->m_szNames[iItem + 1]);
    			// 05/01/05 : ericus replaced "<< 24" by "<< 20" : 4000 samples -> 12bits [see Moddoc.h]
    			pModDoc->UpdateAllViews(this, ((iItem + 1) << HINT_SHIFT_SMP) | (HINT_SMPNAMES|HINT_SAMPLEINFO), this);
    			pModDoc->SetModified();
    		}
    	} else
    	if (m_nListId == IDC_LIST_INSTRUMENTS)
    	{
    		if ((iItem < pSndFile->m_nInstruments) && (pSndFile->Instruments[iItem + 1]))
    		{
    			modplug::tracker::modinstrument_t *pIns = pSndFile->Instruments[iItem+1];
    			memcpy(pIns->name, s, sizeof(pIns->name));
    			SetNullTerminator(pIns->name);
    			pModDoc->UpdateAllViews(this, ((iItem + 1) << HINT_SHIFT_INS) | (HINT_INSNAMES|HINT_INSTRUMENT), this);
    			pModDoc->SetModified();
    		}
    	} else
    	{
    		return;
    	}
    	m_ItemList.SetItemText(iItem, plvItem->iSubItem, s);
    }
}


///////////////////////////////////////////////////////////////
// CViewComments messages

// -> CODE#0015
// -> DESC="channels management dlg"
void CViewComments::OnDraw(CDC* pDC)
{
    CView::OnDraw(pDC);

    CMainFrame * pMainFrm = CMainFrame::GetMainFrame();
    BOOL activeDoc = pMainFrm ? pMainFrm->GetActiveDoc() == GetDocument() : FALSE;

    if(activeDoc && CChannelManagerDlg::sharedInstance(FALSE) && CChannelManagerDlg::sharedInstance()->IsDisplayed())
    	CChannelManagerDlg::sharedInstance()->SetDocument((void*)this);
}
// -! NEW_FEATURE#0015


void CViewComments::OnSize(UINT nType, int cx, int cy)
//----------------------------------------------------
{
    CView::OnSize(nType, cx, cy);
    if (((nType == SIZE_RESTORED) || (nType == SIZE_MAXIMIZED)) && (cx > 0) && (cy > 0) && (m_hWnd))
    {
    	RecalcLayout();
    }
}


void CViewComments::OnShowSamples()
//---------------------------------
{
    if (m_nListId != IDC_LIST_SAMPLES)
    {
    	m_nListId = IDC_LIST_SAMPLES;
    	UpdateButtonState();
    	OnUpdate(NULL, HINT_MODTYPE, NULL);
    }
}


void CViewComments::OnShowInstruments()
//-------------------------------------
{
    if (m_nListId != IDC_LIST_INSTRUMENTS)
    {
    	CModDoc *pModDoc = GetDocument();
    	if (pModDoc)
    	{
    		module_renderer *pSndFile = pModDoc->GetSoundFile();
    		if (pSndFile->m_nInstruments)
    		{
    			m_nListId = IDC_LIST_INSTRUMENTS;
    			UpdateButtonState();
    			OnUpdate(NULL, HINT_MODTYPE, NULL);
    		}
    	}
    }
}


void CViewComments::OnShowPatterns()
//----------------------------------
{
    if (m_nListId != IDC_LIST_PATTERNS)
    {
    	//m_nListId = IDC_LIST_PATTERNS;
    	UpdateButtonState();
    	OnUpdate(NULL, HINT_MODTYPE, NULL);
    }
}

void CViewComments::OnDblClickListItem(NMHDR *, LRESULT *)
//--------------------------------------------------------
{
    // Double click -> switch to instrument or sample tab
    int nItem = m_ItemList.GetSelectionMark();
    if(nItem == -1) return;
    CModDoc *pModDoc = GetDocument();
    if(!pModDoc) return;
    nItem++;

    switch(m_nListId)
    {
    case IDC_LIST_SAMPLES:
    	pModDoc->ViewSample(nItem);
    	break;
    case IDC_LIST_INSTRUMENTS:
    	pModDoc->ViewInstrument(nItem);
    	break;
    case IDC_LIST_PATTERNS:
    	pModDoc->ViewPattern(nItem, 0);
    	break;
    }
}


LRESULT CViewComments::OnModViewMsg(WPARAM wParam, LPARAM lParam)
//-----------------------------------------------------------------
{
//    switch(wParam)
//    {
//    	default:
    		return CModScrollView::OnModViewMsg(wParam, lParam);
//    }
}