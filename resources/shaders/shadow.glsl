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

uniform   COMPAT_PRECISION float      blur;
uniform   COMPAT_PRECISION float      shadowOffset;

vec4 sampleTexture(sampler2D tex, vec2 texCoord) 
{
    // Check if the texture coordinate is within the [0, 1] range
    if (texCoord.x >= 0.0 && texCoord.x <= 1.0 && texCoord.y >= 0.0 && texCoord.y <= 1.0)
        return COMPAT_TEXTURE(tex, texCoord);
    
    return vec4(0.0); // Return transparent black for coordinates outside [0, 1]    
}

void main(void)                                    
{         
	float blurSize = blur;
	if (blurSize == 0.0) {
		blurSize = 3.0;
	}
	
	float offset = shadowOffset;
	if (offset == 0.0) {
		offset = 6.0;
	}
	
	vec2 step = 1.0 / textureSize;
	
	vec2 decal = vec2(-offset / outputSize.x, offset / outputSize.y);	

	vec2 v_padtex = vec2(v_tex.x / (1.0 + decal.x), v_tex.y * (1.0 + decal.y) - decal.y);
	if (decal.y < 0.0) // If outputSize is negative
		v_padtex.y = v_tex.y * (1.0 - decal.y);
	
	vec4 sourceColor = sampleTexture(u_tex, v_padtex + decal);

	int numSamples =  3;

	// Initialize a color accumulator
	vec4 blurColor = sourceColor * float(numSamples);

	int total = numSamples;
	
	for (int i = 0; i < numSamples; i++) {
		vec2 offset = vec2(cos(float(i) * 3.14159 * 2.0 / float(numSamples)), sin(float(i) * 3.14159 * 2.0 / float(numSamples)));		
		for (int b = 1; b < numSamples; b++) {	    
			vec4 sampled = sampleTexture(u_tex, v_padtex + (offset * float(b) * step) + decal);	
			if (sampled.a > 0) {
				blurColor += sampled;
			}
			total++;
		}
	}

	// Normalize the accumulated color
	blurColor /= float(total);
	blurColor.r = 0.0;
	blurColor.g = 0.0;
	blurColor.b = 0.0;
	blurColor.a *= 0.5;
	
	// Output the final blurred color	

	vec4 texColor = sampleTexture(u_tex, v_padtex);
	
	if (texColor.a == 0.0)	{
		FragColor = blurColor * v_col;
	}
	else {
		FragColor = (texColor + blurColor) * v_col;
	}

}
#endif
