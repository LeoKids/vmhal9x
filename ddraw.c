/******************************************************************************
 * Copyright (c) 2023 Jaroslav Hensl                                          *
 *                                                                            *
 * Permission is hereby granted, free of charge, to any person                *
 * obtaining a copy of this software and associated documentation             *
 * files (the "Software"), to deal in the Software without                    *
 * restriction, including without limitation the rights to use,               *
 * copy, modify, merge, publish, distribute, sublicense, and/or sell          *
 * copies of the Software, and to permit persons to whom the                  *
 * Software is furnished to do so, subject to the following                   *
 * conditions:                                                                *
 *                                                                            *
 * The above copyright notice and this permission notice shall be             *
 * included in all copies or substantial portions of the Software.            *
 *                                                                            *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,            *
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES            *
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND                   *
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT                *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,               *
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING               *
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR              *
 * OTHER DEALINGS IN THE SOFTWARE.                                            *
 *                                                                            *
 ******************************************************************************/
#include <windows.h>
#include <ddraw.h>
#include <ddrawi.h>
#include <stdint.h>
#include "ddrawi_ddk.h"

#include "vmdahal32.h"
#include "vmhal9x.h"
#include "nocrt.h"

/* Display mode should by controled by system/HEL all time */
//#define SETMODE_BY_HAL

/* these are in DDRAW.DLL, very funny, very well documented... */
FLATPTR WINAPI DDHAL32_VidMemAlloc(LPDDRAWI_DIRECTDRAW_GBL lpDD, int heap, DWORD dwWidth, DWORD dwHeight);
void WINAPI DDHAL32_VidMemFree(LPDDRAWI_DIRECTDRAW_GBL lpDD, int heap, FLATPTR ptr);

#define IS_GLOBAL_ADDR(_ptr) (((DWORD)_ptr) >= 0x80000000UL)

VMDAHAL_t *GetHAL(LPDDRAWI_DIRECTDRAW_GBL lpDD)
{
	if(lpDD != NULL && IS_GLOBAL_ADDR(lpDD->dwReserved3)) // running on DX5 or greater?
		return (VMDAHAL_t*)lpDD->dwReserved3;
	else
		return globalHal;         // our global value for this
}

/* surfaces */

DDENTRY(CanCreateSurface32, LPDDHAL_CANCREATESURFACEDATA, pccsd)
{
	TRACE_ENTRY
	
	VMDAHAL_t *hal = GetHAL(pccsd->lpDD);
	if(!hal) return DDHAL_DRIVER_HANDLED;
	
	TOPIC("GL", "CanCreateSurface: 0x%X", pccsd->lpDDSurfaceDesc->ddsCaps.dwCaps);

	if(!pccsd->bIsDifferentPixelFormat)
	{
		pccsd->ddRVal = DD_OK;
		return DDHAL_DRIVER_HANDLED;
	}
	
	LPDDSURFACEDESC lpDDSurfaceDesc = pccsd->lpDDSurfaceDesc;

	if((lpDDSurfaceDesc->ddsCaps.dwCaps & 
		(DDSCAPS_TEXTURE | DDSCAPS_EXECUTEBUFFER | DDSCAPS_ZBUFFER /*| DDSCAPS_OVERLAY*/)) != 0)
	{
		pccsd->ddRVal = DD_OK;
		return DDHAL_DRIVER_HANDLED;
	}

	WARN("Cannot create surface: 0x%X", lpDDSurfaceDesc->ddsCaps.dwCaps);

	pccsd->ddRVal = DDERR_INVALIDPIXELFORMAT;
	return DDHAL_DRIVER_HANDLED;
} /* CanCreateSurface */

