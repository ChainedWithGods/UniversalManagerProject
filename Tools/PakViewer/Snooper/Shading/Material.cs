using System;
using System.Collections.Generic;
using System.Linq;
using System.Numerics;
using System.Text.RegularExpressions;
using CUE4Parse.UE4.Assets.Exports.Material;
using CUE4Parse.UE4.Assets.Exports.Texture;
using CUE4Parse.UE4.Objects.Core.Math;
using CUE4Parse.UE4.Objects.Core.Misc;
using PakViewer.Snooper;
using PakViewer.Snooper.Models;
using OpenTK.Graphics.OpenGL4;
using Serilog;

namespace PakViewer.Snooper.Shading;

public class Material : IDisposable
{
    private int _handle;

    public readonly CMaterialParams2 Parameters;
    public MaterialAnalysis Analysis { get; private set; }
    public bool HasAuthoredTextureData { get; private set; }
    public bool UsedFallbackTextureAssignment { get; set; }
    public string Name;
    public string Path;
    public int SelectedChannel;
    public int SelectedTexture;
    public bool IsUsed;

    public Texture[] Diffuse;
    public Texture[] Normals;
    public Texture[] SpecularMasks;
    public Texture[] Emissive;

    public Vector4[] DiffuseColor;
    public Vector4[] EmissiveColor;
    public Vector4 EmissiveRegion;

    public AoParams Ao;
    public bool HasAo;

    public float RoughnessMin = 0f;
    public float RoughnessMax = 1f;
    public float EmissiveMult = 1f;

    private Texture? _proceduralIDMap;
    public Vector3 ProceduralMaterialID;

    public Material()
    {
        Parameters = new CMaterialParams2();
        Analysis = MaterialAnalysis.Unknown;
        HasAuthoredTextureData = false;
        UsedFallbackTextureAssignment = false;
        Name = "";
        Path = "None";
        IsUsed = false;

        Diffuse = Array.Empty<Texture>();
        Normals = Array.Empty<Texture>();
        SpecularMasks = Array.Empty<Texture>();
        Emissive = Array.Empty<Texture>();

        DiffuseColor = Array.Empty<Vector4>();
        EmissiveColor = Array.Empty<Vector4>();
        EmissiveRegion = new Vector4(0, 0, 1, 1);
    }

    public Material(UMaterialInterface unrealMaterial) : this()
    {
        SwapMaterial(unrealMaterial);
    }

    public void SwapMaterial(UMaterialInterface unrealMaterial)
    {
        Name = unrealMaterial.Name;
        Path = unrealMaterial.GetPathName();
        UsedFallbackTextureAssignment = false;

        var preferred = UserSettings.Default.MaterialExportFormat;
        var usedDepth = preferred;

        ResetParams(Parameters);
        unrealMaterial.GetParams(Parameters, preferred);

        if (Parameters.IsNull)
        {
            var fallbacks = new[]
            {
                EMaterialDepth.TopLayerOnly,
                EMaterialDepth.AllLayersNoRef,
                EMaterialDepth.AllLayers
            };

            foreach (var format in fallbacks)
            {
                if (format == preferred) continue;

                ResetParams(Parameters);
                unrealMaterial.GetParams(Parameters, format);

                if (!Parameters.IsNull)
                {
                    usedDepth = format;
                    break;
                }
            }
        }

        MaterialParameterResolver.ResolveFromParents(Parameters, unrealMaterial, usedDepth);

        HasAuthoredTextureData = Parameters.Textures.Count > 0 ||
                                 Parameters.HasTopDiffuse ||
                                 Parameters.HasTopNormals ||
                                 Parameters.HasTopSpecularMasks ||
                                 Parameters.HasTopEmissive;
        Analysis = MaterialAnalysisExtensions.Analyze(unrealMaterial, Parameters);
    }

    private static void ResetParams(CMaterialParams2 parameters)
    {
        parameters.Textures.Clear();
        parameters.Colors.Clear();
        parameters.Scalars.Clear();
        parameters.Switches.Clear();
        parameters.Properties.Clear();
        parameters.BlendMode = EBlendMode.BLEND_Opaque;
        parameters.ShadingModel = EMaterialShadingModel.MSM_Unlit;
    }

