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
			
#if __VERSION__ >= 130
#define COMPAT_VARYING in
#define COMPAT_TEXTURE texture
out vec4 FragColor;
#else
#define COMPAT_VARYING varying
#define FragColor gl_FragColor
#define COMPAT_TEXTURE texture2D
#endif

#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#define COMPAT_PRECISION mediump
#else
#define COMPAT_PRECISION
#endif
			
COMPAT_VARYING   vec4      v_col;
COMPAT_VARYING   vec2      v_tex;

uniform   sampler2D u_tex;
uniform   COMPAT_PRECISION vec2      resolution;
uniform   COMPAT_PRECISION vec2      textureSize;
uniform   COMPAT_PRECISION vec2      outputSize;

uniform   COMPAT_PRECISION float      pixelSize;
uniform   COMPAT_PRECISION vec2       u_resolution;

void main(void)                                    
{         
	vec2 p = v_tex.st;
	
	float pixels = pixelSize;
	if (pixels == 0.0) {
		FragColor = COMPAT_TEXTURE(u_tex, v_tex);
	}
	else {
		// Calculate pixel size in normalized device coordinates
		vec2 pixelSizeNDC = vec2(pixelSize) / outputSize;

		// Snap the texture coordinates to a grid based on pixel size
		p.x = floor(p.x / pixelSizeNDC.x) * pixelSizeNDC.x;
		p.y = floor(p.y / pixelSizeNDC.y) * pixelSizeNDC.y;

		FragColor = COMPAT_TEXTURE(u_tex, p);
	}
}
#endif
