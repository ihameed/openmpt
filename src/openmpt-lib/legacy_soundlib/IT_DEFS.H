#ifndef _ITDEFS_H_
#define _ITDEFS_H_

#pragma pack(1)

typedef struct tagITFILEHEADER
{
	uint32_t id;			// 0x4D504D49
	CHAR songname[26];
	uint8_t highlight_minor;
	uint8_t highlight_major;
	uint16_t ordnum;
	uint16_t insnum;
	uint16_t smpnum;
	uint16_t patnum;
	uint16_t cwtv;			// "made with" tracker
	uint16_t cmwt;			// "compatible with" tracker
	uint16_t flags;
	uint16_t special;
	uint8_t globalvol;
	uint8_t mv;			// master volume
	uint8_t speed;
	uint8_t tempo;
	uint8_t sep;			// panning separation (0...128)
	uint8_t pwd;			// pitch wheel depth
	uint16_t msglength;
	uint32_t msgoffset;
	uint32_t reserved;		// ChibiTracker writes "CHBI" here. OpenMPT writes "OMPT" here in some cases, see Load_it.cpp
	uint8_t chnpan[64];
	uint8_t chnvol[64];
} ITFILEHEADER;


typedef struct tagITENVELOPE
{
	uint8_t flags;
	uint8_t num;
	uint8_t lpb;
	uint8_t lpe;
	uint8_t slb;
	uint8_t sle;
	uint8_t data[25*3];
	uint8_t reserved;
} ITENVELOPE;

// Old Impulse Instrument Format (cmwt < 0x200)
typedef struct tagITOLDINSTRUMENT
{
	uint32_t id;			// IMPI = 0x49504D49
	CHAR filename[12];	// DOS file name
	uint8_t zero;
	uint8_t flags;
	uint8_t vls;
	uint8_t vle;
	uint8_t sls;
	uint8_t sle;
	uint16_t reserved1;
	uint16_t fadeout;
	uint8_t nna;
	uint8_t dnc;
	uint16_t trkvers;
	uint8_t nos;
	uint8_t reserved2;
	CHAR name[26];
	uint16_t reserved3[3];
	uint8_t keyboard[240];
	uint8_t volenv[200];
	uint8_t nodes[50];
} ITOLDINSTRUMENT;


// Impulse Instrument Format
typedef struct tagITINSTRUMENT
{
	uint32_t id;
	CHAR filename[12];
	uint8_t zero;
	uint8_t nna;
	uint8_t dct;
	uint8_t dca;
	uint16_t fadeout;
	signed char pps;
	uint8_t ppc;
	uint8_t gbv;
	uint8_t dfp;
	uint8_t rv;
	uint8_t rp;
	uint16_t trkvers;
	uint8_t nos;
	uint8_t reserved1;
	CHAR name[26];
	uint8_t ifc;
	uint8_t ifr;
	uint8_t mch;
	uint8_t mpr;
	uint16_t mbank;
	uint8_t keyboard[240];
	ITENVELOPE volenv;
	ITENVELOPE panenv;
	ITENVELOPE pitchenv;
	uint8_t dummy[4]; // was 7, but IT v2.17 saves 554 bytes
} ITINSTRUMENT;


// MPT IT Instrument Extension
typedef struct _ITINSTRUMENTEX
{
	ITINSTRUMENT iti;
	uint8_t keyboardhi[120];
} ITINSTRUMENTEX, *PITINSTRUMENTEX;


// IT Sample Format
typedef struct tagITSAMPLESTRUCT
{
	uint32_t id;		// 0x53504D49
	CHAR filename[12];
	uint8_t zero;
	uint8_t gvl;
	uint8_t flags;
	uint8_t vol;
	CHAR name[26];
	uint8_t cvt;
	uint8_t dfp;
	uint32_t length;
	uint32_t loopbegin;
	uint32_t loopend;
	uint32_t C5Speed;
	uint32_t susloopbegin;
	uint32_t susloopend;
	uint32_t samplepointer;
	uint8_t vis;
	uint8_t vid;
	uint8_t vir;
	uint8_t vit;
} ITSAMPLESTRUCT;


// IT Header extension: Save history
typedef struct tagITHISTORYSTRUCT
{
	uint16_t fatdate;	// DOS/FAT date when the file was opened / created in the editor. For details, read http://msdn.microsoft.com/en-us/library/ms724247(VS.85).aspx
	uint16_t fattime;	// DOS/FAT time when the file was opened / created in the editor.
	uint32_t runtime;	// The time how long the file was open in the editor, in 1/18.2th seconds. (= ticks of the DOS timer)
} ITHISTORYSTRUCT;

#pragma pack()

extern uint8_t autovibit2xm[8];
extern uint8_t autovibxm2it[8];

// Impulse Tracker identifcators
#define IT_IMPM 0x4D504D49		// "IMPM" IT header magic bytes 
#define IT_IMPS 0x53504D49		// "IMPS" IT sample header magic bytes
#define IT_IMPI	0x49504D49		// "IMPI" IT instrument header magic bytes

// Identificators by other trackers
#define IT_MPTM	0x2E6D7074		// "tpm." old MPTM header magic bytes
#define IT_OMPT 0x54504D4F		// "OMPT" magic bytes for non-standard OpenMPT IT files
#define IT_CHBI 0x49424843		// "CHBI" magic bytes in the IT header to identify ChibiTracker

enum IT_ReaderBitMasks
{
	// pattern row parsing, the channel data is read to obtain
	// number of channels active in the pattern. These bit masks are
	// to blank out sections of the byte of data being read.

	IT_bitmask_patternChanField_c   = 0x7f,
	IT_bitmask_patternChanMask_c    = 0x3f,
	IT_bitmask_patternChanEnabled_c = 0x80,
	IT_bitmask_patternChanUsed_c    = 0x0f

};

#endif