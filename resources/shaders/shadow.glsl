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
uniform   vec2      resolution;
uniform   vec2      textureSize;
uniform   vec2      outputSize;

uniform   float      blur;
uniform   float      shadowOffset;

vec4 sampleTexture(sampler2D tex, vec2 texCoord) 
{
    // Check if the texture coordinate is within the [0, 1] range
    if (texCoord.x >= 0 && texCoord.x <= 1 && texCoord.y >= 0 && texCoord.y <= 1)
        return texture2D(tex, texCoord);
    
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
	if (decal.y < 0) // If outputSize is negative
		v_padtex.y = v_tex.y * (1.0 - decal.y);
	
	vec4 sourceColor = sampleTexture(u_tex, v_padtex + decal);

	int numSamples =  3;

	// Initialize a color accumulator
	vec4 blurColor = sourceColor * numSamples;

	int total = numSamples;
	
	for (int i = 0; i < numSamples; i++) {
		vec2 offset = vec2(cos(float(i) * 3.14159 * 2.0 / float(numSamples)), sin(float(i) * 3.14159 * 2.0 / float(numSamples)));		
		for (int b = 1; b < numSamples; b++) {	    
			blurColor += sampleTexture(u_tex, v_padtex + (offset * b * step) + decal);	
			total++;
		}
	}

	// Normalize the accumulated color
	blurColor /= float(total);
	blurColor.r = 0;
	blurColor.g = 0;
	blurColor.b = 0;
	blurColor.a *= 0.5;
	
	// Output the final blurred color
	gl_FragColor = (sampleTexture(u_tex, v_padtex) + blurColor);

}
#endif
