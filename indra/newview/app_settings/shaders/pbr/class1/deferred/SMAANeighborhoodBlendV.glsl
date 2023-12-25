/** 
 * @file SMAANeighborhoodBlendV.glsl
 */

/*[EXTRA_CODE_HERE]*/

in vec3 position;

out vec2 vary_texcoord0;
out vec4 vary_offset;

#define float4 vec4
#define float2 vec2
void SMAANeighborhoodBlendingVS(float2 texcoord,
                                out float4 offset);

void main()
{
	gl_Position = vec4(position.xyz, 1.0);	
	vary_texcoord0 = (gl_Position.xy*0.5+0.5);

	SMAANeighborhoodBlendingVS(vary_texcoord0, vary_offset);
}