    public void Setup(Options options, int uvCount)
    {
        _handle = GL.CreateProgram();

        if (uvCount < 1 || Parameters.IsNull)
        {

            Diffuse = new[] { new Texture(FLinearColor.Gray) };
            Normals = new[] { new Texture(new FLinearColor(0.5f, 0.5f, 1f, 1f)) };
            SpecularMasks = new[] { new Texture(new FLinearColor(0f, 0f, 0.5f, 1f)) };
            Emissive = new Texture[1];
            DiffuseColor = FillColors(1, Diffuse, CMaterialParams2.DiffuseColors, Vector4.One);
            EmissiveColor = new[] { Vector4.One };
            return;
        }

        Diffuse = FillTextures(options, uvCount, Parameters.HasTopDiffuse, CMaterialParams2.Diffuse, CMaterialParams2.FallbackDiffuse, true, false);
        Normals = FillTextures(options, uvCount, Parameters.HasTopNormals, CMaterialParams2.Normals, CMaterialParams2.FallbackNormals, false, true);

        SpecularMasks = FillTextures(options, uvCount, Parameters.HasTopSpecularMasks, CMaterialParams2.SpecularMasks, CMaterialParams2.FallbackSpecularMasks, false, true);
        Emissive = new Texture[uvCount];

        for (int i = 0; i < uvCount; i++)
        {
            Diffuse[i] ??= new Texture(FLinearColor.Gray);
            Normals[i] ??= new Texture(new FLinearColor(0.5f, 0.5f, 1f, 1f));
            SpecularMasks[i] ??= new Texture(new FLinearColor(0f, 0f, 0.5f, 1f));
        }

        DiffuseColor = FillColors(uvCount, Diffuse, CMaterialParams2.DiffuseColors, Vector4.One);
        EmissiveColor = FillColors(uvCount, Emissive, CMaterialParams2.EmissiveColors, Vector4.One);

        if (Parameters.TryGetScalar(out var roughnessMin, "RoughnessMin", "SpecRoughnessMin"))
            RoughnessMin = roughnessMin;
        if (Parameters.TryGetScalar(out var roughnessMax, "RoughnessMax", "SpecRoughnessMax"))
            RoughnessMax = roughnessMax;
        if (Parameters.TryGetScalar(out var roughness, "Rough", "Roughness"))
        {
            var d = roughness / 2;
            RoughnessMin = roughness - d;
            RoughnessMax = roughness + d;
        }

        EmissiveMult = 0f;

        EnsureProceduralIDMap();
    }

    public void Update(Options options)
    {
        var uvCount = Diffuse?.Length ?? 0;
        if (uvCount <= 0)
        {
            return;
        }
        Setup(options, uvCount);
    }

    public void EnsureProceduralIDMap()
    {
        if (_proceduralIDMap != null) return;

        var hash = string.IsNullOrEmpty(Name) ? 0 : Name.GetHashCode();
        var r = ((hash >> 16) & 0xFF) / 255f;
        var g = ((hash >> 8) & 0xFF) / 255f;
        var b = (hash & 0xFF) / 255f;

        ProceduralMaterialID = new Vector3(r, g, b);
        _proceduralIDMap = new Texture(new FLinearColor(r, g, b, 1f));

    }

    public Texture? ProceduralIDMap => _proceduralIDMap;

    private Texture[] FillTextures(Options options, int uvCount, bool top, string[][] triggers, string fallback, bool first = false, bool forceLinear = false)
    {
        UTexture original;
        Texture transformed;
        var fix = fallback == CMaterialParams2.FallbackSpecularMasks;
        var textures = new Texture[uvCount];

        if (top)
        {

            if (Parameters.TryGetTexture2d(out original, triggers[0]) && options.TryGetTexture(original, fix, forceLinear, out transformed))
            {
                textures[0] = transformed;

                for (int i = 1; i < textures.Length; i++)
                {
                    textures[i] = transformed;
                }
            }
        }
        else if (Parameters.TryGetTexture2d(out original, fallback) && options.TryGetTexture(original, fix, forceLinear, out transformed))
        {
            for (int i = 0; i < textures.Length; i++)
                textures[i] = transformed;
        }
        else if (first && TryGetFirstTexture2dForFallback(fallback, out original) && options.TryGetTexture(original, fix, forceLinear, out transformed))
        {
            for (int i = 0; i < textures.Length; i++)
                textures[i] = transformed;
        }
        return textures;
    }

    private bool TryGetFirstTexture2dForFallback(string fallback, out UTexture texture)
    {
        texture = null;

        Regex? regex = null;
        if (fallback == CMaterialParams2.FallbackDiffuse)
            regex = new Regex(CMaterialParams2.RegexDiffuse, RegexOptions.IgnoreCase);
        else if (fallback == CMaterialParams2.FallbackNormals)
            regex = new Regex(CMaterialParams2.RegexNormals, RegexOptions.IgnoreCase);
        else if (fallback == CMaterialParams2.FallbackSpecularMasks)
            regex = new Regex(CMaterialParams2.RegexSpecularMasks, RegexOptions.IgnoreCase);
        else if (fallback == CMaterialParams2.FallbackEmissive)
            regex = new Regex(CMaterialParams2.RegexEmissive, RegexOptions.IgnoreCase);

        if (regex != null)
        {
            foreach (var unrealMat in Parameters.GetTexturesByRegex(regex))
            {
                if (unrealMat is UTexture t)
                {
                    texture = t;
                    return true;
                }
            }
        }

        return Parameters.TryGetFirstTexture2d(out texture);
    }

