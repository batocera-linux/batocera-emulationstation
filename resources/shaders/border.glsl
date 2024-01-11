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
COMPAT_VARYING   vec2 v_pos;

void main(void)                                    
{                                                  
	gl_Position = MVPMatrix * vec4(VertexCoord.xy, 0.0, 1.0);
	v_tex       = TexCoord;                           
	v_col       = COLOR;                           
	v_pos       = VertexCoord;
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
COMPAT_VARYING   vec2      v_pos;

uniform   sampler2D u_tex;
uniform   vec2      resolution;
uniform   vec2      textureSize;
uniform   vec2      outputSize;
uniform   vec2      outputOffset;

uniform   float      borderSize;
uniform   vec4       borderColor;
uniform   float      cornerRadius;
uniform   float      innerShadowSize;
uniform   vec4       innerShadowColor;

uniform   float      outerShadowSize;
uniform   vec4       outerShadowColor;

uniform   float		saturation;

uniform bool		bilinearFiltering;

vec4 sampleTexture(sampler2D tex, vec2 texCoord) 
{
    // Check if the texture coordinate is within the [0, 1] range
   // if (texCoord.x >= 0 && texCoord.x <= 1 && texCoord.y >= 0 && texCoord.y <= 1)
        return COMPAT_TEXTURE(tex, texCoord);
    
  //  return vec4(0.0); // Return transparent black for coordinates outside [0, 1]    
}

float getComputedValue(float value, float defaultValue) {
	if (value == 0.0)
		return defaultValue;
	
	if (value < 1.0)
		return abs(outputSize.y) * value;
	
	return value;
}

void main(void)                                    
{
	float outerBorder = getComputedValue(borderSize, 0.0);
	float innerShadow = getComputedValue(innerShadowSize, 0.0);
	float outerShadow = getComputedValue(outerShadowSize, 0.0);
	float cornerSize = getComputedValue(cornerRadius, 0.0);

	// Don't perform bordering if bottom/right pixel is transparent
	vec2 bottomRight = vec2(1.0, 1.0);
	if (sampleTexture(u_tex, bottomRight).a < 0.3) {
		FragColor = sampleTexture(u_tex, v_tex);
		return;
	}
	
	vec2 topLeft = vec2(0.0, 0.0);
	if (sampleTexture(u_tex, topLeft).a < 0.3) {
		FragColor = sampleTexture(u_tex, v_tex);
		return;
	}
		
	// Apply border padding
	vec2 decal = vec2((outerBorder/2.0+outerShadow) / abs(outputSize.x), (outerBorder/2.0+outerShadow) / abs(outputSize.y));	
	vec2 v_padtex = vec2(v_tex.x / (1.0 - 2.0 * decal.x) - decal.x, v_tex.y * (1.0 + 2.0 * decal.y) - decal.y);

	// read texture
	vec4 sampledColor = sampleTexture(u_tex, v_padtex);

    // Apply bilinear filtering
	if (bilinearFiltering)
	{
		vec2 texelSize = 1.0 / textureSize;
		vec2 uv = v_padtex;

		vec2 f = fract(uv);

		vec4 texel00 = sampleTexture(u_tex, uv);
		
		vec4 texel10 = sampleTexture(u_tex, uv + vec2(texelSize.x, 0.0));
		vec4 texel01 = sampleTexture(u_tex, uv + vec2(0.0, texelSize.y));
		vec4 texel11 = sampleTexture(u_tex, uv + texelSize);

		// Bilinear interpolation
		vec4 interpolatedColor = mix(
			mix(texel00, texel10, f.x),
			mix(texel01, texel11, f.x),
			f.y
		);

		sampledColor = interpolatedColor;
	}

	if (saturation != 1.0) {
		vec3 gray = vec3(dot(sampledColor.rgb, vec3(0.34, 0.55, 0.11)));
		vec3 blend = mix(gray, sampledColor.rgb, saturation);
		sampledColor = vec4(blend, sampledColor.a);
	}
				
	sampledColor *= v_col;
				 
	vec2 middle = vec2(abs(outputSize.x), abs(outputSize.y)) / 2.0;
	vec2 center = abs(v_pos - outputOffset - middle);
	vec2 q = center - middle + cornerSize;
	
	float distance = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - cornerSize;	
	if (distance > 0.0) {
		discard;
	}		
	else if (distance > -(outerBorder + innerShadow + outerShadow)) {		
				
		if (outerShadow != 0.0 && distance > -outerShadow) {
			sampledColor = outerShadowColor;
			sampledColor.a = outerShadowColor.a * (1.0 - (outerShadow + distance) / (outerShadow)) * v_col.a;			
		}		
		else if (distance > -(outerBorder + outerShadow)) {
			sampledColor = borderColor;
			sampledColor.a *= v_col.a;
		}
		else if (innerShadow != 0.0) {		
			float val = abs(outerBorder + outerShadow + distance) / innerShadow;
			sampledColor = mix(sampledColor, innerShadowColor, innerShadowColor.a * (1.0 - val)); //(distance + outerBorder + outerShadow + innerShadow) / (outerBorder + outerShadow + innerShadow));			
		}
	}
	else {
		float pixelValue;
		pixelValue = 1.0 - smoothstep(-0.75, 0.5, distance);
		sampledColor.a *= pixelValue;
		sampledColor.rgb *= pixelValue;
	}
	
	
	FragColor = sampledColor;
}
#endif
