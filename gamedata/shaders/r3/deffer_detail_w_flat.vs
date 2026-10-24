#include "common.h"

uniform float4 		consts; // {1/quant,1/quant,diffusescale,ambient}
uniform float4 		wave; 	// cx,cy,cz,tm
uniform float4 		dir2D;

#ifdef USE_DX11
uniform float4 		dt_fade_params; // x=1 camera-dist fade vs y, x=2 light xz fade vs zw, 0=none

// DX11: hardware instancing - per-instance transform comes from vertex stream slot 1.
v2p_flat 	main (v_detail v,
                  float4 m0 : TEXCOORD1, float4 m1 : TEXCOORD2, float4 m2 : TEXCOORD3,
                  float4 inst_na : TEXCOORD4, float4 inst_sh : TEXCOORD5)
{
	v2p_flat 		O;

	// Transform pos to world coords
	float4 	pos;
 	pos.x 		= dot	(m0, v.pos);
 	pos.y 		= dot	(m1, v.pos);
 	pos.z 		= dot	(m2, v.pos);
	pos.w 		= 1;

	// Per-phase fade: scale the blade around its pivot
	float3 Tr = float3(m0.w, m1.w, m2.w);
	float fade = 1.0f;
	if (dt_fade_params.x > 1.5f)
	{
		float2 dxz = Tr.xz - dt_fade_params.zw;
		fade = 1.0f - dot(dxz, dxz) * 0.005f;
	}
	else if (dt_fade_params.x > 0.5f)
	{
		float3 dc = Tr - eye_position;
		fade = 1.0f - max(dot(dc, dc) - dt_fade_params.y, 0.0f) * 0.005f;
	}
	pos.xyz = Tr + (pos.xyz - Tr) * saturate(fade);

	//
	float 	base 	= m1.w;
	float 	dp	= calc_cyclic   (dot(pos,wave));
	float 	H 	= pos.y - base;			// height of vertex (scaled)
	float 	frac 	= v.misc.z*consts.x;		// fractional
	float 	inten 	= H * dp;
	float2 	result	= calc_xz_wave	(dir2D.xz*inten,frac);
	pos		= float4(pos.x+result.x, pos.y, pos.z+result.y, 1);

	// Normal in world coords
	float3 	norm;	//	= float3(0,1,0);
		norm.x 	= pos.x - m0.w	;
		norm.y 	= pos.y - m1.w	+ .75f;	// avoid zero
		norm.z	= pos.z - m2.w	;

	// Final out
	float4	Pp 	= mul		(m_WVP,	pos				);
	O.hpos 		= Pp;
	float3 orig		= mul		(m_WV,  normalize(norm)	);

	O.N = lerp(orig, mul((float3x3)m_WV,  v.pos), 0.25);


	float3	Pe	= mul		(m_WV,  pos				);
//	O.tcdh 		= float4	((v.misc * consts).xy	);
	O.tcdh 		= float4	((v.misc * consts).xyyy );

# if defined(USE_R2_STATIC_SUN)
	O.tcdh.w	= inst_sh.x;						// (,,,dir-occlusion)
# endif
	O.position	= float4	(Pe, 		inst_sh.y	);

	return O;
}
#else
// DX10/R3: baked copies - 'mid' (v.misc.w) selects this copy's transform from array[].
uniform float4 		array[61*4];

v2p_flat 	main (v_detail v)
{
	v2p_flat 		O;
	// index
	int 	i 	= v.misc.w;
	float4  m0 	= array[i+0];
	float4  m1 	= array[i+1];
	float4  m2 	= array[i+2];
	float4  c0 	= array[i+3];

	// Transform pos to world coords
	float4 	pos;
 	pos.x 		= dot	(m0, v.pos);
 	pos.y 		= dot	(m1, v.pos);
 	pos.z 		= dot	(m2, v.pos);
	pos.w 		= 1;

	//
	float 	base 	= m1.w;
	float 	dp	= calc_cyclic   (dot(pos,wave));
	float 	H 	= pos.y - base;			// height of vertex (scaled)
	float 	frac 	= v.misc.z*consts.x;		// fractional
	float 	inten 	= H * dp;
	float2 	result	= calc_xz_wave	(dir2D.xz*inten,frac);
	pos		= float4(pos.x+result.x, pos.y, pos.z+result.y, 1);

	// Normal in world coords
	float3 	norm;	//	= float3(0,1,0);
		norm.x 	= pos.x - m0.w	;
		norm.y 	= pos.y - m1.w	+ .75f;	// avoid zero
		norm.z	= pos.z - m2.w	;

	// Final out
	float4	Pp 	= mul		(m_WVP,	pos				);
	O.hpos 		= Pp;
	float3 orig		= mul		(m_WV,  normalize(norm)	);

	O.N = lerp(orig, mul((float3x3)m_WV,  v.pos), 0.25);

	float3	Pe	= mul		(m_WV,  pos				);
	O.tcdh 		= float4	((v.misc * consts).xyyy );

# if defined(USE_R2_STATIC_SUN)
	O.tcdh.w	= c0.x;								// (,,,dir-occlusion)
# endif
	O.position	= float4	(Pe, 		c0.w		);

	return O;
}
#endif
FXVS;
