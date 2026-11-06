#version 460 core

#define PI 3.1415926535897932384626433832795
#define MAX_UV_COUNT 8
#define MAX_LIGHT_COUNT 100

in vec3 fPos;
in vec3 fNormal;
in vec3 fTangent;
in vec2 fTexCoords;
flat in int fTexLayer;
in vec4 fColor;

struct Texture
{
    sampler2D Sampler;
    vec4 Color;
};

struct Boost
{
    vec3 Color;
    float Exponent;
};

struct AoParams
{
    sampler2D Sampler;
    float AmbientOcclusion;

    Boost ColorBoost;
    bool HasColorBoost;
};

struct Parameters
{
    Texture Diffuse[MAX_UV_COUNT];
    Texture Normals[MAX_UV_COUNT];
    Texture SpecularMasks[MAX_UV_COUNT];
    Texture Emissive[MAX_UV_COUNT];

    AoParams Ao;
    bool HasAo;

    vec4 EmissiveRegion;
    float RoughnessMin;
    float RoughnessMax;
    float EmissiveMult;
};

// Layered material system (stockings, clothing overlays)
struct StockingLayerParams
{
    sampler2D DiffuseSampler;
    sampler2D NormalSampler;
    sampler2D MaskSampler;
    vec4 Color;
    float TilingU;
    float TilingV;
    bool HasNormal;
};

struct BaseLight
{
    vec4 Color;
    vec3 Position;
    float Intensity;
};

struct Light {
    BaseLight Base;

    float InnerConeAngle;
    float OuterConeAngle;
    float Attenuation;

    float Linear;
    float Quadratic;

    int Type; // 0 Point, 1 Spot
};

uniform Parameters uParameters;
uniform Light uLights[MAX_LIGHT_COUNT];
uniform int uNumLights;
uniform int uUvCount;
uniform float uOpacity;
uniform bool uHasVertexColors;
uniform vec3 uSectionColor;
uniform bool bVertexColors[6];
uniform vec3 uViewPos;

// Layered material uniforms
uniform bool uHasLayeredMaterial;
uniform StockingLayerParams uStockingLayer;
uniform bool uHasORSSMask;
uniform sampler2D uORSSMaskSampler;
uniform bool uHasDMSEMask;
uniform sampler2D uDMSEMaskSampler;

// Material Inspector Preview System
uniform bool uPreviewMode;
uniform sampler2D uMaterialIDMap;
uniform sampler2D uPreviewDiffuse;
uniform sampler2D uPreviewNormals;
uniform sampler2D uPreviewSpecular;
uniform sampler2D uPreviewEmissive;
uniform sampler2D uTargetMaterialIDsMap;
uniform int uNumTargetMaterialIDs;
uniform float uIDMatchTolerance;
uniform int uPreviewTextureType;
uniform float uPreviewOpacity;        // 0.0 = original, 1.0 = full preview (default 1.0)
uniform bool uDebugShowMaterialIDs;   // When true, render Material ID map directly for debugging
uniform bool uShowPreviewBounds;      // When true, tint preview regions for visualization
uniform int uChannelMode;             // 0=RGB, 1=R, 2=G, 3=B, 4=A, 5=Grayscale

out vec4 FragColor;

// Forward declarations for Material ID Preview System.
// Defined later in this file, but referenced earlier by CalcLight.
bool IsTargetMaterial(vec2 uv);
vec3 GetPreviewDiffuse(vec2 uv, int layer);
vec3 GetPreviewNormals(vec2 uv, int layer);
vec3 GetPreviewSpecular(vec2 uv, int layer);
vec3 GetPreviewEmissive(vec2 uv, int layer);
vec3 ApplyChannelMode(vec4 color);

int LayerToIndex()
{
    return clamp(fTexLayer, 0, uUvCount - 1);
}

vec4 SamplerToVector(sampler2D s, vec2 coords)
{
    return texture(s, coords);
}

vec4 SamplerToVector(sampler2D s)
{
    return SamplerToVector(s, fTexCoords);
}

vec3 ComputeNormalsFromSampler(sampler2D normalSampler, vec2 coords)
{
    vec3 normal = texture(normalSampler, coords).rgb * 2.0 - 1.0;

    vec3 t  = normalize(fTangent);
    vec3 n  = normalize(fNormal);
    vec3 b  = -normalize(cross(n, t));
    mat3 tbn = mat3(t, b, n);

    return normalize(tbn * normal);
}

