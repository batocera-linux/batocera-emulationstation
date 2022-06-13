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
COMPAT_ATTRIBUTE vec2 VertexCoord;
COMPAT_ATTRIBUTE vec2 TexCoord;
COMPAT_ATTRIBUTE vec4 COLOR;
COMPAT_VARYING   vec2 v_tex;
COMPAT_VARYING   vec4 v_col;
void main(void)                                    
{                                                  
	gl_Position = MVPMatrix * vec4(VertexCoord.xy, 0.0, 1.0);
	v_tex       = TexCoord;                           
	v_col       = COLOR;                           
}

#elif defined(FRAGMENT)
			
varying   vec4      v_col;
varying   vec2      v_tex;
uniform   sampler2D u_tex;
uniform   float saturation;

void main(void)                                    
{                                                  
	vec4 clr = texture2D(u_tex, v_tex) * v_col;
	
	if (mod(gl_FragCoord.y, 2.0) < 1.0) {
		clr = vec4(clr.rgb / 2.0, clr.a);
	}
	
	if (saturation != 1.0) {
		vec3 gray = vec3(dot(clr.rgb, vec3(0.34, 0.55, 0.11)));
		vec3 blend = mix(gray, clr.rgb, saturation);
		clr = vec4(blend, clr.a);
	}

	gl_FragColor = clr;
}
#endif
