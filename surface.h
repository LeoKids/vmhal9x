/******************************************************************************
 * Copyright (c) 2025 Jaroslav Hensl                                          *
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
#ifndef __SURFACE_H__INCLUDED__
#define __SURFACE_H__INCLUDED__

typedef struct surface_info surface_info_t;

#define DDSURF_ATTACH_MAX 32

typedef struct _DDSURF
{
	FLATPTR fpVidMem;
	LPDDRAWI_DDRAWSURFACE_GBL lpGbl;
	LPDDRAWI_DDRAWSURFACE_LCL lpLcl;
	DWORD dwFlags;
	DWORD dwCaps;
	DWORD dwAttachments;
	LPDDRAWI_DDRAWSURFACE_GBL lpAttachmentsGbl[DDSURF_ATTACH_MAX];
	LPDDRAWI_DDRAWSURFACE_LCL lpAttachmentsLcl[DDSURF_ATTACH_MAX];
} DDSURF;

void SurfaceCopyLCL(LPDDRAWI_DDRAWSURFACE_LCL surf, DDSURF *dest);

#endif /* __SURFACE_H__INCLUDED__ */
