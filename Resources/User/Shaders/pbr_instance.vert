#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;
layout(location = 3) in vec4 inTangent;
layout(location = 4) in uvec4 inJoint0;
layout(location = 5) in vec4 inWeights0;

layout(binding = 0) uniform UniformBufferObject{
	mat4 model;
    mat4 view;
    mat4 proj;
	mat4 lightVMat;
	mat4 lightPMat;

	vec4 lightDir;
	vec4 lightColor;
	vec4 cameraPos;

	vec4 baseColorFactor;
	vec4 emissiveFactor;
    vec4 spatialCullPos;
    vec4 ambientColor;

    float time;
    float metallicFactor;
    float roughnessFactor;
    float normalMapScale;

	float occlusionStrength;
    float mipCount;
    float ShadowMapX;
    float ShadowMapY;

    float emissiveStrength;
	float fPad0;
    float fPad1;
    float fPad2;

    int   useBaseColorTexture;
    int   useMetallicRoughnessTexture;
    int   useEmissiveTexture;
    int   useNormalTexture;
    
    int   useOcclusionTexture;
    int   useCubeMap;
    int   useShadowMap;
    int   useIBL;

    int   useSkinMeshAnimation;
    int   useDirCubemap;
    int   pad1;
    int   pad2;
} ubo;

layout(binding = 25) uniform SkinMatrixBuffer
{
    mat4 SkinMat[1024];
} r_SkinMatrixBuffer;

#ifdef USE_OPENGL
layout(binding = 26) uniform sampler2D vertexAnimationTexture;
#else
layout(binding = 26) uniform texture2D vertexAnimationTexture;
layout(binding = 27) uniform sampler vertexAnimationTextureSampler;
#endif

layout(binding = 28) uniform VATUniformBuffer{
	mat4 mPad0;
    mat4 mPad1;
    mat4 mPad2;
	mat4 mPad3;

	float texW;
	float texH;
    float frameNum;
    float endtime;
} vat_ubo;

layout(location = 0) out vec3 f_WorldNormal;
layout(location = 1) out vec2 f_Texcoord;
layout(location = 2) out vec4 f_WorldPos;
layout(location = 3) out vec3 f_WorldTangent;
layout(location = 4) out vec3 f_WorldBioTangent;
layout(location = 5) out vec4 f_LightSpacePos;

#define rot(a) mat2(cos(a), -sin(a), sin(a), cos(a))

vec4 fetchElement(float JointIndex, int Offset, float v)
{
    // テクスチャに焼いたデータを使う時はテクセルの中心からサンプリングすることを心がける(texelSizeX * 0.5 を足す)
    // これを足さない時の値はテクセルの左下、つまりテクセルとテクセルの境界線を差している
    // もしこのままだと補完時にとなりのテクセルの影響を受けて、意図しない値が返ってくることがある
    // この結果、ボーンの動きがブレたり、不安定になるといったことが起こる。
    // VATにはGL_RGBA32F, GL_FLOATのテクスチャに対して、GL_NEAREST・CLAMP_TO_EDGEのサンプラーを適応している
    // GL_NEARESTはサンプリングに最も近い値に補完して返す機能だが、テクセル境界のままだとこれで混ざってしまう
    // 用語整理 /////
    // - ピクセル: 画面上の最小単位(ディスプレイのドット)
    // - テクセル: テクスチャ画像内の最小単位(テクスチャのピクセル)
    ////////////////
    float texelSizeX = 1.0 / vat_ubo.texW;

    vec2 st = vec2(float(JointIndex * 4 + Offset + 0.5) * texelSizeX, v);

    #ifdef USE_OPENGL
    vec4 val = texture(vertexAnimationTexture, st);
    #else
    vec4 val = texture(sampler2D(vertexAnimationTexture, vertexAnimationTextureSampler), st);
    #endif

    return val;
}

