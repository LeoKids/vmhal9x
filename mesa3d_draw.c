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
#define MESA3D_DRAW_C

#include <windows.h>
#include <stddef.h>
#include <stdint.h>
#include <ddraw.h>
#include <ddrawi.h>
#include <stddef.h>
#include <stdint.h>
#include "vmdahal32.h"
#include "vmhal9x.h"
#include "mesa3d.h"
#include "osmesa.h"

#include "nocrt.h"

#define SV_UNPROJECT(_v, _x, _y, _z, _w) \
	_v[3] = 2.0f/(_w); \
	_v[0] = ((((_x + 0.5) - ctx->matrix.vpnorm[0]) * ctx->matrix.vpnorm[2]) - 1.0f)*_v[3]; \
	_v[1] = ((((_y - 0.5) - ctx->matrix.vpnorm[1]) * ctx->matrix.vpnorm[3]) - 1.0f)*_v[3]; \
	_v[2] = (_z)*_v[3]

#define SV_UNPROJECT_NTP(_v, _x, _y, _z) \
	_v[3] = 1.0f; \
	_v[0] = ((((_x + 0.5) - ctx->matrix.vpnorm[0]) * ctx->matrix.vpnorm[2]) - 1.0f); \
	_v[1] = ((((_y - 0.5) - ctx->matrix.vpnorm[1]) * ctx->matrix.vpnorm[3]) - 1.0f); \
	_v[2] = (_z)

#define SV_UNPROJECTD(_v, _x, _y, _z, _w) \
	_v[3] = 2.0/((double)_w); \
	_v[0] = (((((double)_x) - ctx->matrix.vpnorm[0]) * ctx->matrix.vpnorm[2]) - 1.0)*_v[3]; \
	_v[1] = (((((double)_y) - ctx->matrix.vpnorm[1]) * ctx->matrix.vpnorm[3]) - 1.0)*_v[3]; \
	_v[2] = ((double)_z)*_v[3]

#define FVF_X 0
#define FVF_Y 1
#define FVF_Z 2
#define FVF_BETA1 3
#define FVF_RHW 3

#define FVF_MAX 32

typedef struct _FVF
{
	union
	{
		D3DVALUE fv[32];
		DWORD    dw[32];
	};
} FVF_t;

static void LoadColor1(mesa3d_entry_t *entry, mesa3d_ctx_t *ctx, DWORD color)
{
	GLfloat cv[4];
	MESA_D3DCOLOR_TO_FV(color, cv);
	entry->proc.pglColor4fv(&cv[0]);
	
	if(ctx->state.material.lighting)
	{
		if(ctx->state.material.untracked & (MESA_MAT_DIFFUSE_C1 | MESA_MAT_AMBIENT_C1))
		{
			entry->proc.pglMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, &cv[0]);
		}
		else if(ctx->state.material.untracked & MESA_MAT_DIFFUSE_C1)
		{
			entry->proc.pglMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, &cv[0]);
		}
		else if(ctx->state.material.untracked & MESA_MAT_AMBIENT_C1)
		{
			entry->proc.pglMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, &cv[0]);
		}
		
		if(ctx->state.material.untracked & MESA_MAT_EMISSIVE_C1)
		{
			entry->proc.pglMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, &cv[0]);
		}

		if(ctx->state.material.untracked & MESA_MAT_SPECULAR_C1)
		{
			entry->proc.pglMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, &cv[0]);
		}
	}
}

