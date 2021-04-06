#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragUv;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragLightDir;

void main()
{
	vec3 lightDir = vec3(1, -1, 0);
	float ldotn = dot(lightDir, fragNormal);

	vec3 color = fragColor * texture(texSampler, fragUv).rgb; 
	color *= ldotn;
    outColor = vec4(color, 1.0);
}