/******************************************************************************
 * Copyright (c) 2024 Jaroslav Hensl                                          *
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
#ifndef NUKED_SKIP
#include <windows.h>
#include <stddef.h>
#include <stdint.h>
#include <ddraw.h>
#include <ddrawi.h>
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include "vmdahal32.h"
#include "vmhal9x.h"
#include "mesa3d.h"
#include "osmesa.h"
#include "d3dhal_ddk.h"

#include "nocrt.h"
#endif

static void LightApply(mesa3d_ctx_t *ctx, DWORD id)
{
	TRACE_ENTRY

	if(id >= ctx->light.lights_size)
		return;

	mesa3d_light_t *light = ctx->light.lights[id];
	if(light == NULL)
		return;

	if(!light->active)
		return;

	mesa3d_entry_t *entry = ctx->entry;
	GLenum lindex = GL_LIGHT0 + light->active_index;

	GL_CHECK(entry->proc.pglLightfv(lindex, GL_DIFFUSE,  &light->diffuse[0]));
	GL_CHECK(entry->proc.pglLightfv(lindex, GL_SPECULAR, &light->specular[0]));
	GL_CHECK(entry->proc.pglLightfv(lindex, GL_AMBIENT,  &light->ambient[0]));

	switch(light->type)
	{
		case D3DLIGHT_POINT:
			TOPIC("LIGHT", "D3DLIGHT_POINT");
			GL_CHECK(entry->proc.pglLightfv(lindex, GL_POSITION,              &light->pos[0]));
			GL_CHECK(entry->proc.pglLightf(lindex,  GL_SPOT_EXPONENT,         0.0f));
			GL_CHECK(entry->proc.pglLightf(lindex,  GL_SPOT_CUTOFF,           180.0f));
			GL_CHECK(entry->proc.pglLightf(lindex,  GL_CONSTANT_ATTENUATION,  light->attenuation[0]));
			GL_CHECK(entry->proc.pglLightf(lindex,  GL_LINEAR_ATTENUATION,    light->attenuation[1]));
			GL_CHECK(entry->proc.pglLightf(lindex,  GL_QUADRATIC_ATTENUATION, light->attenuation[3]));
			break;
		case D3DLIGHT_SPOT:
			TOPIC("LIGHT", "D3DLIGHT_SPOT");
			GL_CHECK(entry->proc.pglLightfv(lindex, GL_POSITION,              &light->pos[0]));
			GL_CHECK(entry->proc.pglLightfv(lindex, GL_SPOT_DIRECTION,        &light->dir[0]));
			GL_CHECK(entry->proc.pglLightf(lindex,  GL_SPOT_EXPONENT,         light->exponent));
			GL_CHECK(entry->proc.pglLightf(lindex,  GL_SPOT_CUTOFF,           (light->phi * 90) / M_PI));
			GL_CHECK(entry->proc.pglLightf(lindex,  GL_CONSTANT_ATTENUATION,  light->attenuation[0]));
			GL_CHECK(entry->proc.pglLightf(lindex,  GL_LINEAR_ATTENUATION,    light->attenuation[1]));
			GL_CHECK(entry->proc.pglLightf(lindex,  GL_QUADRATIC_ATTENUATION, light->attenuation[3]));
			break;
		case D3DLIGHT_DIRECTIONAL:
		{
			TOPIC("LIGHT", "D3DLIGHT_DIRECTIONAL");
			GLfloat vd[4];
			vd[0] = -light->dir[0];
			vd[1] = -light->dir[1];
			vd[2] = -light->dir[2];
			vd[3] = 0.0f;

			/* Note GL uses w position of 0 for direction! */
			GL_CHECK(entry->proc.pglLightfv(lindex, GL_POSITION,      &vd[0]));
			GL_CHECK(entry->proc.pglLightf(lindex,  GL_SPOT_EXPONENT,   0.0f));
			GL_CHECK(entry->proc.pglLightf(lindex,  GL_SPOT_CUTOFF,   180.0f));
			GL_CHECK(entry->proc.pglLightf(lindex,  GL_CONSTANT_ATTENUATION,  1.0f));
			GL_CHECK(entry->proc.pglLightf(lindex,  GL_LINEAR_ATTENUATION,    0.0f));
			GL_CHECK(entry->proc.pglLightf(lindex,  GL_QUADRATIC_ATTENUATION, 0.0f));
			break;
		}
		case D3DLIGHT_PARALLELPOINT:
		case D3DLIGHT_GLSPOT:
		default:
			break;
	} // switch(light->type)
}