static void LoadColor2(mesa3d_entry_t *entry, mesa3d_ctx_t *ctx, DWORD color)
{
	GLfloat cv[4];
	MESA_D3DCOLOR_TO_FV(color, cv);

	if(ctx->state.material.lighting)
	{
		if(ctx->state.material.untracked & (MESA_MAT_DIFFUSE_C2 | MESA_MAT_AMBIENT_C2))
		{
			entry->proc.pglMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, &cv[0]);
		}
		else if(ctx->state.material.untracked & MESA_MAT_DIFFUSE_C2)
		{
			entry->proc.pglMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, &cv[0]);
		}
		else if(ctx->state.material.untracked & MESA_MAT_AMBIENT_C2)
		{
			entry->proc.pglMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, &cv[0]);
		}
		
		if(ctx->state.material.untracked & MESA_MAT_EMISSIVE_C2)
		{
			entry->proc.pglMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, &cv[0]);
		}

		if(ctx->state.specular_vertex)
		{
			entry->proc.pglSecondaryColor3fv(&cv[0]);
		}
	}
}


void MesaDrawTLVertex(mesa3d_ctx_t *ctx, LPD3DTLVERTEX vertex)
{
	mesa3d_entry_t *entry = ctx->entry;
	
	if(ctx->state.tmu[0].image)
	{
		entry->proc.pglTexCoord2f(CONV_U_TO_S(vertex->tu), CONV_V_TO_T(vertex->tv));
		TOPIC("TEX", "glTexCoord2f(%f, %f)", vertex->tu, vertex->tv);
	}

	entry->proc.pglColor4ub(
		RGBA_GETRED(vertex->color),
		RGBA_GETGREEN(vertex->color),
		RGBA_GETBLUE(vertex->color),
		RGBA_GETALPHA(vertex->color)
	);

	if(ctx->state.specular_vertex)
	{
		entry->proc.pglSecondaryColor3ub(
			RGBA_GETRED(vertex->specular),
			RGBA_GETGREEN(vertex->specular),
			RGBA_GETBLUE(vertex->specular)
		);
	}

#if 0
	GLfloat x, y, z, w = 2.0;
	MesaUnproject(ctx, vertex->sx, vertex->sy, vertex->sz, &x, &y, &z);
	// w = (1.0/vertex->rhw) - 0.5; = currently best
	if(vertex->rhw != 0)
	{
		w = 2.0/vertex->rhw;
	}
	
	entry->proc.pglVertex4f(x*w, y*w, z*w, w);
	TOPIC("TEX", "glVertex4f(%f, %f, %f, %f)", x, y, z, w);	
#else
	GLfloat v[4];
	SV_UNPROJECT(v, vertex->sx, vertex->sy, vertex->sz, vertex->rhw);
	entry->proc.pglVertex4fv(&v[0]);
#endif

}

void MesaDrawLVertex(mesa3d_ctx_t *ctx, LPD3DLVERTEX vertex)
{
	mesa3d_entry_t *entry = ctx->entry;
	
	if(ctx->state.tmu[0].image)
	{
		entry->proc.pglTexCoord2f(CONV_U_TO_S(vertex->tu), CONV_V_TO_T(vertex->tv));
	}

	entry->proc.pglColor4ub(
		RGBA_GETRED(vertex->color),
		RGBA_GETGREEN(vertex->color),
		RGBA_GETBLUE(vertex->color),
		RGBA_GETALPHA(vertex->color)
	);
	
	if(ctx->state.specular_vertex)
	{
		entry->proc.pglSecondaryColor3ub(
			RGBA_GETRED(vertex->specular),
			RGBA_GETGREEN(vertex->specular),
			RGBA_GETBLUE(vertex->specular)
		);
	}
	
	entry->proc.pglVertex3f(vertex->x, vertex->y, vertex->z);
}

void MesaDrawVertex(mesa3d_ctx_t *ctx, LPD3DVERTEX vertex)
{
	mesa3d_entry_t *entry = ctx->entry;
	
	if(ctx->state.tmu[0].image)
	{
		entry->proc.pglTexCoord2f(CONV_U_TO_S(vertex->tu), CONV_V_TO_T(vertex->tv));
	}
					
	entry->proc.pglVertex3f(vertex->x, vertex->y, vertex->z);
}