vec3 ComputeNormals(int layer)
{
    return ComputeNormalsFromSampler(uParameters.Normals[layer].Sampler, fTexCoords);
}

vec3 schlickFresnel(vec3 fLambert, float metallic, float hDotv)
{
    vec3 f0 = vec3(0.04);
    f0 = mix(f0, fLambert, metallic);
    return f0 + (1.0 - f0) * pow(clamp(1.0 - hDotv, 0.0, 1.0), 5);
}

float ggxDistribution(float roughness, float nDoth)
{
    float alpha2 = roughness * roughness * roughness * roughness;
    float d = nDoth * nDoth * (alpha2- 1.0) + 1.0;
    return alpha2 / (PI * d * d);
}

float geomSmith(float roughness, float dp)
{
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float denom = dp * (1.0 - k) + k;
    return dp / denom;
}

vec3 CalcLightWithDiffuse(vec3 diffuseColor, vec3 specMasks, vec3 normals, vec3 position, vec3 color, float attenuation, bool global)
{
    vec3 fLambert = diffuseColor;
    float cavity = specMasks.g;
    if (uParameters.HasAo)
    {
        cavity = SamplerToVector(uParameters.Ao.Sampler).g;
    }
    float roughness = mix(uParameters.RoughnessMin, uParameters.RoughnessMax, specMasks.b);

    vec3 l = normalize(uViewPos - fPos);

    vec3 n = normals;
    vec3 v = normalize(position - fPos);
    vec3 h = normalize(v + l);

    float nDotH = max(dot(n, h), 0.0);
    float hDotv = max(dot(h, v), 0.0);
    float nDotL = max(dot(n, l), 0.0);
    float nDotV = max(dot(n, v), 0.0);

    vec3 f = schlickFresnel(fLambert, specMasks.g, hDotv);

    vec3 kS = f;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - cavity;

    vec3 specBrdfNom = ggxDistribution(roughness, nDotH) * geomSmith(roughness, nDotL) * geomSmith(roughness, nDotV) * f;
    float specBrdfDenom = 4.0 * nDotV * nDotL + 0.0001;
    vec3 specBrdf = specBrdfNom / specBrdfDenom;

    vec3 diffuseBrdf = fLambert;
    if (!global) diffuseBrdf = kD * fLambert / PI;
    return (diffuseBrdf + specBrdf) * color * attenuation * nDotL;
}

vec3 CalcLight(int layer, vec3 normals, vec3 position, vec3 color, float attenuation, bool global)
{
    // Use preview-aware diffuse and specular when preview mode is active so
    // dynamic lights (point/spot) reflect the preview texture, not the original.
    vec3 fLambert = uPreviewMode ?
        GetPreviewDiffuse(fTexCoords, layer) :
        (SamplerToVector(uParameters.Diffuse[layer].Sampler).rgb * uParameters.Diffuse[layer].Color.rgb);
    vec3 specular_masks = uPreviewMode ?
        GetPreviewSpecular(fTexCoords, layer) :
        SamplerToVector(uParameters.SpecularMasks[layer].Sampler).rgb;
    return CalcLightWithDiffuse(fLambert, specular_masks, normals, position, color, attenuation, global);
}

vec3 CalcBaseLight(int layer, vec3 normals, BaseLight base, float attenuation, bool global)
{
    return CalcLight(layer, normals, base.Position, base.Color.rgb * base.Intensity, attenuation, global);
}

vec3 CalcPointLight(int layer, vec3 normals, Light light)
{
    float distanceToLight = length(light.Base.Position - fPos);
    float attenuation = 1.0 / (1.0 + light.Linear * distanceToLight + light.Quadratic * pow(distanceToLight, 2));
    return CalcBaseLight(layer, normals, light.Base, attenuation, true);
}

