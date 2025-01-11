#ifndef __MESA3D_H__INCLUDED__
#define __MESA3D_H__INCLUDED__

#include <GL/gl.h>
#include <GL/glext.h>
#include <d3dtypes.h>
#include "surface.h"

typedef void (*OSMESAproc)();
typedef void *OSMesaContext;

struct mesa3d_texture;
struct mesa3d_ctx;
struct mesa3d_entry;

typedef OSMESAproc (APIENTRYP OSMesaGetProcAddress_h)(const char *funcName);

#define MESA_API(_n, _t, _p) \
	typedef _t (APIENTRYP _n ## _h)_p;
#define MESA_API_OS  MESA_API
#define MESA_API_DRV MESA_API

#include "mesa3d_api.h"
#undef MESA_API
#undef MESA_API_OS
#undef MESA_API_DRV

#define MESA3D_MAX_TEXS 65536
#define MESA3D_MAX_CTXS 128
#define MESA3D_MAX_MIPS 16

#define MESA_TMU_MAX 8
#define MESA_CLIPS_MAX 8
#define MESA_WORLDS_MAX 4

#define MESA_MAT_DIFFUSE_C1  0x01
#define MESA_MAT_AMBIENT_C1  0x02
#define MESA_MAT_EMISSIVE_C1 0x04
#define MESA_MAT_SPECULAR_C1 0x08

#define MESA_MAT_DIFFUSE_C2  0x10
#define MESA_MAT_AMBIENT_C2  0x20
#define MESA_MAT_EMISSIVE_C2 0x40
#define MESA_MAT_SPECULAR_C2 0x80

typedef struct mesa3d_texture
{
	BOOL    alloc;
	GLuint  gltex;
	GLuint  width;
	GLuint  height;
	GLuint  bpp;
	GLint   internalformat;
	GLenum  format;
	GLenum  type;
	FLATPTR data_ptr[MESA3D_MAX_MIPS];
	BOOL    data_dirty[MESA3D_MAX_MIPS];
	LPDDRAWI_DDRAWSURFACE_LCL data_surf[MESA3D_MAX_MIPS];
	BOOL dirty;
	struct mesa3d_ctx *ctx;
	BOOL mipmap;
	int mipmap_level;
	BOOL colorkey;
	BOOL compressed;
	BOOL tmu[MESA_TMU_MAX];
} mesa3d_texture_t;

typedef struct mesa3d_light
{
	BOOL active;
	DWORD active_index;
	D3DLIGHTTYPE type;
	GLfloat diffuse[4];
	GLfloat specular[4];
	GLfloat ambient[4];
	GLfloat quad_att;
	GLfloat pos[4];
	GLfloat dir[4];
	GLfloat attenuation[4]; // attenuation[3]: GL_QUADRATIC_ATTENUATION
	GLfloat range;
	GLfloat falloff;
	GLfloat theta;
	GLfloat phi;
	GLfloat exponent;
} mesa3d_light_t;

struct mesa3d_tmustate
{
	BOOL active;
	
	mesa3d_texture_t *image;
	// texture address DX5 DX6 DX7
	D3DTEXTUREADDRESS texaddr_u; // wrap, mirror, clamp, border
	D3DTEXTUREADDRESS texaddr_v;

	// blend DX5
	D3DTEXTUREBLEND   texblend;
	
	// blend DX6 DX7
	BOOL dx6_blend; // latch for DX6 and DX7 interface
	DWORD color_op; // D3DTEXTUREOP
	DWORD color_arg1; // D3DTA_*
	DWORD color_arg2;
	DWORD alpha_op; // D3DTEXTUREOP
	DWORD alpha_arg1;
	DWORD alpha_arg2;
	
	// filter DX5
	GLenum texmin;
	GLenum texmag;
	
	// filter DX6 DX7
	BOOL dx6_filter;
	D3DTEXTUREMAGFILTER dx_mag;
	D3DTEXTUREMINFILTER dx_min;
	D3DTEXTUREMIPFILTER dx_mip;
	DWORD anisotropy;

	// mipmap
	GLfloat miplodbias;
	GLint mipmaxlevel;
	
	GLfloat border[4];
	BOOL colorkey;
	int coordindex; /* D3DTSS_TEXCOORDINDEX */
	int coordnum; /* D3DTSS_TEXTURETRANSFORMFLAGS */
	BOOL projected;
	GLfloat matrix[16];
	
	DWORD wrap;
	
	BOOL reload; // reload texture data (surface -> GPU)
	BOOL update; // update texture params
	BOOL move;   // reload texture matrix
};

/* DX7 only: relation between surfaces and dwSurfaceHandle */
typedef struct mesa_surfaces_table
{
	LPDDRAWI_DIRECTDRAW_LCL lpDDLcl;
	DDSURF **table;
	DWORD table_size;
} mesa_surfaces_table_t;

#define SURFACE_TABLES_PER_ENTRY 8 /* in theory there should by only 1 */

typedef struct mesa3d_ctx
{
	LONG thread_lock;
	mesa3d_texture_t tex[MESA3D_MAX_TEXS];
	struct mesa3d_entry *entry;
	int id; /* mesa3d_entry.ctx[_id_] */
	
	/* offscreen context */
	OSMesaContext *osctx;
	void *osbuf;
	DWORD ossize;
	
	/* device context */
	HDC dc;
	HGLRC glrc;
	HWND fbo_win;
	
	DWORD thread_id;
	GLint front_bpp;
	GLint depth_bpp;
	//DDRAWI_DDRAWSURFACE_LCL flips[MESA3D_MAX_FLIPS];
	//int flips_cnt;
	DDSURF backbuffer;
	DDSURF depth;
	LPDDRAWI_DIRECTDRAW_GBL dd;
	BOOL depth_stencil;	
	int tmu_count;

	/* render state */
	struct {
		// screen coordinates
		GLint sw;
		GLint sh;
		BOOL zvisible; // Z-visibility testing
		BOOL specular; // specular enable (DX state)
		BOOL specular_vertex; // use glSecondaryColor
		BOOL blend;
		GLenum blend_sfactor;
		GLenum blend_dfactor;
		DWORD overrides[4]; // 128 bit set
		DWORD stipple[32];
		GLfloat tfactor[4]; // TEXTUREFACTOR eq. GL_CONSTANT
		BOOL texperspective;
		struct {
			BOOL     enabled;
			GLenum   func;
			GLclampf ref;
		} alpha;
		struct {
			BOOL enabled;
			GLenum  tmode;
			GLenum  vmode;
			GLfloat start;
			GLfloat end;
			GLfloat density;
			GLfloat color[4];
			BOOL range;
		} fog;
		struct {
			DWORD stride;
			DWORD type;
			int pos_normal;
			int pos_diffuse;
			int pos_specular;
			int pos_tmu[MESA_TMU_MAX];
			int betas;
		} fvf;
		struct {
			BOOL enabled;
			GLenum sfail;
			GLenum dpfail;
			GLenum dppass;
			GLenum func;
			GLint ref;
			GLuint mask;
			GLuint writemask;
		} stencil;
		struct {
			BOOL enabled;
			BOOL writable;
			BOOL wbuffer;
		} depth;
		struct {
			D3DMATERIALCOLORSOURCE diffuse_source;
			D3DMATERIALCOLORSOURCE specular_source;
			D3DMATERIALCOLORSOURCE ambient_source;
			D3DMATERIALCOLORSOURCE emissive_source;
			BOOL color_vertex;
			GLfloat diffuse[4];
			GLfloat ambient[4];
			GLfloat specular[4];
			GLfloat emissive[4];
			GLfloat shininess;
			DWORD untracked;
			BOOL lighting;
		} material;
		struct {
			BOOL enabled;
			BOOL activeplane[MESA_CLIPS_MAX];
			GLdouble plane[MESA_CLIPS_MAX][4];
		} clipping;
		struct mesa3d_tmustate tmu[MESA_TMU_MAX];
	} state;

	/* fbo */
	struct {
		/* dimensions */
		GLuint width;
		GLuint height;
		/* main plain */
		GLuint plane_fb;
		GLuint plane_color_tex;
		GLuint plane_depth_tex;
		/* conversion and blit */
		GLuint color_tex;
		GLuint color_fb;
		GLenum color_format;
		GLuint depth_tex;
		GLuint depth_fb;
		GLuint stencil_tex;
		GLuint stencil_fb;
		GLenum depth_type;
		int tmu; /* can be higher than tmu_count, if using extra TMU for FBO operations */
	} fbo;

	/* rendering state */
	struct {
		BOOL dirty;
		BOOL zdirty;
		DWORD planemask;
		DWORD rop2;
	} render;

	/* dimensions */
	struct {
		/* space transform */
		GLfloat wmax;
		GLfloat zscale[16];
		/* DX matices */
		GLfloat proj[16];
		GLfloat view[16];
		GLfloat world[MESA_WORLDS_MAX][16];
		GLint viewport[4];
		/* precalculated values */
		GLfloat projfix[16];
		GLfloat vpnorm[4]; /* viewport for faster unproject */
		BOOL identity_mode;
		DWORD outdated_stack; // MesaApplyTransform -> changes
		int weight;
	} matrix;
	
	struct {
		DWORD lights_size;
		DWORD active_bitfield;
		mesa3d_light_t **lights;
	} light;

	mesa_surfaces_table_t *surfaces;
} mesa3d_ctx_t;

/* maximum for 24bit signed zbuff = (1<<23) - 1 */
#define GL_WRANGE_MAX 8388607

#define CONV_U_TO_S(_u) (_u)
#define CONV_V_TO_T(_v) (_v)

#define MESA_API(_n, _t, _p) _n ## _h p ## _n;
#define MESA_API_OS MESA_API
#define MESA_API_DRV MESA_API
typedef struct mesa3d_entry
{
	DWORD pid;
	struct mesa3d_entry *next;
	HANDLE lib;
	BOOL os; // offscreen rendering
	BOOL runtime_ver; // 3, 5, 6, 7
	mesa3d_ctx_t *ctx[MESA3D_MAX_CTXS];
	mesa_surfaces_table_t surfaces_tables[SURFACE_TABLES_PER_ENTRY];
	OSMesaGetProcAddress_h GetProcAddress;
	struct {
#include "mesa3d_api.h"
	} proc;
	DWORD D3DParseUnknownCommand;
} mesa3d_entry_t;
#undef MESA_API
#undef MESA_API_OS
#undef MESA_API_DRV

mesa3d_entry_t *Mesa3DGet(DWORD pid, BOOL create);
void Mesa3DFree(DWORD pid);

#define GL_BLOCK_BEGIN(_ctx_h) \
	do{ \
		mesa3d_ctx_t *ctx = MESA_HANDLE_TO_CTX(_ctx_h); \
		mesa3d_entry_t *entry = ctx->entry; \
		OSMesaContext oldos = NULL; \
		HGLRC oldgrc = NULL; \
		MesaBlockLock(ctx); \
		do { \
			if(ctx->thread_id != GetCurrentThreadId()){ \
				TOPIC("GL", "Thread switch %s:%d", __FILE__, __LINE__); \
				if(!MesaSetCtx(ctx)){break;} \
			}else{ \
			if(entry->os){ \
				oldos = entry->proc.pOSMesaGetCurrentContext(); \
				if(oldos != ctx->osctx){ \
					WARN("Wrong context in %s:%d", __FILE__, __LINE__); \
					if(!MesaSetCtx(ctx)){break;}}\
			}else{ \
				oldgrc = entry->proc.pDrvGetCurrentContext(); \
				if(oldgrc != ctx->glrc){ \
					WARN("Wrong context in %s:%d", __FILE__, __LINE__); \
					if(!MesaSetCtx(ctx)){break;}}\
			} }

#define GL_BLOCK_END \
			if(entry->os){ \
				if(oldos != ctx->osctx && oldos != NULL){ \
					entry->proc.pOSMesaMakeCurrent(NULL, NULL, 0, 0, 0);} \
			}else{ \
				if(oldgrc != ctx->glrc && oldgrc != NULL){ \
					entry->proc.pDrvSetContext(NULL, NULL, NULL);}} \
		}while(0); \
		MesaBlockUnlock(ctx); \
	} while(0);

#define MESA_TEX_TO_HANDLE(_p) ((DWORD)(_p))
#define MESA_HANDLE_TO_TEX(_h) ((mesa3d_texture_t*)(_h))

#define MESA_CTX_TO_HANDLE(_p) ((DWORD)(_p))
#define MESA_HANDLE_TO_CTX(_h) ((mesa3d_ctx_t*)(_h))

#define MESA_MTX_TO_HANDLE(_p) ((DWORD)(_p))
#define MESA_HANDLE_TO_MTX(_h) ((GLfloat*)(_h))

#ifdef TRACE_ON
# ifndef DEBUG_GL_TOPIC
#  define GL_ERR_TOPIC "GL"
# else
#  define GL_ERR_TOPIC VMHAL_DSTR(DEBUG_GL_TOPIC)
# endif

#define GL_CHECK(_code) _code; \
	do{ GLenum err; \
		while((err = entry->proc.pglGetError()) != GL_NO_ERROR){ \
			TOPIC(GL_ERR_TOPIC, "GL error: %s = %X", #_code, err); \
			ERR("%s = %X", #_code, err); \
	}}while(0)
#else
#define GL_CHECK(_code) _code
#endif

mesa3d_ctx_t *MesaCreateCtx(mesa3d_entry_t *entry, DDSURF *dds, DDSURF *ddz);
void MesaDestroyCtx(mesa3d_ctx_t *ctx);
void MesaDestroyAllCtx(mesa3d_entry_t *entry);
void MesaInitCtx(mesa3d_ctx_t *ctx);
void MesaLightDestroyAll(mesa3d_ctx_t *ctx);

void MesaBlockLock(mesa3d_ctx_t *ctx);
void MesaBlockUnlock(mesa3d_ctx_t *ctx);
BOOL MesaSetCtx(mesa3d_ctx_t *ctx);
void MesaSetTransform(mesa3d_ctx_t *ctx, DWORD xtype, D3DMATRIX *matrix);

/* needs GL_BLOCK */
mesa3d_texture_t *MesaCreateTexture(mesa3d_ctx_t *ctx, DDSURF *surf);
void MesaReloadTexture(mesa3d_texture_t *tex, int tmu);
void MesaDestroyTexture(mesa3d_texture_t *tex);
void MesaApplyTransform(mesa3d_ctx_t *ctx, DWORD changes);
void MesaApplyViewport(mesa3d_ctx_t *ctx, GLint x, GLint y, GLint w, GLint h);

#define MESA_TF_PROJECTION 1
#define MESA_TF_VIEW       2
#define MESA_TF_WORLD      4
//#define MESA_TF_TEXTURE    8
void MesaSetRenderState(mesa3d_ctx_t *ctx, LPD3DSTATE state, LPDWORD RStates);
void MesaDraw(mesa3d_ctx_t *ctx, D3DPRIMITIVETYPE dx_ptype, D3DVERTEXTYPE vtype, LPVOID vertices, DWORD verticesCnt);
void MesaDrawIndex(mesa3d_ctx_t *ctx, D3DPRIMITIVETYPE dx_ptype, D3DVERTEXTYPE vtype,
	LPVOID vertices, DWORD verticesCnt,
	LPWORD indices, DWORD indicesCnt);

void MesaRender(mesa3d_ctx_t *ctx);
void MesaReadback(mesa3d_ctx_t *ctx, GLbitfield mask);
BOOL MesaSetTarget(mesa3d_ctx_t *ctx, DDSURF *dss, DDSURF *dsz);
void MesaSetTextureState(mesa3d_ctx_t *ctx, int tmu, DWORD state, void *value);

void MesaDrawRefreshState(mesa3d_ctx_t *ctx);
void MesaDrawSetSurfaces(mesa3d_ctx_t *ctx);
void MesaDraw3(mesa3d_ctx_t *ctx, DWORD op, void *prim, LPBYTE vertices);
BOOL MesaDraw6(mesa3d_ctx_t *ctx, LPBYTE cmdBufferStart, LPBYTE cmdBufferEnd, LPBYTE vertices, DWORD *error_offset, LPDWORD RStates);

void MesaClear(mesa3d_ctx_t *ctx, DWORD flags, D3DCOLOR color, D3DVALUE depth, DWORD stencil, int rects_cnt, RECT *rects);
DWORD DDSurf_GetBPP(DDSURF *surf);

void MesaStencilApply(mesa3d_ctx_t *ctx);

/* buffer (needs GL_BLOCK) */
void MesaBufferUploadColor(mesa3d_ctx_t *ctx, const void *src);
void MesaBufferDownloadColor(mesa3d_ctx_t *ctx, void *dst);
void MesaBufferUploadDepth(mesa3d_ctx_t *ctx, const void *src);
void MesaBufferDownloadDepth(mesa3d_ctx_t *ctx, void *dst);
void MesaBufferUploadTexture(mesa3d_ctx_t *ctx, mesa3d_texture_t *tex, int level, int tmu);
void MesaBufferUploadTextureChroma(mesa3d_ctx_t *ctx, mesa3d_texture_t *tex, int level, int tmu, DWORD chroma_lw, DWORD chroma_hi);
BOOL MesaBufferFBOSetup(mesa3d_ctx_t *ctx, int width, int height);

/* calculation */
BOOL MesaUnprojectf(GLfloat winx, GLfloat winy, GLfloat winz, GLfloat clipw,
	const GLfloat modelMatrix[16], 
	const GLfloat projMatrix[16],
	const GLint viewport[4],
	GLfloat *objx, GLfloat *objy, GLfloat *objz, GLfloat *objw);
void MesaIdentity(GLfloat matrix[16]);
BOOL MesaIsIdentity(GLfloat matrix[16]);
void MesaSpaceIdentitySet(mesa3d_ctx_t *ctx);
void MesaSpaceIdentityReset(mesa3d_ctx_t *ctx);
void MesaSpaceModelviewSet(mesa3d_ctx_t *ctx);
void MesaSpaceModelviewReset(mesa3d_ctx_t *ctx);
//void MesaConvProjection(GLfloat m[16]);
//void MesaConvView(GLfloat m[16]);
void MesaVetexBlend(mesa3d_ctx_t *ctx, GLfloat coords[3], GLfloat *betas, int betas_cnt, GLfloat out[4]);

/* vertex (needs GL_BLOCK + glBegin) */
void MesaDrawVertex(mesa3d_ctx_t *ctx, LPD3DVERTEX vertex);
void MesaDrawLVertex(mesa3d_ctx_t *ctx, LPD3DLVERTEX vertex);
void MesaDrawTLVertex(mesa3d_ctx_t *ctx, LPD3DTLVERTEX vertex);
/* DX6 */
void MesaFVFSet(mesa3d_ctx_t *ctx, DWORD type, DWORD size);
void MesaDrawFVF(mesa3d_ctx_t *ctx, void *vertex);
void MesaDrawFVFIndex(mesa3d_ctx_t *ctx, void *vertices, int index);
void MesaFVFRecalc(mesa3d_ctx_t *ctx);

/* drawing sequence (needs GL_BLOCK) */
void MesaDrawFVFs(mesa3d_ctx_t *ctx, GLenum gl_ptype, void *vertices, DWORD start, DWORD cnt);

/* scene capture, need GL block */
void MesaSceneBegin(mesa3d_ctx_t *ctx);
void MesaSceneEnd(mesa3d_ctx_t *ctx);

/* DX7 surface tables */
mesa_surfaces_table_t *MesaSurfacesTableGet(mesa3d_entry_t *entry, LPDDRAWI_DIRECTDRAW_LCL lpDDLcl, DWORD max_id);
void MesaSurfacesTableRemoveSurface(mesa3d_entry_t *entry, DDSURF *surf);
void MesaSurfacesTableRemoveDDLcl(mesa3d_entry_t *entry, LPDDRAWI_DIRECTDRAW_LCL lpDDLcl);
void MesaSurfacesTableInsertSurface(mesa3d_entry_t *entry, LPDDRAWI_DIRECTDRAW_LCL lpDDLcl, DWORD id, DDSURF *surf);

BOOL SurfaceExInsert(mesa3d_entry_t *entry, LPDDRAWI_DIRECTDRAW_LCL lpDDLcl, LPDDRAWI_DDRAWSURFACE_LCL surface);
mesa3d_texture_t *SurfaceGetTexture(LPDDRAWI_DDRAWSURFACE_LCL surf, void *ctx, int level);

/* need GL block */
mesa3d_texture_t *MesaTextureFromSurfaceId(mesa3d_ctx_t *ctx, DWORD surfaceId);
void MesaApplyMaterial(mesa3d_ctx_t *ctx);

/* need GL block + viewmodel matrix to matrix.view */
void MesaTLRecalcModelview(mesa3d_ctx_t *ctx);

/* chroma to alpha conversion */
void *MesaChroma32(const void *buf, DWORD w, DWORD h, DWORD lwkey, DWORD hikey);
void *MesaChroma24(const void *buf, DWORD w, DWORD h, DWORD lwkey, DWORD hikey);
void *MesaChroma16(const void *buf, DWORD w, DWORD h, DWORD lwkey, DWORD hikey);
void *MesaChroma15(const void *buf, DWORD w, DWORD h, DWORD lwkey, DWORD hikey);
void *MesaChroma12(const void *buf, DWORD w, DWORD h, DWORD lwkey, DWORD hikey);
void MesaChromaFree(void *ptr);

#define MESA_1OVER16       0.0625f
#define MESA_1OVER255	     0.003921568627451f
#define MESA_1OVER32767    0.000030518509475f
#define MESA_1OVER65535    0.000015259021896f
#define MESA_1OVER16777215 0.000000059604648f

#define MESA_D3DCOLOR_TO_FV(_c, _v) \
	(_v)[0] = RGBA_GETRED(_c)*MESA_1OVER255; \
	(_v)[1] = RGBA_GETGREEN(_c)*MESA_1OVER255; \
	(_v)[2] = RGBA_GETBLUE(_c)*MESA_1OVER255; \
	(_v)[3] = RGBA_GETALPHA(_c)*MESA_1OVER255

#define MESA_D3DCOLORVALUE_TO_FV(_dxcv, _v) \
	(_v)[0] = (_dxcv).r; \
	(_v)[1] = (_dxcv).g; \
	(_v)[2] = (_dxcv).b; \
	(_v)[3] = (_dxcv).a

#define MESA_TMU_CNT() ((VMHALenv.texture_num_units > MESA_TMU_MAX) ? MESA_TMU_MAX : VMHALenv.texture_num_units)

#define SURFACES_TABLE_POOL 1024

/* heavy debug */
#define MESA_KEY_DUMP 1
#define MESA_KEY_DUMP_MORE 2

int mesa_dump_key();
void mesa_dump(mesa3d_ctx_t *ctx);
void mesa_dump_inc();

#endif /* __MESA3D_H__INCLUDED__ */