void MesaFVFSet(mesa3d_ctx_t *ctx, DWORD type, DWORD size)
{
	if(ctx->state.fvf.type == type)
	{
		/* not need to recalculate */
		return;
	}
	
	ctx->state.fvf.type = type;
	MesaFVFRecalc(ctx);
	
	TOPIC("MATRIX", "MesaFVFSet type=0x%X, size=%d, realsize=%d",
		type, size, ctx->state.fvf.stride
	);
}
	
void MesaFVFRecalc(mesa3d_ctx_t *ctx)
{
	int offset = 0; // in DW
	int i;
	
	DWORD type = ctx->state.fvf.type;
	
	switch(type & D3DFVF_POSITION_MASK)
	{
		case D3DFVF_XYZ:
			offset = 3; // sizeof(D3DVECTOR) == 12
			ctx->state.fvf.betas = 0;
			break;
		case D3DFVF_XYZRHW:
			offset = 4;
			ctx->state.fvf.betas = 0;
			break;
		case D3DFVF_XYZB1:
			offset = 3 + 1;
			ctx->state.fvf.betas = 1;
			break;
		case D3DFVF_XYZB2:
			offset = 3 + 2;
			ctx->state.fvf.betas = 2;
			break;
		case D3DFVF_XYZB3:
			offset = 3 + 3;
			ctx->state.fvf.betas = 3;
			break;
		case D3DFVF_XYZB4:
			offset = 3 + 4;
			ctx->state.fvf.betas = 4;
			break;
		case D3DFVF_XYZB5:
			offset = 3 + 5;
			ctx->state.fvf.betas = 5;
			break;
	}
	
	if(type & D3DFVF_NORMAL)
	{
		ctx->state.fvf.pos_normal = offset;
		offset += 3;
	}
	else
	{
		ctx->state.fvf.pos_normal = 0;
	}
	
	if(type & D3DFVF_RESERVED1)
	{
		offset++;
	}
	
	if(type & D3DFVF_DIFFUSE)
	{
		ctx->state.fvf.pos_diffuse = offset++;
	}
	else
	{
		ctx->state.fvf.pos_diffuse = 0;
	}
	
	if(type & D3DFVF_SPECULAR)
	{
		ctx->state.fvf.pos_specular = offset++;
	}
	else
	{
		ctx->state.fvf.pos_specular = 0;
	}
	
	int tc = (type & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
	for(i = 0; i < ctx->tmu_count; i++)
	{
		if(tc > i)
		{
			ctx->state.fvf.pos_tmu[i] = offset;
			if((type & D3DFVF_POSITION_MASK) == D3DFVF_XYZRHW)
			{
				offset += 2;
			}
			else
			{
				offset += ctx->state.tmu[i].coordnum;
			}
		}
		else
		{
			ctx->state.fvf.pos_tmu[i] = 0;
		}
	}

	ctx->state.fvf.stride = offset * sizeof(D3DVALUE);
}

static const GLfloat normal_one[3] = {1.0, 1.0, 1.0};

inline static void MesaDrawFVF_internal(mesa3d_entry_t *entry, mesa3d_ctx_t *ctx, FVF_t *vertex)
{
	int i;
	
	for(i = 0; i < ctx->tmu_count; i++)
	{
		if(ctx->state.tmu[i].image)
		{
			int coordindex = ctx->state.tmu[i].coordindex;
			int coordnum = ctx->state.tmu[i].coordnum;
			if(ctx->state.fvf.pos_tmu[coordindex] && coordnum > 0)
			{
				switch(coordnum)
				{
					// TODO: use glMultiTexCoord2fv
					case 1:
						entry->proc.pglMultiTexCoord1f(GL_TEXTURE0 + i,
							vertex->fv[ctx->state.fvf.pos_tmu[coordindex] + 0]);
						break;
					case 2:
						entry->proc.pglMultiTexCoord2f(GL_TEXTURE0 + i,
							vertex->fv[ctx->state.fvf.pos_tmu[coordindex] + 0],
							vertex->fv[ctx->state.fvf.pos_tmu[coordindex] + 1]);
						break;
					case 3:
						entry->proc.pglMultiTexCoord3f(GL_TEXTURE0 + i,
							vertex->fv[ctx->state.fvf.pos_tmu[coordindex] + 0],
							vertex->fv[ctx->state.fvf.pos_tmu[coordindex] + 1],
							vertex->fv[ctx->state.fvf.pos_tmu[coordindex] + 2]);
						break;
					case 4:
						entry->proc.pglMultiTexCoord4f(GL_TEXTURE0 + i,
							vertex->fv[ctx->state.fvf.pos_tmu[coordindex] + 0],
							vertex->fv[ctx->state.fvf.pos_tmu[coordindex] + 1],
							vertex->fv[ctx->state.fvf.pos_tmu[coordindex] + 2],
							vertex->fv[ctx->state.fvf.pos_tmu[coordindex] + 3]);
						break;
				} // switch(coordnum)
			}
		}
	}

	if(ctx->state.fvf.pos_diffuse)
	{
		LoadColor1(entry, ctx, vertex->dw[ctx->state.fvf.pos_diffuse]);
	}

	if(ctx->state.fvf.pos_specular)
	{
		LoadColor2(entry, ctx, vertex->dw[ctx->state.fvf.pos_specular]);
	}
	else
	{
		// FIXME: optimize for load black
		LoadColor2(entry, ctx, 0x00000000);
	}

	if((ctx->state.fvf.type & D3DFVF_POSITION_MASK) == D3DFVF_XYZRHW)
	{
		GLfloat v[4];
		if(ctx->state.texperspective)
		{
			SV_UNPROJECT(v, vertex->fv[FVF_X], vertex->fv[FVF_Y], vertex->fv[FVF_Z], vertex->fv[FVF_RHW]);
		}
		else
		{
			SV_UNPROJECT_NTP(v, vertex->fv[FVF_X], vertex->fv[FVF_Y], vertex->fv[FVF_Z]);
		}
		entry->proc.pglVertex4fv(&v[0]);
	}
	else
	{
		if(ctx->state.fvf.pos_normal && ctx->state.texperspective)
		{
			entry->proc.pglNormal3fv(&vertex->fv[ctx->state.fvf.pos_normal]);
		}
		else
		{
			entry->proc.pglNormal3fv(&normal_one[0]);
		}

		if(ctx->matrix.weight == 0)
		{
			entry->proc.pglVertex3fv(&vertex->fv[FVF_X]);
		}
		else
		{
			GLfloat coords[4];
			MesaVetexBlend(ctx, &vertex->fv[FVF_X], &vertex->fv[FVF_BETA1], 
				ctx->state.fvf.betas, coords);
			entry->proc.pglVertex3fv(&coords[0]);
		}
	}
}

void MesaDrawFVF(mesa3d_ctx_t *ctx, void *vertex)
{
	MesaDrawFVF_internal(ctx->entry, ctx, vertex);
}

void MesaDrawFVFIndex(mesa3d_ctx_t *ctx, void *vertices, int index)
{
	FVF_t *fvf = (FVF_t *)(((BYTE *)vertices) + (index * ctx->state.fvf.stride));
	MesaDrawFVF_internal(ctx->entry, ctx, fvf);
}

void MesaDrawFVFs(mesa3d_ctx_t *ctx, GLenum gl_ptype, void *vertices, DWORD start, DWORD cnt)
{
	mesa3d_entry_t *entry = ctx->entry;
	BYTE *vb = ((BYTE *)vertices) + (start * ctx->state.fvf.stride);
	
	if(gl_ptype != -1)
	{
		TOPIC("GL", "glBegin(%d)", gl_ptype);
		entry->proc.pglBegin(gl_ptype);
		while(cnt--)
		{
			MesaDrawFVF_internal(entry, ctx, (FVF_t *)vb);
			vb += ctx->state.fvf.stride;
		}
		GL_CHECK(entry->proc.pglEnd());
	}
}