vec3 CalcSpotLight(int layer, vec3 normals, Light light)
{
    vec3 v = normalize(light.Base.Position - fPos);
    float inner = cos(radians(light.InnerConeAngle));
    float outer = cos(radians(light.OuterConeAngle));

    float distanceToLight = length(light.Base.Position - fPos);
    float theta = dot(v, normalize(-vec3(0, -1, 0)));
    float epsilon = inner - outer;
    float attenuation = 1.0 / (1.0 + light.Attenuation * pow(distanceToLight, 2));
    light.Base.Intensity *= smoothstep(0.0, 1.0, (theta - outer) / epsilon);

    if(theta > outer)
    {
        return CalcBaseLight(layer, normals, light.Base, attenuation, true);
    }
    else
    {
        return vec3(0.0);
    }
}

// Material ID Preview System Functions
// Apply channel mode to isolate specific channels
vec3 ApplyChannelMode(vec4 color)
{
    switch (uChannelMode)
    {
        case 1: // Red channel only
            return vec3(color.r);
        case 2: // Green channel only
            return vec3(color.g);
        case 3: // Blue channel only
            return vec3(color.b);
        case 4: // Alpha channel only
            return vec3(color.a);
        case 5: // Grayscale
            {
                float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
                return vec3(gray);
            }
        default: // 0 = RGB (normal view)
            return color.rgb;
    }
}

// Check if current pixel belongs to target material
bool IsTargetMaterial(vec2 uv)
{
    if (!uPreviewMode || uNumTargetMaterialIDs <= 0) return false;

    // Sample material ID map
    vec3 pixelID = texture(uMaterialIDMap, uv).rgb;

    // Compare with every selected material ID so one clicked texture can
    // preview across all materials that reference it.
    for (int i = 0; i < uNumTargetMaterialIDs; i++)
    {
        vec3 targetID = texelFetch(uTargetMaterialIDsMap, ivec2(i, 0), 0).rgb;
        float distance = length(pixelID - targetID);
        if (distance < uIDMatchTolerance)
        {
            return true;
        }
    }

    return false;
}

// Get preview texture for current pixel based on type.
// Mixes preview with original by uPreviewOpacity (1.0 = full preview, 0.0 = original).
vec3 GetPreviewDiffuse(vec2 uv, int layer)
{
    vec3 original = texture(uParameters.Diffuse[layer].Sampler, uv).rgb * uParameters.Diffuse[layer].Color.rgb;
    if (IsTargetMaterial(uv))
    {
        vec4 previewColor = texture(uPreviewDiffuse, uv);
        vec3 preview = ApplyChannelMode(previewColor);
        return mix(original, preview, uPreviewOpacity);
    }
    return original;
}

vec3 GetPreviewNormals(vec2 uv, int layer)
{
    if (IsTargetMaterial(uv) && uPreviewOpacity > 0.0)
    {
        // Sample preview normal and transform to tangent space
        vec3 normal = texture(uPreviewNormals, uv).rgb * 2.0 - 1.0;
        vec3 t = normalize(fTangent);
        vec3 n = normalize(fNormal);
        vec3 b = -normalize(cross(n, t));
        mat3 tbn = mat3(t, b, n);
        vec3 previewNormal = normalize(tbn * normal);

        // Blend preview normal with original when opacity < 1.0
        vec3 originalNormal = ComputeNormals(layer);
        return normalize(mix(originalNormal, previewNormal, uPreviewOpacity));
    }
    // Use original normals
    return ComputeNormals(layer);
}

vec3 GetPreviewSpecular(vec2 uv, int layer)
{
    vec3 original = texture(uParameters.SpecularMasks[layer].Sampler, uv).rgb;
    if (IsTargetMaterial(uv))
    {
        vec4 previewColor = texture(uPreviewSpecular, uv);
        vec3 preview = ApplyChannelMode(previewColor);
        return mix(original, preview, uPreviewOpacity);
    }
    return original;
}

vec3 GetPreviewEmissive(vec2 uv, int layer)
{
    vec3 original = vec3(0.0);
    vec2 coords = fTexCoords;
    if (coords.x > uParameters.EmissiveRegion.x &&
        coords.y > uParameters.EmissiveRegion.y &&
        coords.x < uParameters.EmissiveRegion.z &&
        coords.y < uParameters.EmissiveRegion.w)
    {
        coords.x -= uParameters.EmissiveRegion.x;
        coords.y -= uParameters.EmissiveRegion.y;
        coords.x *= 1.0 / (uParameters.EmissiveRegion.z - uParameters.EmissiveRegion.x);
        coords.y *= 1.0 / (uParameters.EmissiveRegion.w - uParameters.EmissiveRegion.y);
        original = texture(uParameters.Emissive[layer].Sampler, coords).rgb;
    }

    if (IsTargetMaterial(uv))
    {
        vec4 previewColor = texture(uPreviewEmissive, uv);
        vec3 preview = ApplyChannelMode(previewColor);
        return mix(original, preview, uPreviewOpacity);
    }
    return original;
}