DDENTRY_FPUSAVE(CreateSurface32, LPDDHAL_CREATESURFACEDATA, pcsd)
{
	TRACE_ENTRY

 	VMDAHAL_t *hal = GetHAL(pcsd->lpDD);
  if(!hal) return DDHAL_DRIVER_NOTHANDLED;

#ifdef D3DHAL
	if(pcsd->lpDDSurfaceDesc->ddsCaps.dwCaps & (DDSCAPS_TEXTURE | DDSCAPS_ZBUFFER | DDSCAPS_FLIP |
		DDSCAPS_FRONTBUFFER | DDSCAPS_BACKBUFFER | DDSCAPS_PRIMARYSURFACE | DDSCAPS_OFFSCREENPLAIN))
	{
		LPDDRAWI_DDRAWSURFACE_LCL *lplpSList = pcsd->lplpSList;
		int i;

		for(i = 0; i < (int)pcsd->dwSCnt; i++)
		{
			LPDDRAWI_DDRAWSURFACE_LCL lpSurf = lplpSList[i];

			if(lpSurf->lpGbl->ddpfSurface.dwFlags & DDPF_FOURCC)
			{
				DWORD pitch;
				DWORD size = SurfaceDataSize(lpSurf->lpGbl, &pitch);

				lpSurf->lpGbl->lPitch = pitch; // for FOURCC needs to be fill
				if(i == 0)
				{
					pcsd->lpDDSurfaceDesc->lPitch = pitch;
				}
				
				lpSurf->lpGbl->dwBlockSizeX = size;
				lpSurf->lpGbl->dwBlockSizeY = 1;
				lpSurf->lpGbl->fpVidMem     = DDHAL_PLEASEALLOC_BLOCKSIZE;
				
				TOPIC("ALLOC", "FourCC %d x %d = %d", lpSurf->lpGbl->wWidth, lpSurf->lpGbl->wHeight, lpSurf->lpGbl->dwBlockSizeX);
			}
			else if(lpSurf->lpGbl->ddpfSurface.dwFlags & DDPF_RGB)
			{
				lpSurf->lpGbl->dwBlockSizeX = (DWORD)lpSurf->lpGbl->wHeight * lpSurf->lpGbl->lPitch;
				lpSurf->lpGbl->dwBlockSizeY = 1;
				lpSurf->lpGbl->fpVidMem     = DDHAL_PLEASEALLOC_BLOCKSIZE;

				TOPIC("CUBE", "Alloc RGB %d x %d = %d (pitch %d)", lpSurf->lpGbl->wWidth, lpSurf->lpGbl->wHeight, lpSurf->lpGbl->dwBlockSizeX, lpSurf->lpGbl->lPitch);
				TOPIC("MIPMAP", "Alloc RGB %d x %d = %d (pitch %d)", lpSurf->lpGbl->wWidth, lpSurf->lpGbl->wHeight, lpSurf->lpGbl->dwBlockSizeX, lpSurf->lpGbl->lPitch);
				TOPIC("ALLOC", "RGB %d x %d = %d (pitch %d)", lpSurf->lpGbl->wWidth, lpSurf->lpGbl->wHeight, lpSurf->lpGbl->dwBlockSizeX, lpSurf->lpGbl->lPitch);
			}
			else if(lpSurf->lpGbl->ddpfSurface.dwFlags & DDPF_ZBUFFER)
			{
				if(lpSurf->lpGbl->ddpfSurface.dwZBufferBitDepth >= 24)
				{
					lpSurf->lpGbl->lPitch = SurfacePitch(lpSurf->lpGbl->wWidth, 32);
				}
				
				lpSurf->lpGbl->dwBlockSizeX = (DWORD)lpSurf->lpGbl->wHeight * lpSurf->lpGbl->lPitch;
				lpSurf->lpGbl->dwBlockSizeY = 1;
				lpSurf->lpGbl->fpVidMem     = DDHAL_PLEASEALLOC_BLOCKSIZE;

				TOPIC("ALLOC", "ZBUF %d x %d = %d (pitch %d)", lpSurf->lpGbl->wWidth, lpSurf->lpGbl->wHeight, lpSurf->lpGbl->dwBlockSizeX, lpSurf->lpGbl->lPitch);
			}
			else
			{
				DWORD s = (DWORD)lpSurf->lpGbl->wHeight * lpSurf->lpGbl->lPitch;

				lpSurf->lpGbl->dwBlockSizeX = s;
				lpSurf->lpGbl->dwBlockSizeY = 1;
				lpSurf->lpGbl->fpVidMem     = DDHAL_PLEASEALLOC_BLOCKSIZE;

				TOPIC("ALLOC", "Unknown format %X, allocated primary surface (%d x %d) = %d",
					lpSurf->lpGbl->ddpfSurface.dwFlags,
					lpSurf->lpGbl->wWidth,
					lpSurf->lpGbl->wHeight, s);
			}
			
			SurfaceCreate(lpSurf);
			
			TOPIC("GL", "Mipmam %d/%d created", i+1, pcsd->dwSCnt);
		}
		pcsd->ddRVal = DD_OK;
		TOPIC("GL", "Texture created");
		return DDHAL_DRIVER_HANDLED;
	}
#endif
	if(pcsd->lplpSList)
	{
		WARN("Cannot create: 0x%X", pcsd->lpDDSurfaceDesc->ddsCaps.dwCaps);
		//SurfaceCreate(pcsd->lplpSList[0]);
	}

	WARN("Alloc by HEL, type 0x%X", pcsd->lpDDSurfaceDesc->ddsCaps.dwCaps);

	pcsd->ddRVal = DD_OK;
	return DDHAL_DRIVER_NOTHANDLED;
} /* CreateSurface32 */

