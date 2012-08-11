/*
 * This source code is public domain.
 *
 * Copied to OpenMPT from libmodplug.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
*/

#include "stdafx.h"
#include "Loaders.h"

#define MODMAGIC_OFFSET    (20+31*30+130)


bool module_renderer::ReadUMX(const uint8_t *lpStream, const uint32_t dwMemLength)
//---------------------------------------------------------------------
{
    if ((!lpStream) || (dwMemLength < 0x800)) return false;
    // Rip Mods from UMX
    if ((LittleEndian(*((uint32_t *)(lpStream+0x20))) < dwMemLength)
     && (LittleEndian(*((uint32_t *)(lpStream+0x18))) <= dwMemLength - 0x10)
     && (LittleEndian(*((uint32_t *)(lpStream+0x18))) >= dwMemLength - 0x200))
    {
    	for (UINT uscan=0x40; uscan<0x500; uscan++)
    	{
    		uint32_t dwScan = LittleEndian(*((uint32_t *)(lpStream+uscan)));
    		// IT
    		if (dwScan == 0x4D504D49)
    		{
    			uint32_t dwRipOfs = uscan;
    			return ReadIT(lpStream + dwRipOfs, dwMemLength - dwRipOfs);
    		}
    		// S3M
    		if (dwScan == 0x4D524353)
    		{
    			uint32_t dwRipOfs = uscan - 44;
    			return ReadS3M(lpStream + dwRipOfs, dwMemLength - dwRipOfs);
    		}
    		// XM
    		if (!_strnicmp((LPCSTR)(lpStream+uscan), "Extended Module", 15))
    		{
    			uint32_t dwRipOfs = uscan;
    			return ReadXM(lpStream + dwRipOfs, dwMemLength - dwRipOfs);
    		}
    		// MOD
    		if ((uscan > MODMAGIC_OFFSET) && (dwScan == '.K.M'))
    		{
    			uint32_t dwRipOfs = uscan - MODMAGIC_OFFSET;
    			return ReadMod(lpStream+dwRipOfs, dwMemLength-dwRipOfs);
    		}
    	}
    }
    return false;
}