// Compute layered material (skin + stockings). When uPreviewMode is true,
// preview textures are sampled per-pixel based on Material ID so that previews
// blend correctly with stockings/clothing overlays.
void ComputeLayeredMaterial(int layer, out vec3 finalDiffuse, out vec3 finalNormal, out vec3 finalSpecMask)
{
    // Base layer (skin) - preview-aware when preview mode is on
    vec3 baseDiffuse = uPreviewMode ?
        GetPreviewDiffuse(fTexCoords, layer) :
        (SamplerToVector(uParameters.Diffuse[layer].Sampler).rgb * uParameters.Diffuse[layer].Color.rgb);
    vec3 baseNormal = uPreviewMode ?
        GetPreviewNormals(fTexCoords, layer) :
        ComputeNormals(layer);
    vec3 baseSpecMask = uPreviewMode ?
        GetPreviewSpecular(fTexCoords, layer) :
        SamplerToVector(uParameters.SpecularMasks[layer].Sampler).rgb;

    if (!uHasLayeredMaterial)
    {
        finalDiffuse = baseDiffuse;
        finalNormal = baseNormal;
        finalSpecMask = baseSpecMask;
        return;
    }

    // Sample stocking mask - R channel typically controls opacity/blend
    vec4 stockingMask = texture(uStockingLayer.MaskSampler, fTexCoords);
    float stockingBlend = stockingMask.r;

    // If no stocking in this area, just use base
    if (stockingBlend < 0.01)
    {
        finalDiffuse = baseDiffuse;
        finalNormal = baseNormal;
        finalSpecMask = baseSpecMask;
        return;
    }

    // Sample stocking textures with tiling
    vec2 stockingUV = fTexCoords * vec2(uStockingLayer.TilingU, uStockingLayer.TilingV);
    vec3 stockingDiffuse = texture(uStockingLayer.DiffuseSampler, stockingUV).rgb * uStockingLayer.Color.rgb;

    // Stocking normal (if available)
    vec3 stockingNormal = baseNormal;
    if (uStockingLayer.HasNormal)
    {
        stockingNormal = ComputeNormalsFromSampler(uStockingLayer.NormalSampler, stockingUV);
    }

    // Stocking specular properties (typically more shiny/glossy than skin)
    vec3 stockingSpecMask = vec3(0.0, 0.3, 0.2); // Low AO, medium metallic, low roughness (shiny)

    // Apply ORSS mask if available (Occlusion/Roughness/Specular/Subsurface)
    if (uHasORSSMask)
    {
        vec4 orss = texture(uORSSMaskSampler, fTexCoords);
        // R = Occlusion, G = Roughness, B = Specular, A = Subsurface
        stockingSpecMask = vec3(orss.r, orss.b, orss.g);
    }

    // Blend based on mask
    // Use a more sophisticated blend that simulates semi-transparent fabric
    float fabricOpacity = stockingBlend * 0.85; // Stockings are semi-transparent

    // For diffuse: blend skin showing through stocking
    finalDiffuse = mix(baseDiffuse, stockingDiffuse * baseDiffuse * 1.5, fabricOpacity);

    // For normal: blend normals
    finalNormal = normalize(mix(baseNormal, stockingNormal, fabricOpacity * 0.7));

    // For specular: stockings are shinier
    finalSpecMask = mix(baseSpecMask, stockingSpecMask, fabricOpacity);
}