DDENTRY_FPUSAVE(DestroySurface32, LPDDHAL_DESTROYSURFACEDATA, lpd)
{
	TRACE_ENTRY
	
 	VMDAHAL_t *hal = GetHAL(lpd->lpDD);
  if(!hal) return DDHAL_DRIVER_NOTHANDLED;

	TOPIC("DESTROY", "Detroying surface caps:  0x%X", lpd->lpDDSurface->ddsCaps.dwCaps);
	TOPIC("DESTROY", "Detroying surface flags: 0x%X, refs: %d", lpd->lpDDSurface->dwFlags, lpd->lpDDSurface->dwLocalRefCnt);
	TOPIC("DESTROY", "Detroying surface dim:  %d x %d (pitch: %d), global flags: 0x%X",
		lpd->lpDDSurface->lpGbl->wWidth,
		lpd->lpDDSurface->lpGbl->wHeight,
		lpd->lpDDSurface->lpGbl->lPitch,
		lpd->lpDDSurface->lpGbl->ddpfSurface.dwFlags);
	TOPIC("DESTROY", "VRAM ptr: 0x%X", lpd->lpDDSurface->lpGbl->fpVidMem);

	TOPIC("GARBAGE", "destroy surface vram=%X", lpd->lpDDSurface->lpGbl->fpVidMem);

	SurfaceDelete(lpd->lpDDSurface->dwReserved1);
	
	
	TOPIC("GARBAGE", "SurfaceDelete() success");
	lpd->ddRVal = DD_OK;
	return DDHAL_DRIVER_NOTHANDLED;
}

/* return tenths of millisecionds */
uint64_t GetTimeTMS()
{
	static LARGE_INTEGER freq = {0};
	LARGE_INTEGER stamp;
	
	if(freq.QuadPart == 0)
	{
		QueryPerformanceFrequency(&freq);
		freq.QuadPart /= 10*1000;
	}
	
	QueryPerformanceCounter(&stamp);
	
	return stamp.QuadPart / freq.QuadPart;
}