mat4 GetSkinMatFromVAT(uint JointIndex, int FrameIndex)
{
    float f_JointIndex = float(JointIndex);
    
    // テクスチャに焼いたデータを使う時はテクセルの中心からサンプリングすることを心がける(texelSizeX * 0.5 を足す)
    // これを足さない時の値はテクセルの左下、つまりテクセルとテクセルの境界線を差している
    // もしこのままだと補完時にとなりのテクセルの影響を受けて、意図しない値が返ってくることがある
    // この結果、ボーンの動きがブレたり、不安定になるといったことが起こる。
    // VATにはGL_RGBA32F, GL_FLOATのテクスチャに対して、GL_NEAREST・CLAMP_TO_EDGEのサンプラーを適応している
    // GL_NEARESTはサンプリングに最も近い値に補完して返す機能だが、テクセル境界のままだとこれで混ざってしまう
    // 用語整理 /////
    // - ピクセル: 画面上の最小単位(ディスプレイのドット)
    // - テクセル: テクスチャ画像内の最小単位(テクスチャのピクセル)
    ////////////////
    float texelSizeY = 1.0 / vat_ubo.texH;

    float v = (float(FrameIndex) + 0.5) * texelSizeY;

    mat4 SkinMatrix = mat4(
        fetchElement(f_JointIndex, 0, v),
        fetchElement(f_JointIndex, 1, v),
        fetchElement(f_JointIndex, 2, v),
        fetchElement(f_JointIndex, 3, v)
    );
    
    return SkinMatrix;
}

float rand(vec2 st)
{
	return fract(sin(dot(st, vec2(12.9898, 78.233))) * 43758.5453123) * 2.0 - 1.0;
}

void main(){
    vec3 BioTangent = cross(inNormal, inTangent.xyz);

    vec4 WorldPos;
    vec3 WorldNormal;
    vec3 WorldTangent;
    vec3 WorldBioTangent;

    // インスタンス描画
    #ifdef USE_OPENGL
    int id = gl_InstanceID;
#else
    int id = gl_InstanceIndex;
#endif
    float sidenum = 64;
    float yid = floor(float(id) / sidenum);
    float xid = float(id) - yid * sidenum;

    xid = xid - sidenum * 0.5;

    // フレームを計算
    float LocalTime = mod(ubo.time + rand(vec2(xid, yid) * 0.5), vat_ubo.endtime);
    int CurrentFrame = int(floor((LocalTime / vat_ubo.endtime) * vat_ubo.frameNum));

    mat4 SkinMat =
        inWeights0.x * GetSkinMatFromVAT(inJoint0.x, CurrentFrame) +
        inWeights0.y * GetSkinMatFromVAT(inJoint0.y, CurrentFrame) +
        inWeights0.z * GetSkinMatFromVAT(inJoint0.z, CurrentFrame) +
        inWeights0.w * GetSkinMatFromVAT(inJoint0.w, CurrentFrame);

    // 位置を決定
    vec3 base = vec3(0.0, -3.0, -10.0); 
    float w = 1.0, h = 1.0;
    vec3 offset = base + vec3(w * xid, 0.0, h * -yid);
    mat4 InsMat = mat4(
        1.0, 0.0, 0.0, offset.x,
        0.0, 1.0, 0.0, offset.y,
        0.0, 0.0, 1.0, offset.z,
        0.0, 0.0, 0.0, 1.0
    );

    WorldPos = SkinMat * vec4(inPosition, 1.0);
    WorldNormal = normalize((SkinMat * vec4(inNormal, 0.0)).xyz);
    WorldTangent = normalize((SkinMat * inTangent).xyz);
    WorldBioTangent = normalize((SkinMat * vec4(BioTangent, 0.0)).xyz);

    float angle = 0.0;
    {
        vec3 pfm = vec3(0.0, 0.125, 0.0);
        vec3 tmp = offset;
        vec3 forward = vec3(0.0, 0.0, -1.0);
        vec3 view = normalize(vec3(tmp.x, 0.0, tmp.z) - vec3(pfm.x, 0.0, pfm.z));
        // view = vec3(0.0, 0.0, 1.0);

        angle = acos(dot(view, forward));
        angle *= (tmp.x > 0.0)? 1.0 : -1.0;
    }

    WorldPos.xz *= rot(angle);
    WorldPos.xz *= rot(-0.45);
    WorldPos *= InsMat;

    //
    gl_Position = ubo.proj * ubo.view * WorldPos;
    f_WorldNormal = WorldNormal;
    f_Texcoord = inTexcoord;
    f_WorldPos = WorldPos;
    f_WorldTangent = WorldTangent;
    f_WorldBioTangent = WorldBioTangent;
    f_LightSpacePos = ubo.lightPMat * ubo.lightVMat * WorldPos;
}