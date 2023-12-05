#if defined(VERTEX)

#if __VERSION__ >= 130
#define COMPAT_VARYING out
#define COMPAT_ATTRIBUTE in
#define COMPAT_TEXTURE texture
#else
#define COMPAT_VARYING varying 
#define COMPAT_ATTRIBUTE attribute 
#define COMPAT_TEXTURE texture2D
#endif

#ifdef GL_ES
#define COMPAT_PRECISION mediump
#else
#define COMPAT_PRECISION
#endif

uniform   mat4 MVPMatrix;
uniform   vec2       resolution;

COMPAT_ATTRIBUTE vec2 VertexCoord;
COMPAT_ATTRIBUTE vec2 TexCoord;
COMPAT_ATTRIBUTE vec4 COLOR;
COMPAT_VARYING   vec2 v_tex;
COMPAT_VARYING   vec4 v_col;
COMPAT_VARYING   vec2 v_pos;

void main(void)                                    
{                                                  
	gl_Position = MVPMatrix * vec4(VertexCoord.xy, 0.0, 1.0);
	v_tex       = TexCoord;                           
	v_col       = COLOR;      
	v_pos       = VertexCoord.xy;	
}

#elif defined(FRAGMENT)
			
varying   vec4      v_col;
varying   vec2      v_tex;
uniform   sampler2D u_tex;
varying   vec2      v_pos;

uniform   float     s_offset;
uniform   float     s_height;
uniform   float     s_forcetop;
uniform   float     s_forcebottom;

void main(void)                                    
{                                                  
	vec4 clr = texture2D(u_tex, v_tex) * v_col;

	float top = v_pos.y - s_offset;
	if (top < s_forcetop) {
		float diff = 1.0 - ((s_forcetop - top) / s_forcetop);
		clr.a = clr.a * diff;
	}
	
	float bottom = s_height - (v_pos.y - s_offset);
	if (bottom < s_forcebottom) {
		float diff = 1.0 - ((s_forcebottom - bottom) / s_forcebottom);
		clr.a = clr.a * diff;
	}
		
	gl_FragColor = clr;
}
#endif