DDENTRY_FPUSAVE(WaitForVerticalBlank32, LPDDHAL_WAITFORVERTICALBLANKDATA, pwd)
{
	TRACE_ENTRY
	
	switch(pwd->dwFlags)
	{
		case DDWAITVB_I_TESTVB:
			/* just request for current blank status */
			/* JH: we haven't any display timing information in VM, but some application,
			       may waited in loop to vertical blank. Thats why I emulating VGA timing here.
			 */
			pwd->ddRVal = DD_OK;
			const DWORD period = 10000/60;
			DWORD frame_pos = GetTimeTMS() % period;
			DWORD visible_time = (period*4800)/5250;
			if(frame_pos <= visible_time)
			{
				pwd->bIsInVB = FALSE;
			}
			else
			{
				pwd->bIsInVB = TRUE;
			}
			return DDHAL_DRIVER_HANDLED;
			break;
		case DDWAITVB_BLOCKBEGIN:
			/* wait until vertical blank is over and wait display period to the end */
			pwd->ddRVal = DD_OK;
			return DDHAL_DRIVER_HANDLED;
			break;
		case DDWAITVB_BLOCKEND:
			/* wait for blank end */
			pwd->ddRVal = DD_OK;
			return DDHAL_DRIVER_HANDLED;
			break;
	}
	
	pwd->ddRVal = DD_OK;
	return DDHAL_DRIVER_NOTHANDLED;
}

DDENTRY_FPUSAVE(Lock32, LPDDHAL_LOCKDATA, pld)
{
	TRACE_ENTRY
	
	VMDAHAL_t *ddhal = GetHAL(pld->lpDD);
	if(IsInFront(ddhal, (void*)pld->lpDDSurface->lpGbl->fpVidMem))
	{
#if 0
		/* idea from sample driver, where they need wait to be done flipping before can be surface locked */
		uint64_t timestamp = GetTimeTMS();
		
		if(timestamp >= last_flip_time)
		{
			if((last_flip_time + SCREEN_TIME) >= timestamp)
			{
				pld->ddRVal = DDERR_WASSTILLDRAWING;
				return DDHAL_DRIVER_HANDLED;
			}
		}
#endif
		TOPIC("READBACK", "LOCK %X (primary)", pld->lpDDSurface->lpGbl->fpVidMem);
		FBHDA_access_begin(0);
	}
	else
	{
		TOPIC("READBACK", "LOCK %X (non primary)", pld->lpDDSurface->lpGbl->fpVidMem);
	}
	
	surface_id sid = pld->lpDDSurface->dwReserved1;
	if(sid)
	{
		if(SurfaceIsEmpty(sid))
		{
			SurfaceClearData(sid);
			TOPIC("CLEAR", "clear in lock 0x%X", pld->lpDDSurface->lpGbl->fpVidMem);
		}
		SurfaceFromMesa(pld->lpDDSurface, FALSE);
	}

	return DDHAL_DRIVER_NOTHANDLED; /* let the lock processed */
}

DDENTRY_FPUSAVE(Unlock32, LPDDHAL_UNLOCKDATA, pld)
{
	TRACE_ENTRY
	VMDAHAL_t *ddhal = GetHAL(pld->lpDD);
		
	if(IsInFront(ddhal, (void*)pld->lpDDSurface->lpGbl->fpVidMem))
	{
		FBHDA_access_end(0);
	}
	
	TOPIC("READBACK", "UNLOCK %X", pld->lpDDSurface->lpGbl->fpVidMem);
	TOPIC("DEPTHCONV", "Unlock32");
	SurfaceToMesa(pld->lpDDSurface, FALSE);

	return DDHAL_DRIVER_NOTHANDLED; /* let the unlock processed */
}

DDENTRY(SetExclusiveMode32, LPDDHAL_SETEXCLUSIVEMODEDATA, psem)
{
	TRACE_ENTRY

	//VMDAHAL_t *ddhal = GetHAL(psem->lpDD);

	if(psem->dwEnterExcl)
	{
		TRACE("exclusive mode: ON");
	}
	else
	{
		TRACE("exclusive mode: OFF");
	}
	
	psem->ddRVal = DD_OK;
	return DDHAL_DRIVER_HANDLED;
}