    private Vector4[] FillColors(int uvCount, Texture[] textures, string[][] triggers, Vector4 fallback)
    {
        var colors = new Vector4[uvCount];
        for (int i = 0; i < colors.Length; i++)
        {
            if (textures[i] == null) continue;

            if (Parameters.TryGetLinearColor(out var color, triggers[i]) && color is { A: > 0 })
            {
                colors[i] = new Vector4(color.R, color.G, color.B, color.A);
            }
            else colors[i] = fallback;
        }
        return colors;
    }

    public void Render(Shader shader)
    {
        var unit = 0;
        for (var i = 0; i < Diffuse.Length; i++)
        {
            shader.SetUniform($"uParameters.Diffuse[{i}].Sampler", unit);
            shader.SetUniform($"uParameters.Diffuse[{i}].Color", DiffuseColor[i]);
            Diffuse[i]?.Bind(TextureUnit.Texture0 + unit);
            unit++;
        }

        for (var i = 0; i < Normals.Length; i++)
        {
            shader.SetUniform($"uParameters.Normals[{i}].Sampler", unit);
            Normals[i]?.Bind(TextureUnit.Texture0 + unit);
            unit++;
        }

        for (var i = 0; i < SpecularMasks.Length; i++)
        {
            shader.SetUniform($"uParameters.SpecularMasks[{i}].Sampler", unit);
            SpecularMasks[i]?.Bind(TextureUnit.Texture0 + unit);
            unit++;
        }

        for (var i = 0; i < Emissive.Length; i++)
        {
            shader.SetUniform($"uParameters.Emissive[{i}].Sampler", unit);
            shader.SetUniform($"uParameters.Emissive[{i}].Color", EmissiveColor[i]);
            Emissive[i]?.Bind(TextureUnit.Texture0 + unit++);
        }

        Ao.Texture?.Bind(TextureUnit.Texture31);
        shader.SetUniform("uParameters.Ao.Sampler", 31);
        shader.SetUniform("uParameters.Ao.HasColorBoost", Ao.HasColorBoost);
        shader.SetUniform("uParameters.Ao.ColorBoost.Color", Ao.ColorBoost.Color);
        shader.SetUniform("uParameters.Ao.ColorBoost.Exponent", Ao.ColorBoost.Exponent);
        shader.SetUniform("uParameters.HasAo", HasAo);

        EnsureProceduralIDMap();
        if (_proceduralIDMap != null)
        {
            _proceduralIDMap.Bind(TextureUnit.Texture20);
            shader.SetUniform("uMaterialIDMap", 20);
        }

        shader.SetUniform("uParameters.EmissiveRegion", EmissiveRegion);
        shader.SetUniform("uParameters.RoughnessMin", RoughnessMin);
        shader.SetUniform("uParameters.RoughnessMax", RoughnessMax);
        shader.SetUniform("uParameters.EmissiveMult", EmissiveMult);
    }

    private bool _disposed;

    public void Dispose()
    {
        if (_disposed) return;

        if (Diffuse != null)
        {
            for (int i = 0; i < Diffuse.Length; i++)
            {
                if (Diffuse[i] != null && !Diffuse[i].IsShared)
                    Diffuse[i].Dispose();
            }
            Array.Clear(Diffuse, 0, Diffuse.Length);
            Diffuse = null;
        }

        if (Normals != null)
        {
            for (int i = 0; i < Normals.Length; i++)
            {
                if (Normals[i] != null && !Normals[i].IsShared)
                    Normals[i].Dispose();
            }
            Array.Clear(Normals, 0, Normals.Length);
            Normals = null;
        }

        if (SpecularMasks != null)
        {
            for (int i = 0; i < SpecularMasks.Length; i++)
            {
                if (SpecularMasks[i] != null && !SpecularMasks[i].IsShared)
                    SpecularMasks[i].Dispose();
            }
            Array.Clear(SpecularMasks, 0, SpecularMasks.Length);
            SpecularMasks = null;
        }

        if (Emissive != null)
        {
            for (int i = 0; i < Emissive.Length; i++)
            {
                if (Emissive[i] != null && !Emissive[i].IsShared)
                    Emissive[i].Dispose();
            }
            Array.Clear(Emissive, 0, Emissive.Length);
            Emissive = null;
        }

        if (Ao.Texture != null && !Ao.Texture.IsShared)
        {
            Ao.Texture.Dispose();
        }
        Ao.Texture = null;

        if (_proceduralIDMap != null)
        {
            _proceduralIDMap.Dispose();
            _proceduralIDMap = null;
        }

        GL.DeleteProgram(_handle);
        _disposed = true;
    }
}

public struct AoParams
{
    public Texture Texture;

    public Boost ColorBoost;
    public bool HasColorBoost;
}

public struct Boost
{
    public Vector3 Color;
    public float Exponent;
}