void main()
{
    int layer = LayerToIndex();
    vec3 lightDir = normalize(uViewPos - fPos);

    // Debug visualization: render Material ID map directly (or preview bounds overlay).
    // Useful for diagnosing why preview appears in the wrong UV regions.
    if (uDebugShowMaterialIDs)
    {
        vec3 idColor = texture(uMaterialIDMap, fTexCoords).rgb;
        if (uPreviewMode)
        {
            // Tint matching regions magenta so user can see what was matched
            if (IsTargetMaterial(fTexCoords))
                idColor = mix(idColor, vec3(1.0, 0.0, 1.0), 0.5);
        }
        FragColor = vec4(idColor, uOpacity);
        return;
    }

    if (bVertexColors[1])
    {
        vec3 normals = ComputeNormals(layer);
        float diffuseFactor = max(dot(normals, lightDir), 0.4);
        FragColor = vec4(diffuseFactor * uSectionColor, uOpacity);
    }
    else if (bVertexColors[2] && uHasVertexColors)
    {
        vec3 normals = ComputeNormals(layer);
        float diffuseFactor = max(dot(normals, lightDir), 0.4);
        FragColor = vec4(diffuseFactor * fColor.rgb, fColor.a);
    }
    else if (bVertexColors[3])
    {
        vec3 normals = ComputeNormals(layer);
        FragColor = vec4(normals, uOpacity);
    }
    else if (bVertexColors[4])
    {
        vec3 normals = ComputeNormals(layer);
        float diffuseFactor = max(dot(normals, lightDir), 0.4);
        vec4 diffuse = SamplerToVector(uParameters.Diffuse[0].Sampler);
        FragColor = vec4(diffuseFactor * diffuse.rgb, diffuse.a);
    }
    else
    {
        // Compute layered material. ComputeLayeredMaterial itself is preview-aware
        // so stockings/clothing overlays blend correctly with the previewed texture.
        vec3 finalDiffuse, finalNormal, finalSpecMask;
        ComputeLayeredMaterial(layer, finalDiffuse, finalNormal, finalSpecMask);

        // Optional preview bounds visualization - red tint where preview is applied
        if (uShowPreviewBounds && uPreviewMode && IsTargetMaterial(fTexCoords))
        {
            finalDiffuse = mix(finalDiffuse, vec3(1.0, 0.0, 1.0), 0.15);
        }

        float diffuseFactor = max(dot(finalNormal, lightDir), 0.4);
        vec3 result = diffuseFactor * finalDiffuse;

        if (uParameters.HasAo)
        {
            vec3 m = SamplerToVector(uParameters.Ao.Sampler).rgb;
            if (uParameters.Ao.HasColorBoost)
            {
                vec3 color = uParameters.Ao.ColorBoost.Color * uParameters.Ao.ColorBoost.Exponent;
                result = mix(result, result * color, m.b);
            }
            result *= m.r;
        }

        // Emissive: preview-aware so emissive textures can be previewed too
        vec3 emissiveColor;
        if (uPreviewMode)
        {
            emissiveColor = GetPreviewEmissive(fTexCoords, layer);
        }
        else
        {
            emissiveColor = vec3(0.0);
            vec2 coords = fTexCoords;
            if (coords.x > uParameters.EmissiveRegion.x &&
                coords.y > uParameters.EmissiveRegion.y &&
                coords.x < uParameters.EmissiveRegion.z &&
                coords.y < uParameters.EmissiveRegion.w)
            {
                coords.x -= uParameters.EmissiveRegion.x;
                coords.y -= uParameters.EmissiveRegion.y;
                coords.x *= 1.0 / (uParameters.EmissiveRegion.z - uParameters.EmissiveRegion.x);
                coords.y *= 1.0 / (uParameters.EmissiveRegion.w - uParameters.EmissiveRegion.y);
                emissiveColor = SamplerToVector(uParameters.Emissive[layer].Sampler, coords).rgb;
            }
        }
        result += uParameters.Emissive[layer].Color.rgb * emissiveColor * uParameters.EmissiveMult;

        {
            // Use layered material for lighting calculation (preview-aware)
            result += CalcLightWithDiffuse(finalDiffuse, finalSpecMask, finalNormal, uViewPos, vec3(1.0), 1.0, false);

            vec3 lights = vec3(uNumLights > 0 ? 0 : 1);
            for (int i = 0; i < uNumLights; i++)
            {
                if (uLights[i].Type == 0)
                {
                    lights += CalcPointLight(layer, finalNormal, uLights[i]);
                }
                else if (uLights[i].Type == 1)
                {
                    lights += CalcSpotLight(layer, finalNormal, uLights[i]);
                }
            }
            result *= lights;
        }

        result = result / (result + vec3(1.0));
        FragColor = vec4(pow(result, vec3(1.0 / 2.2)), uOpacity);
    }
}