DDENTRY(SetMode32, LPDDHAL_SETMODEDATA, psmod)
{
	TRACE_ENTRY
	
	VMDAHAL_t *ddhal = GetHAL(psmod->lpDD);
	DWORD index = psmod->dwModeIndex;
	DWORD rc = DDHAL_DRIVER_NOTHANDLED;
	
#ifdef SETMODE_BY_HAL
	if(index < ddhal->modes_count)
	{
		DEVMODEA newmode;
		memset(&newmode, 0, sizeof(DEVMODEA));
		newmode.dmSize = sizeof(DEVMODEA);
		
		TRACE("SetMode32: %u", index);
	
		newmode.dmBitsPerPel  = ddhal->modes[index].dwBPP;
		newmode.dmPelsWidth   = ddhal->modes[index].dwWidth;
		newmode.dmPelsHeight  = ddhal->modes[index].dwHeight;
		newmode.dmFields      = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
	
		if(ChangeDisplaySettingsA(&newmode, CDS_RESET|CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL)
		{
			psmod->ddRVal = DD_OK;
			rc = DDHAL_DRIVER_HANDLED;
		}
	}
#endif

	if(index < ddhal->modes_count)
	{
		if(ddhal->modes[index].dwBPP   != ddhal->pFBHDA32->bpp ||
			ddhal->modes[index].dwWidth  != ddhal->pFBHDA32->width ||
			ddhal->modes[index].dwHeight != ddhal->pFBHDA32->height)
		{
			ddhal->invalid = TRUE;
		}
	}
	
	return rc;
}

DDENTRY_FPUSAVE(DestroyDriver32, LPDDHAL_DESTROYDRIVERDATA, pdstr)
{
	TRACE_ENTRY
	
	VMDAHAL_t *ddhal = GetHAL(pdstr->lpDD);
	
	if(ddhal)
	{
#ifdef SETMODE_BY_HAL	
		/* switch to system furface */
		FBHDA_swap(ddhal->pFBHDA32->system_surface);

		/* reset resolution when necessary */
		ChangeDisplaySettingsA(NULL, 0);
#endif

#ifdef D3DHAL
		Mesa3DCleanProc();
#endif
		//FBHDA_free();
	}
	
	pdstr->ddRVal = DD_OK;
	return DDHAL_DRIVER_HANDLED;
}

DDENTRY_FPUSAVE(SetColorKey32, LPDDHAL_SETCOLORKEYDATA, lpSetColorKey)
{
	TRACE_ENTRY

	VMDAHAL_t *ddhal = GetHAL(lpSetColorKey->lpDD);
	
	if(ddhal)
	{
		lpSetColorKey->lpDDSurface->dwFlags |= DDRAWISURF_HASCKEYSRCBLT;
		DWORD c1 = lpSetColorKey->ckNew.dwColorSpaceLowValue;
		DWORD c2 = lpSetColorKey->ckNew.dwColorSpaceHighValue;
		DWORD c1_32 = 0;
		DWORD c2_32 = 0;
		
		/* convert keys to 32bpp */
		switch(ddhal->pFBHDA32->bpp)
		{
			case 15:
				c1_32 = ((c1 & 0x001F) << 3) | ((c1 & 0x03E0) << 6) | ((c1 & 0x7C00) << 9);
				c2_32 = ((c2 & 0x001F) << 3) | ((c2 & 0x03E0) << 6) | ((c2 & 0x7C00) << 9);
				break;
			case 16:
				c1_32 = ((c1 & 0x001F) << 3) | ((c1 & 0x07E0) << 5) | ((c1 & 0xF800) << 8);
				c2_32 = ((c2 & 0x001F) << 3) | ((c2 & 0x07E0) << 5) | ((c2 & 0xF800) << 8);
				break;
			case 24:
			case 32:
			default:
				c1_32  = c1 & 0x00FFFFFFUL;
				c2_32 = c2 & 0x00FFFFFFUL;
				break;
		}
		
		lpSetColorKey->lpDDSurface->ddckCKSrcBlt.dwColorSpaceLowValue  = c1_32;
		lpSetColorKey->lpDDSurface->ddckCKSrcBlt.dwColorSpaceHighValue = c2_32;
		SurfaceApplyColorKey(lpSetColorKey->lpDDSurface->dwReserved1, c1_32, c2_32);
	}

	lpSetColorKey->ddRVal = DD_OK;
	return DDHAL_DRIVER_HANDLED;	
}