static void LightData(mesa3d_ctx_t *ctx, DWORD id, D3DLIGHT7 *dxlight)
{
	TRACE_ENTRY

	if(id >= ctx->light.lights_size)
		return;

	mesa3d_light_t *light = ctx->light.lights[id];
	if(light == NULL)
		return;

	TOPIC("LIGHT", "LightData light=%X, dxlight=%X", light, dxlight);

	light->type = dxlight->dltType;

	MESA_D3DCOLORVALUE_TO_FV(dxlight->dcvDiffuse,  light->diffuse);
	MESA_D3DCOLORVALUE_TO_FV(dxlight->dcvSpecular, light->specular);
	MESA_D3DCOLORVALUE_TO_FV(dxlight->dcvAmbient,  light->ambient);

	light->pos[0] = dxlight->dvPosition.x;
	light->pos[1] = dxlight->dvPosition.y;
	light->pos[2] = dxlight->dvPosition.z;	
	light->pos[3] = 1.0;

	light->dir[0] = dxlight->dvDirection.x;
	light->dir[1] = dxlight->dvDirection.y;
	light->dir[2] = dxlight->dvDirection.z;	
	light->dir[3] = 0.0;

	light->attenuation[0] = dxlight->dvAttenuation0;
	light->attenuation[1] = dxlight->dvAttenuation1;
	light->attenuation[2] = dxlight->dvAttenuation2;

	GLfloat range2 = dxlight->dvRange * dxlight->dvRange;
	if(range2 >= FLT_MIN)
		light->attenuation[3] = 1.4f/range2;
	else
		light->attenuation[3] = 0.0f; /*  0 or  MAX?  (0 seems to be ok) */

	if(light->attenuation[3] < light->attenuation[2])
		light->attenuation[3] = light->attenuation[2];

	light->range   = dxlight->dvRange;
	light->falloff = dxlight->dvFalloff;
	light->theta   = dxlight->dvTheta;
	light->phi     = dxlight->dvPhi;

	light->exponent = 0.0f;

	if(dxlight->dltType == D3DLIGHT_SPOT)
	{
		/*
		 *	Pre-calculate exponent for spot light,
		 *	comment and algoritm from WINE:
		 *		opengl-ish and d3d-ish spot lights use too different models
		 *		for the light "intensity" as a function of the angle towards
		 *		the main light direction, so we only can approximate very
		 *		roughly. However, spot lights are rather rarely used in games
		 *		(if ever used at all). Furthermore if still used, probably
		 *		nobody pays attention to such details.
		 *
		 *		Falloff = 0 is easy, because d3d's and opengl's spot light
		 *		equations have the falloff resp. exponent parameter as an
		 *		exponent, so the spot light lighting will always be 1.0 for
		 *		both of them, and we don't have to care for the rest of the
		 *		rather complex calculation.
		 */
		if(dxlight->dvFalloff)
		{
			GLfloat rho = dxlight->dvTheta + (dxlight->dvPhi - dxlight->dvTheta) / (2 * dxlight->dvFalloff);
			if (rho < 0.0001f)
			{
				rho = 0.0001f;
			}
			light->exponent = -0.3f / logf(cosf(rho / 2));

			if(light->exponent > 128.0f)
			{
				light->exponent = 128.0f;
			}
		}
	}
	
	LightApply(ctx, id);
}

static void LightCreate(mesa3d_ctx_t *ctx, DWORD id)
{
	TRACE_ENTRY

	if(id >= ctx->light.lights_size)
	{
		DWORD new_size = (id + 8);
		if(ctx->light.lights == NULL)
		{
			ctx->light.lights = HeapAlloc(hSharedHeap, HEAP_ZERO_MEMORY,
				new_size*sizeof(mesa3d_light_t*));
		}
		else
		{
			ctx->light.lights = HeapReAlloc(hSharedHeap, HEAP_ZERO_MEMORY, ctx->light.lights,
				new_size*sizeof(mesa3d_light_t*));
		}
		ctx->light.lights_size = new_size;
	}

	if(ctx->light.lights[id] == NULL)
	{
		ctx->light.lights[id] = HeapAlloc(hSharedHeap, HEAP_ZERO_MEMORY, sizeof(mesa3d_light_t));
		
		TOPIC("LIGHT", "Light %d created!", id);
	}
}

NUKED_LOCAL void MesaLightDestroyAll(mesa3d_ctx_t *ctx)
{
	TRACE_ENTRY

	DWORD i;
	if(ctx->light.lights)
	{
		for(i = 0; i < ctx->light.lights_size; i++)
		{
			if(ctx->light.lights[i] != NULL)
			{
				HeapFree(hSharedHeap, 0, ctx->light.lights[i]);
			}
		}

		HeapFree(hSharedHeap, 0, ctx->light.lights);
		ctx->light.lights_size = 0;
		ctx->light.lights = NULL;
	}
}

static void LightActive(mesa3d_ctx_t *ctx, DWORD id, BOOL activate)
{
	TRACE_ENTRY

	int gl_id = -1;
	int i = 0;

	if(id >= ctx->light.lights_size)
		return;

	mesa3d_light_t *light = ctx->light.lights[id];
	if(light == NULL)
		return;

	if(light->active)
	{
		if(!activate)
		{
			ctx->entry->proc.pglDisable(GL_LIGHT0 + light->active_index);
			light->active = FALSE;
			ctx->light.active_bitfield &= ~(1 << light->active_index);
		}
	}
	else
	{
		if(activate)
		{
			DWORD t = 1;
			for(i = 0; i < VMHALenv.num_light; i++, t <<= 1)
			{
				if((ctx->light.active_bitfield & t) == 0)
				{
					gl_id = i;
					break;
				}
			}

			if(gl_id >= 0)
			{
				TOPIC("LIGHT", "light %d actived at position %d", id, gl_id);

				light->active = TRUE;
				light->active_index = gl_id;
				ctx->light.active_bitfield |= 1 << gl_id;
				ctx->entry->proc.pglEnable(GL_LIGHT0 + gl_id);
				
				MesaSpaceModelviewSet(ctx);
				LightApply(ctx, id);
				MesaSpaceModelviewReset(ctx);
				
			}
		}
	}
}

static void PlaneApply(mesa3d_ctx_t *ctx, DWORD id)
{
	mesa3d_entry_t *entry = ctx->entry;

	GL_CHECK(entry->proc.pglClipPlane(GL_CLIP_PLANE0 + id,
		&ctx->state.clipping.plane[id][0]));
}

NUKED_LOCAL void MesaTLRecalcModelview(mesa3d_ctx_t *ctx)
{
	int i;

	/* recal clips */
	for(i = 0; i < MESA_CLIPS_MAX; i++)
	{
		PlaneApply(ctx, i);
	}

	/* recal lights */
	for(i = 0; i < ctx->light.lights_size; i++)
	{
		if(ctx->light.lights[i] != NULL)
		{
			if(ctx->light.lights[i]->active)
			{
				LightApply(ctx, i);
			}
		}
	}
}

#define NEXT_INST(_s) inst = (LPD3DHAL_DP2COMMAND)(prim + (_s))

// Vertices in an IMM instruction are stored in the
// command buffer and are DWORD aligned
#define PRIM_ALIGN prim = (LPBYTE)((ULONG_PTR)(prim + 3) & ~3)
#define COMMAND(_s) case _s: TRACE("%s", #_s);

#define SET_DIRTY \
	ctx->render.dirty = TRUE; \
	ctx->render.zdirty = TRUE

