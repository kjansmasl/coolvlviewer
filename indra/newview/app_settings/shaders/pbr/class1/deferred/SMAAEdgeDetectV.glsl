/** 
 * @file SMAAEdgeDetectV.glsl
 */

/*[EXTRA_CODE_HERE]*/

in vec3 position;

out vec2 vary_texcoord0;
out vec4 vary_offset[3];

#define float4 vec4
#define float2 vec2
void SMAAEdgeDetectionVS(float2 texcoord,
                         out float4 offset[3]);
						 
void main()
{
	gl_Position = vec4(position.xyz, 1.0);	
	vary_texcoord0 = (gl_Position.xy*0.5+0.5);
	
	SMAAEdgeDetectionVS(vary_texcoord0, vary_offset);
}
