/** 
 * @file SMAAEdgeDetectF.glsl
 */ 

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

in vec2 vary_texcoord0;
in vec4 vary_offset[3];

uniform sampler2D tex0;
#if SMAA_PREDICATION
uniform sampler2D tex1;
#endif
uniform sampler2D depthMap;

#define float4 vec4
#define float2 vec2
#define SMAATexture2D(tex) sampler2D tex

float2 SMAAColorEdgeDetectionPS(float2 texcoord,
                                float4 offset[3],
                                SMAATexture2D(colorTex)
                                #if SMAA_PREDICATION
                                , SMAATexture2D(predicationTex)
                                #endif
                                );

void main()
{
	vec2 val = SMAAColorEdgeDetectionPS(vary_texcoord0,
										  vary_offset,
										  tex0
										  #if SMAA_PREDICATION
										  , tex1
										  #endif
										  );
	frag_color = float4(val,0.0,0.0);

	gl_FragDepth = texture(depthMap, vary_texcoord0.xy).r;
}