NUKED_LOCAL BOOL MesaDraw6(mesa3d_ctx_t *ctx, LPBYTE cmdBufferStart, LPBYTE cmdBufferEnd, LPBYTE vertices, DWORD *error_offset, LPDWORD RStates)
{
	TOPIC("READBACK", "MesaDraw6");
	
	mesa3d_entry_t *entry = ctx->entry;
	
//	MesaDrawSetSurfaces(ctx);

	LPD3DHAL_DP2COMMAND inst = (LPD3DHAL_DP2COMMAND)cmdBufferStart;
	DWORD start, count, base;
	int i;
	WORD *pos;
	
	while((LPBYTE)inst < cmdBufferEnd)
	{
		LPBYTE prim = (LPBYTE)(inst + 1);
		switch((D3DHAL_DP2OPERATION)inst->bCommand)
		{
			COMMAND(D3DDP2OP_POINTS)
				// Point primitives in vertex buffers are defined by the 
				// D3DHAL_DP2POINTS structure. The driver should render
				// wCount points starting at the initial vertex specified 
				// by wFirst. Then for each D3DHAL_DP2POINTS, the points
				// rendered will be (wFirst),(wFirst+1),...,
				// (wFirst+(wCount-1)). The number of D3DHAL_DP2POINTS
				// structures to process is specified by the wPrimitiveCount
				// field of D3DHAL_DP2COMMAND.
				start = ((D3DHAL_DP2POINTS*)prim)->wVStart;
				count = ((D3DHAL_DP2POINTS*)prim)->wCount;
				MesaDrawFVFs(ctx, GL_POINTS, vertices, start, count);
				SET_DIRTY;
				NEXT_INST(sizeof(D3DHAL_DP2POINTS));
				break;
			COMMAND(D3DDP2OP_LINELIST)
				// Non-indexed vertex-buffer line lists are defined by the 
				// D3DHAL_DP2LINELIST structure. Given an initial vertex, 
				// the driver will render a sequence of independent lines, 
				// processing two new vertices with each line. The number 
				// of lines to render is specified by the wPrimitiveCount
				// field of D3DHAL_DP2COMMAND. The sequence of lines 
				// rendered will be 
				// (wVStart, wVStart+1),(wVStart+2, wVStart+3),...,
				// (wVStart+(wPrimitiveCount-1)*2), wVStart+wPrimitiveCount*2 - 1).
				start = ((D3DHAL_DP2LINELIST*)prim)->wVStart;
				MesaDrawFVFs(ctx, GL_LINES, vertices, start, inst->wPrimitiveCount*2);
				SET_DIRTY;
				NEXT_INST(sizeof(D3DHAL_DP2LINELIST));
				break;
			COMMAND(D3DDP2OP_LINESTRIP)
				// Non-index line strips rendered with vertex buffers are
				// specified using D3DHAL_DP2LINESTRIP. The first vertex 
				// in the line strip is specified by wVStart. The 
				// number of lines to process is specified by the 
				// wPrimitiveCount field of D3DHAL_DP2COMMAND. The sequence
				// of lines rendered will be (wVStart, wVStart+1),
				// (wVStart+1, wVStart+2),(wVStart+2, wVStart+3),...,
				// (wVStart+wPrimitiveCount, wVStart+wPrimitiveCount+1).
				start = ((D3DHAL_DP2LINESTRIP*)prim)->wVStart;
				MesaDrawFVFs(ctx, GL_LINE_STRIP, vertices, start, inst->wPrimitiveCount+1);
				SET_DIRTY;
				NEXT_INST(sizeof(D3DHAL_DP2LINESTRIP));
				break;
			COMMAND(D3DDP2OP_TRIANGLELIST)
				// Non-indexed vertex buffer triangle lists are defined by 
				// the D3DHAL_DP2TRIANGLELIST structure. Given an initial
				// vertex, the driver will render independent triangles, 
				// processing three new vertices with each triangle. The
				// number of triangles to render is specified by the 
				// wPrimitveCount field of D3DHAL_DP2COMMAND. The sequence
				// of vertices processed will be  (wVStart, wVStart+1, 
				// vVStart+2), (wVStart+3, wVStart+4, vVStart+5),...,
				// (wVStart+(wPrimitiveCount-1)*3), wVStart+wPrimitiveCount*3-2, 
				// vStart+wPrimitiveCount*3-1).
				start = ((D3DHAL_DP2TRIANGLELIST*)prim)->wVStart;
				MesaDrawFVFs(ctx, GL_TRIANGLES, vertices, start, inst->wPrimitiveCount*3);
				SET_DIRTY;
				NEXT_INST(sizeof(D3DHAL_DP2TRIANGLELIST));
				break;
			COMMAND(D3DDP2OP_TRIANGLESTRIP)
				// Non-index triangle strips rendered with vertex buffers 
				// are specified using D3DHAL_DP2TRIANGLESTRIP. The first 
				// vertex in the triangle strip is specified by wVStart. 
				// The number of triangles to process is specified by the 
				// wPrimitiveCount field of D3DHAL_DP2COMMAND. The sequence
				// of triangles rendered for the odd-triangles case will 
				// be (wVStart, wVStart+1, vVStart+2), (wVStart+1, 
				// wVStart+3, vVStart+2),.(wVStart+2, wVStart+3, 
				// vVStart+4),.., (wVStart+wPrimitiveCount-1), 
				// wVStart+wPrimitiveCount, vStart+wPrimitiveCount+1). For an
				// even number of , the last triangle will be .,
				// (wVStart+wPrimitiveCount-1, vStart+wPrimitiveCount+1,
				// wVStart+wPrimitiveCount).
				start = ((D3DHAL_DP2TRIANGLESTRIP*)prim)->wVStart;
				MesaDrawFVFs(ctx, GL_TRIANGLE_STRIP, vertices, start, inst->wPrimitiveCount+2);
				SET_DIRTY;
				NEXT_INST(sizeof(D3DHAL_DP2TRIANGLESTRIP));
				break;
			COMMAND(D3DDP2OP_TRIANGLEFAN)
				// The D3DHAL_DP2TRIANGLEFAN structure is used to draw 
				// non-indexed triangle fans. The sequence of triangles
				// rendered will be (wVstart+1, wVStart+2, wVStart),
				// (wVStart+2,wVStart+3,wVStart), (wVStart+3,wVStart+4
				// wVStart),...,(wVStart+wPrimitiveCount,
				// wVStart+wPrimitiveCount+1,wVStart).
				start = ((D3DHAL_DP2TRIANGLEFAN*)prim)->wVStart;
				MesaDrawFVFs(ctx, GL_TRIANGLE_FAN, vertices, start, inst->wPrimitiveCount+2);
				SET_DIRTY;
				NEXT_INST(sizeof(D3DHAL_DP2TRIANGLEFAN));
				break;
			COMMAND(D3DDP2OP_INDEXEDLINELIST)
				// The D3DHAL_DP2INDEXEDLINELIST structure specifies 
				// unconnected lines to render using vertex indices.
				// The line endpoints for each line are specified by wV1 
				// and wV2. The number of lines to render using this 
				// structure is specified by the wPrimitiveCount field of
				// D3DHAL_DP2COMMAND.  The sequence of lines 
				// rendered will be (wV[0], wV[1]), (wV[2], wV[3]),...
				// (wVStart[(wPrimitiveCount-1)*2], wVStart[wPrimitiveCount*2-1]).
				entry->proc.pglBegin(GL_LINES);
				for(i = 0; i < inst->wPrimitiveCount; i++)
				{
					start = ((D3DHAL_DP2INDEXEDLINELIST*)prim)->wV1;
					MesaDrawFVFIndex(ctx, vertices, start);

					start = ((D3DHAL_DP2INDEXEDLINELIST*)prim)->wV2;
					MesaDrawFVFIndex(ctx, vertices, start);

					prim += sizeof(D3DHAL_DP2INDEXEDLINELIST);
				}
				entry->proc.pglEnd();
				SET_DIRTY;
				NEXT_INST(0);
				break;
			COMMAND(D3DDP2OP_INDEXEDLINESTRIP)
				// Indexed line strips rendered with vertex buffers are 
				// specified using D3DHAL_DP2INDEXEDLINESTRIP. The number
				// of lines to process is specified by the wPrimitiveCount
				// field of D3DHAL_DP2COMMAND. The sequence of lines 
				// rendered will be (wV[0], wV[1]), (wV[1], wV[2]),
				// (wV[2], wV[3]), ...
				// (wVStart[wPrimitiveCount-1], wVStart[wPrimitiveCount]). 
				// Although the D3DHAL_DP2INDEXEDLINESTRIP structure only
				// has enough space allocated for a single line, the wV 
				// array of indices should be treated as a variable-sized 
				// array with wPrimitiveCount+1 elements.
				// The indexes are relative to a base index value that 
				// immediately follows the command
				base = ((D3DHAL_DP2STARTVERTEX*)prim)->wVStart;
				prim += sizeof(D3DHAL_DP2STARTVERTEX);

				entry->proc.pglBegin(GL_LINE_STRIP);
				for(i = 0; i < inst->wPrimitiveCount+1; i++)
				{
					start = ((D3DHAL_DP2INDEXEDLINESTRIP*)prim)->wV[0];
					MesaDrawFVFIndex(ctx, vertices, base+start);

					prim += sizeof(WORD);
				}
				entry->proc.pglEnd();
				SET_DIRTY;
				NEXT_INST(0);
				break;
			COMMAND(D3DDP2OP_INDEXEDTRIANGLELIST)
				// The D3DHAL_DP2INDEXEDTRIANGLELIST structure specifies 
				// unconnected triangles to render with a vertex buffer.
				// The vertex indices are specified by wV1, wV2 and wV3. 
				// The wFlags field allows specifying edge flags identical 
				// to those specified by D3DOP_TRIANGLE. The number of 
				// triangles to render (that is, number of 
				// D3DHAL_DP2INDEXEDTRIANGLELIST structures to process) 
				// is specified by the wPrimitiveCount field of 
				// D3DHAL_DP2COMMAND.

				// This is the only indexed primitive where we don't get 
				// an offset into the vertex buffer in order to maintain
				// DX3 compatibility. A new primitive 
				// (D3DDP2OP_INDEXEDTRIANGLELIST2) has been added to handle
				// the corresponding DX6 primitive.
				entry->proc.pglBegin(GL_TRIANGLES);
				for(i = 0; i < inst->wPrimitiveCount; i++)
				{
					start = ((D3DHAL_DP2INDEXEDTRIANGLELIST*)prim)->wV3;
					MesaDrawFVFIndex(ctx, vertices, start);

					start = ((D3DHAL_DP2INDEXEDTRIANGLELIST*)prim)->wV2;
					MesaDrawFVFIndex(ctx, vertices, start);

					start = ((D3DHAL_DP2INDEXEDTRIANGLELIST*)prim)->wV1;
					MesaDrawFVFIndex(ctx, vertices, start);

					prim += sizeof(D3DHAL_DP2INDEXEDTRIANGLELIST);
				}
				entry->proc.pglEnd();
				SET_DIRTY;
				NEXT_INST(0);
				break;
			COMMAND(D3DDP2OP_INDEXEDTRIANGLESTRIP)
				// Indexed triangle strips rendered with vertex buffers are 
				// specified using D3DHAL_DP2INDEXEDTRIANGLESTRIP. The number
				// of triangles to process is specified by the wPrimitiveCount
				// field of D3DHAL_DP2COMMAND. The sequence of triangles 
				// rendered for the odd-triangles case will be 
				// (wV[0],wV[1],wV[2]),(wV[1],wV[3],wV[2]),
				// (wV[2],wV[3],wV[4]),...,(wV[wPrimitiveCount-1],
				// wV[wPrimitiveCount],wV[wPrimitiveCount+1]). For an even
				// number of triangles, the last triangle will be
				// (wV[wPrimitiveCount-1],wV[wPrimitiveCount+1],
				// wV[wPrimitiveCount]).Although the 
				// D3DHAL_DP2INDEXEDTRIANGLESTRIP structure only has 
				// enough space allocated for a single line, the wV 
				// array of indices should be treated as a variable-sized 
				// array with wPrimitiveCount+2 elements.
				// The indexes are relative to a base index value that 
				// immediately follows the command
				base = ((D3DHAL_DP2STARTVERTEX*)prim)->wVStart;
				prim += sizeof(D3DHAL_DP2STARTVERTEX);
				pos = (WORD*)prim;
				count = inst->wPrimitiveCount+2;
				prim += sizeof(WORD) * count;
#if 1
				if(count == 4)
				{
					entry->proc.pglBegin(GL_QUADS);
					MesaDrawFVFIndex(ctx, vertices, base+pos[3]);
					MesaDrawFVFIndex(ctx, vertices, base+pos[1]);
					MesaDrawFVFIndex(ctx, vertices, base+pos[0]);
					MesaDrawFVFIndex(ctx, vertices, base+pos[2]);
					entry->proc.pglEnd();
				}
				else
#endif
				{
					entry->proc.pglBegin(GL_TRIANGLES);
					MesaDrawFVFIndex(ctx, vertices, base+pos[count-1]);
					MesaDrawFVFIndex(ctx, vertices, base+pos[count-3]);
					MesaDrawFVFIndex(ctx, vertices, base+pos[count-2]);
					entry->proc.pglEnd();
					
					if(count >= 4)
					{
						entry->proc.pglBegin(GL_TRIANGLE_STRIP);
						for(i = count-2; i >= 0; i--)
						{
							MesaDrawFVFIndex(ctx, vertices, base+pos[i]);
						}
						entry->proc.pglEnd();
					}
				}
				SET_DIRTY;
				NEXT_INST(0);
				break;
			COMMAND(D3DDP2OP_INDEXEDTRIANGLEFAN)
				// The D3DHAL_DP2INDEXEDTRIANGLEFAN structure is used to 
				// draw indexed triangle fans. The sequence of triangles
				// rendered will be (wV[1], wV[2],wV[0]), (wV[2], wV[3],
				// wV[0]), (wV[3], wV[4], wV[0]),...,
				// (wV[wPrimitiveCount], wV[wPrimitiveCount+1],wV[0]).
				// The indexes are relative to a base index value that 
				// immediately follows the command
				base = ((D3DHAL_DP2STARTVERTEX*)prim)->wVStart;
				prim += sizeof(D3DHAL_DP2STARTVERTEX);
				pos = (WORD*)prim;
				prim += sizeof(WORD)*(inst->wPrimitiveCount+2);

				entry->proc.pglBegin(GL_TRIANGLE_FAN);
				MesaDrawFVFIndex(ctx, vertices, base+pos[0]);
				for(i = inst->wPrimitiveCount; i >= 1; i--)
				{
					MesaDrawFVFIndex(ctx, vertices, base+pos[i]);
				}
				entry->proc.pglEnd();

				SET_DIRTY;
				NEXT_INST(0);
				break;
			COMMAND(D3DDP2OP_INDEXEDTRIANGLELIST2)
				// The D3DHAL_DP2INDEXEDTRIANGLELIST2 structure specifies 
				// unconnected triangles to render with a vertex buffer.
				// The vertex indices are specified by wV1, wV2 and wV3. 
				// The wFlags field allows specifying edge flags identical 
				// to those specified by D3DOP_TRIANGLE. The number of 
				// triangles to render (that is, number of 
				// D3DHAL_DP2INDEXEDTRIANGLELIST structures to process) 
				// is specified by the wPrimitiveCount field of 
				// D3DHAL_DP2COMMAND.
				// The indexes are relative to a base index value that 
				// immediately follows the command
				base = ((D3DHAL_DP2STARTVERTEX*)prim)->wVStart;
				prim += sizeof(D3DHAL_DP2STARTVERTEX);
				entry->proc.pglBegin(GL_TRIANGLES);
				for(i = 0; i < inst->wPrimitiveCount; i++)
				{
					start = base + ((D3DHAL_DP2INDEXEDTRIANGLELIST2*)prim)->wV3;
					MesaDrawFVFIndex(ctx, vertices, start);

					start = base + ((D3DHAL_DP2INDEXEDTRIANGLELIST2*)prim)->wV2;
					MesaDrawFVFIndex(ctx, vertices, start);

					start = base + ((D3DHAL_DP2INDEXEDTRIANGLELIST2*)prim)->wV1;
					MesaDrawFVFIndex(ctx, vertices, start);

					prim += sizeof(D3DHAL_DP2INDEXEDTRIANGLELIST2);
				}
				entry->proc.pglEnd();
				SET_DIRTY;
				NEXT_INST(0);
				break;
			COMMAND(D3DDP2OP_INDEXEDLINELIST2)
				// The D3DHAL_DP2INDEXEDLINELIST structure specifies 
				// unconnected lines to render using vertex indices.
				// The line endpoints for each line are specified by wV1 
				// and wV2. The number of lines to render using this 
				// structure is specified by the wPrimitiveCount field of
				// D3DHAL_DP2COMMAND.  The sequence of lines 
				// rendered will be (wV[0], wV[1]), (wV[2], wV[3]),
				// (wVStart[(wPrimitiveCount-1)*2], wVStart[wPrimitiveCount*2-1]).
				// The indexes are relative to a base index value that 
				// immediately follows the command
				base = ((D3DHAL_DP2STARTVERTEX*)prim)->wVStart;
				prim += sizeof(D3DHAL_DP2STARTVERTEX);
				entry->proc.pglBegin(GL_LINES);
				for(i = 0; i < inst->wPrimitiveCount; i++)
				{
					start = base + ((D3DHAL_DP2INDEXEDLINELIST*)prim)->wV1;
					MesaDrawFVFIndex(ctx, vertices, start);

					start = base + ((D3DHAL_DP2INDEXEDLINELIST*)prim)->wV2;
					MesaDrawFVFIndex(ctx, vertices, start);

					prim += sizeof(D3DHAL_DP2INDEXEDLINELIST);
				}
				entry->proc.pglEnd();
				SET_DIRTY;
				NEXT_INST(0);
				break;
			COMMAND(D3DDP2OP_TRIANGLEFAN_IMM)
				// Draw a triangle fan specified by pairs of vertices 
				// that immediately follow this instruction in the
				// command stream. The wPrimitiveCount member of the
				// D3DHAL_DP2COMMAND structure specifies the number
				// of triangles that follow. The type and size of the
				// vertices are determined by the dwVertexType member
				// of the D3DHAL_DRAWPRIMITIVES2DATA structure.
				count = (DWORD)inst->wPrimitiveCount + 2;

				// Get Edge flags (we still have to process them)
				//dwEdgeFlags = ((D3DHAL_DP2TRIANGLEFAN_IMM *)prim)->dwEdgeFlags;

    		prim += sizeof(D3DHAL_DP2TRIANGLEFAN_IMM);
    		PRIM_ALIGN;

    		MesaDrawFVFs(ctx, GL_TRIANGLE_FAN, prim, 0, count);
				prim += ctx->state.fvf.stride * count;
				//PRIM_ALIGN;
				SET_DIRTY;
        NEXT_INST(0);
				break;
			COMMAND(D3DDP2OP_LINELIST_IMM)
				// Draw a set of lines specified by pairs of vertices 
				// that immediately follow this instruction in the
				// command stream. The wPrimitiveCount member of the
				// D3DHAL_DP2COMMAND structure specifies the number
				// of lines that follow. The type and size of the
				// vertices are determined by the dwVertexType member
				// of the D3DHAL_DRAWPRIMITIVES2DATA structure.
				count = (DWORD)inst->wPrimitiveCount * 2;

				// Primitives in an IMM instruction are stored in the
				// command buffer and are DWORD aligned
				PRIM_ALIGN;

				MesaDrawFVFs(ctx, GL_LINES, prim, 0, count);
				prim += ctx->state.fvf.stride * count;
				//PRIM_ALIGN;
				SET_DIRTY;
				NEXT_INST(0);
				break;
			COMMAND(D3DDP2OP_RENDERSTATE)
				// Specifies a render state change that requires processing. 
				// The rendering state to change is specified by one or more 
				// D3DHAL_DP2RENDERSTATE structures following D3DHAL_DP2COMMAND.

				for(i = 0; i < inst->wStateCount; i++)
				{
					MesaSetRenderState(ctx, (LPD3DSTATE)prim, RStates);
					prim += sizeof(D3DHAL_DP2RENDERSTATE);
				}
				
				MesaStencilApply(ctx);
				MesaDrawRefreshState(ctx);
				NEXT_INST(0);
				break;
			COMMAND(D3DDP2OP_TEXTURESTAGESTATE) // Has edge flags and called from Execute
        // Specifies texture stage state changes, having wStateCount 
        // D3DNTHAL_DP2TEXTURESTAGESTATE structures follow the command
        // buffer. For each, the driver should update its internal 
        // texture state associated with the texture at dwStage to 
        // reflect the new value based on TSState.
        
				for(i = 0; i < inst->wStateCount; i++)
				{
					D3DHAL_DP2TEXTURESTAGESTATE *state = (D3DHAL_DP2TEXTURESTAGESTATE *)(prim);
					TRACE("D3DHAL_DP2TEXTURESTAGESTATE(%d, %d, %X)", state->wStage, state->TSState, state->dwValue);
					// state->wStage  = texture unit from 0
					// state->TSState = D3DTSS_TEXTUREMAP, D3DTSS_TEXTURETRANSFORMFLAGS, ...
					// state->dwValue = (value)
					MesaSetTextureState(ctx, state->wStage, state->TSState, &state->dwValue);
					prim += sizeof(D3DHAL_DP2TEXTURESTAGESTATE);
				}
        MesaDrawRefreshState(ctx);
        NEXT_INST(0);
				break;
			COMMAND(D3DDP2OP_VIEWPORTINFO)
				// Specifies the clipping rectangle used for guard-band 
				// clipping by guard-band aware drivers. The clipping 
				// rectangle (i.e. the viewing rectangle) is specified 
				// by the D3DHAL_DP2 VIEWPORTINFO structures following 
				// D3DHAL_DP2COMMAND
				for(i = 0; i < inst->wStateCount; i++)
				{
					D3DHAL_DP2VIEWPORTINFO *viewport = (D3DHAL_DP2VIEWPORTINFO*)prim;
					TOPIC("ZVIEW", "viewport = %d %d %d %d",
						viewport->dwX,
						viewport->dwY,
						viewport->dwWidth,
						viewport->dwHeight);
						
					MesaApplyViewport(ctx, viewport->dwX, viewport->dwY, viewport->dwWidth, viewport->dwHeight);
					prim += sizeof(D3DHAL_DP2VIEWPORTINFO);
				}
				NEXT_INST(0);
				break;
			COMMAND(D3DDP2OP_WINFO)
			{
				// Specifies the w-range for W buffering. It is specified
				// by one or more D3DHAL_DP2WINFO structures following
				// D3DHAL_DP2COMMAND.
				for(i = 0; i < inst->wStateCount; i++)
				{
					D3DHAL_DP2WINFO *winfo = (D3DHAL_DP2WINFO*)prim;
					prim += sizeof(D3DHAL_DP2WINFO);
					
					TOPIC("DEPTH", "w-buffer range %f - %f", winfo->dvWNear, winfo->dvWFar);
					//GL_CHECK(entry->proc.pglDepthRange(winfo->dvWNear, winfo->dvWFar));
					GLfloat wmax = NOCRT_MAX(winfo->dvWNear, winfo->dvWFar);
					
					/* scale projection if Wmax is too large */
#if 0
					if(wmax > ctx->matrix.wmax)
					{
						GLfloat sz = GL_WRANGE_MAX/wmax;
						if(sz > 1.0)
							sz = 1.0f;

						ctx->matrix.zscale[10] = sz;
						MesaApplyTransform(ctx, MESA_TF_PROJECTION);

						ctx->matrix.wmax = wmax;
					}
#endif
				}
				NEXT_INST(0);
				break;
			}
			// two below are for pre-DX7 interface apps running DX7 driver
			COMMAND(D3DDP2OP_SETPALETTE)
				// Attach a palette to a texture, that is , map an association
				// between a palette handle and a surface handle, and specify
				// the characteristics of the palette. The number of
				// D3DNTHAL_DP2SETPALETTE structures to follow is specified by
				// the wStateCount member of the D3DNTHAL_DP2COMMAND structure
				
				// D3DHAL_DP2SETPALETTE* lpSetPal = (D3DHAL_DP2SETPALETTE*)(prim);
				
				// skipped
				NEXT_INST(sizeof(D3DHAL_DP2SETPALETTE));
				break;
			COMMAND(D3DDP2OP_UPDATEPALETTE)
				// Perform modifications to the palette that is used for palettized
				// textures. The palette handle attached to a surface is updated
				// with wNumEntries PALETTEENTRYs starting at a specific wStartIndex
				// member of the palette. (A PALETTENTRY (defined in wingdi.h and
				// wtypes.h) is actually a DWORD with an ARGB color for each byte.) 
				// After the D3DNTHAL_DP2UPDATEPALETTE structure in the command
				// stream the actual palette data will follow (without any padding),
				// comprising one DWORD per palette entry. There will only be one
				// D3DNTHAL_DP2UPDATEPALETTE structure (plus palette data) following
				// the D3DNTHAL_DP2COMMAND structure regardless of the value of
				// wStateCount.
				
				// skipped
				NEXT_INST(sizeof(D3DHAL_DP2UPDATEPALETTE));
				break;
			// New for DX7
			COMMAND(D3DDP2OP_ZRANGE)
				for(i = 0; i < inst->wStateCount; i++)
				{
					D3DHAL_DP2ZRANGE *zrange = (D3DHAL_DP2ZRANGE*)prim;
					prim += sizeof(D3DHAL_DP2ZRANGE);
					TOPIC("DEPTH", "D3DDP2OP_ZRANGE = %f %f", zrange->dvMinZ, zrange->dvMaxZ);
					// entry->glDepthRange(zrange->dvMinZ, zrange->dvMaxZ);
					// ^JH: I only sees values 0.0 and 1.0, so nothing set when
					// something sets here some junk
				}
				NEXT_INST(0);
				break;
			COMMAND(D3DDP2OP_SETMATERIAL)
				for(i = 0; i < inst->wStateCount; i++)
				{
					LPD3DHAL_DP2SETMATERIAL material = (LPD3DHAL_DP2SETMATERIAL)prim;
					prim += sizeof(D3DHAL_DP2SETMATERIAL);

					MESA_D3DCOLORVALUE_TO_FV(material->dcvDiffuse,  ctx->state.material.diffuse);
					MESA_D3DCOLORVALUE_TO_FV(material->dcvAmbient,  ctx->state.material.ambient);
					MESA_D3DCOLORVALUE_TO_FV(material->dcvEmissive, ctx->state.material.emissive);
					MESA_D3DCOLORVALUE_TO_FV(material->dcvSpecular, ctx->state.material.specular);

					if(material->dvPower < 0)
						ctx->state.material.shininess = 0;
					else if(material->dvPower > 128)
						ctx->state.material.shininess = 128;
					else
						ctx->state.material.shininess = material->dvPower;
					
					MesaApplyMaterial(ctx);
				}
				NEXT_INST(0);
				break;
			COMMAND(D3DDP2OP_SETLIGHT)
				for(i = 0; i < inst->wStateCount; i++)
				{
					LPD3DHAL_DP2SETLIGHT lightset = (LPD3DHAL_DP2SETLIGHT)prim;
					prim += sizeof(D3DHAL_DP2SETLIGHT);

					TOPIC("LIGHT", "light->dwDataType=%d, light->dwIndex=%d", lightset->dwDataType, lightset->dwIndex);

					switch(lightset->dwDataType)
					{
						case D3DHAL_SETLIGHT_ENABLE:
							LightActive(ctx, lightset->dwIndex, TRUE);
							break;
						case D3DHAL_SETLIGHT_DISABLE:
							LightActive(ctx, lightset->dwIndex, FALSE);
							break;
						case D3DHAL_SETLIGHT_DATA:
						{
							D3DLIGHT7 *light_data = (D3DLIGHT7*)prim;
							prim += sizeof(D3DLIGHT7);
							TOPIC("LIGHT", "LIGHT DATA %d, index=%d, ptr=%X",
								light_data->dltType, lightset->dwIndex, light_data);

							MesaSpaceModelviewSet(ctx);
							LightData(ctx, lightset->dwIndex, light_data);
							MesaSpaceModelviewReset(ctx);
						}
					} // switch(light->dwDataType)
				} // for
				NEXT_INST(0);
				break;
			COMMAND(D3DDP2OP_CREATELIGHT)
				for(i = 0; i < inst->wStateCount; i++)
				{
					D3DHAL_DP2CREATELIGHT *lightcreate = (D3DHAL_DP2CREATELIGHT*)prim;
					prim += sizeof(D3DHAL_DP2CREATELIGHT);
					
					LightCreate(ctx, lightcreate->dwIndex);
				}
				NEXT_INST(0);
				break;
			COMMAND(D3DDP2OP_SETTRANSFORM)
				for(i = 0; i < inst->wStateCount; i++)
				{
					LPD3DHAL_DP2SETTRANSFORM stf = (LPD3DHAL_DP2SETTRANSFORM)prim;
					MesaSetTransform(ctx, stf->xfrmType, &stf->matrix);
					prim += sizeof(D3DHAL_DP2SETTRANSFORM);
				}
				NEXT_INST(0);
				break;
			COMMAND(D3DDP2OP_EXT)
				for(i = 0; i < inst->wStateCount; i++)
				{
					prim += sizeof(D3DHAL_DP2EXT);
					WARN("D3DDP2OP_EXT");
				}
				NEXT_INST(0);
				break;
			COMMAND(D3DDP2OP_TEXBLT)
				// Inform the drivers to perform a BitBlt operation from a source
				// texture to a destination texture. A texture can also be cubic
				// environment map. The driver should copy a rectangle specified
				// by rSrc in the source texture to the location specified by pDest
				// in the destination texture. The destination and source textures
				// are identified by handles that the driver was notified with
				// during texture creation time. If the driver is capable of
				// managing textures, then it is possible that the destination
				// handle is 0. This indicates to the driver that it should preload
				// the texture into video memory (or wherever the hardware
				// efficiently textures from). In this case, it can ignore rSrc and
				// pDest. Note that for mipmapped textures, only one D3DDP2OP_TEXBLT
				// instruction is inserted into the D3dDrawPrimitives2 command stream.
				// In this case, the driver is expected to BitBlt all the mipmap
				// levels present in the texture.
				
				for(i = 0; inst->wStateCount; i++)
				{
					// FIXME: do the blit...
					WARN("D3DDP2OP_TEXBLT");
					prim += sizeof(D3DHAL_DP2TEXBLT);
				}
				NEXT_INST(0);
				break;
			COMMAND(D3DDP2OP_STATESET)
				for(i = 0; inst->wStateCount; i++)
				{
					LPD3DHAL_DP2STATESET pStateSetOp = (LPD3DHAL_DP2STATESET)(prim);
					switch(pStateSetOp->dwOperation)
					{
						case D3DHAL_STATESETBEGIN:
						case D3DHAL_STATESETEND:
						case D3DHAL_STATESETDELETE:
						case D3DHAL_STATESETEXECUTE:
						case D3DHAL_STATESETCAPTURE:
						default:
							WARN("D3DDP2OP_STATESET: %d", pStateSetOp->dwOperation);
							break;
					}
					prim += sizeof(D3DHAL_DP2STATESET);
				}
				NEXT_INST(0);
				break;
			COMMAND(D3DDP2OP_SETPRIORITY)
				for(i = 0; i < inst->wStateCount; i++)
				{
					prim += sizeof(D3DHAL_DP2SETPRIORITY);
				}
				NEXT_INST(0);
				break;
			COMMAND(D3DDP2OP_SETRENDERTARGET)
				// Map a new rendering target surface and depth buffer in
				// the current context.  This replaces the old D3dSetRenderTarget
				// callback.
				for(i = 0; i < inst->wStateCount; i++)
				{
					if(entry->runtime_ver >= 7)
					{
						D3DHAL_DP2SETRENDERTARGET *pSRTData = (D3DHAL_DP2SETRENDERTARGET*)prim;
						
						TOPIC("TARGET", "hRenderTarget=%d, hZBuffer=%d", pSRTData->hRenderTarget, pSRTData->hZBuffer);
						
						surface_id dds_sid = ctx->surfaces->table[pSRTData->hRenderTarget];
						surface_id ddz_sid = ctx->surfaces->table[pSRTData->hZBuffer];
	
						if(dds_sid)
						{
							MesaSetTarget(ctx, dds_sid, ddz_sid, FALSE);
						}
						else
						{
							WARN("render target (%d) is NULL", pSRTData->hRenderTarget);
						}
					}
					prim += sizeof(D3DHAL_DP2SETRENDERTARGET);
				}
				NEXT_INST(0);
				break;
			COMMAND(D3DDP2OP_CLEAR)
				// Perform hardware-assisted clearing on the rendering target,
				// depth buffer or stencil buffer. This replaces the old D3dClear
				// and D3dClear2 callbacks. 
				{
					D3DHAL_DP2CLEAR *pClear = (D3DHAL_DP2CLEAR*)prim;
					prim += sizeof(D3DHAL_DP2CLEAR) - sizeof(RECT);
					MesaClear(ctx, pClear->dwFlags, pClear->dwFillColor, pClear->dvFillDepth, pClear->dwFillStencil,
						inst->wStateCount,
						(RECT*)prim
					);
					
					prim += sizeof(RECT) * inst->wStateCount;
				}
				NEXT_INST(0);
				break;
			COMMAND(D3DDP2OP_SETTEXLOD)
				for(i = 0; i < inst->wStateCount; i++)
				{
					prim += sizeof(D3DHAL_DP2SETTEXLOD);
				}
				WARN("D3DDP2OP_SETTEXLOD");
				NEXT_INST(0);
				break;
			COMMAND(D3DDP2OP_SETCLIPPLANE)
				for(i = 0; i < inst->wStateCount; i++)
				{
					D3DHAL_DP2SETCLIPPLANE *plane = (D3DHAL_DP2SETCLIPPLANE*)prim;
					prim += sizeof(D3DHAL_DP2SETCLIPPLANE);

					if(plane->dwIndex < MESA_CLIPS_MAX)
					{
						ctx->state.clipping.plane[plane->dwIndex][0] = plane->plane[0];
						ctx->state.clipping.plane[plane->dwIndex][1] = plane->plane[1];
						ctx->state.clipping.plane[plane->dwIndex][2] = plane->plane[2];
						ctx->state.clipping.plane[plane->dwIndex][3] = plane->plane[3];
					}

					MesaSpaceModelviewSet(ctx);
					PlaneApply(ctx, plane->dwIndex);
					MesaSpaceModelviewReset(ctx);
				}
				NEXT_INST(0);
				break;
			// COMMAND(D3DDP2OP_RESERVED0)
			// Used by the front-end only
			default:
				WARN("Unknown command: 0x%X", inst->bCommand);
				
				if(!entry->D3DParseUnknownCommand)
				{
					*error_offset = (LPBYTE)inst - (LPBYTE)cmdBufferStart;
					return FALSE;
				}
				
				{
					void *resume_inst = NULL;
					PFND3DPARSEUNKNOWNCOMMAND fn = (PFND3DPARSEUNKNOWNCOMMAND)entry->D3DParseUnknownCommand;
					fn((LPVOID)inst, &resume_inst);
					if(resume_inst == NULL)
					{
						*error_offset = (LPBYTE)inst - (LPBYTE)cmdBufferStart;
						return FALSE;
					}
					inst = resume_inst;
				}
				break;
		} // switch
	} // while
		
	return TRUE;
}

