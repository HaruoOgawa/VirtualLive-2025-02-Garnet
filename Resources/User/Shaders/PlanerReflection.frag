#version 450

layout(location = 0) in vec2 v2f_UV;

#ifdef USE_OPENGL
layout(binding = 0) uniform sampler2D frameTexture;
#else
layout(binding = 0) uniform texture2D frameTexture;
layout(binding = 1) uniform sampler frameTextureSampler;
#endif

layout(location = 0) out vec4 outColor;

void main()
{
    vec2 st = v2f_UV;
    st.x = 1.0 - st.x;

    #ifdef USE_OPENGL
	vec3 col = texture(frameTexture, st).rgb;
    #else
    vec3 col = texture(sampler2D(frameTexture, frameTextureSampler), st).rgb;
    #endif
     
    outColor = vec4(col, 0.5);
}