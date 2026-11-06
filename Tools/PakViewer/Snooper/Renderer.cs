using System;
using System.Collections.Generic;
using System.Linq;
using System.Numerics;
using System.Threading;
using CUE4Parse_Conversion.Animations;
using CUE4Parse_Conversion.Meshes;
using CUE4Parse_Conversion.Dto;
using CUE4Parse_Conversion.Options;
using CUE4Parse_Conversion.Writers.ActorX.Structs.Animations;
using CUE4Parse.UE4.Assets;
using CUE4Parse.UE4.Assets.Exports;
using CUE4Parse.UE4.Assets.Exports.Animation;
using CUE4Parse.UE4.Assets.Exports.Component.SplineMesh;
using CUE4Parse.UE4.Assets.Exports.Component.StaticMesh;
using CUE4Parse.UE4.Assets.Exports.GeometryCollection;
using CUE4Parse.UE4.Assets.Exports.Material;
using CUE4Parse.UE4.Assets.Exports.Nanite;
using CUE4Parse.UE4.Assets.Exports.Paper2D;
using CUE4Parse.UE4.Assets.Exports.SkeletalMesh;
using CUE4Parse.UE4.Assets.Exports.StaticMesh;
using CUE4Parse.UE4.Assets.Exports.Texture;
using CUE4Parse.UE4.Objects.Core.Math;
using CUE4Parse.UE4.Objects.Core.Misc;
using CUE4Parse.UE4.Objects.Engine;
using CUE4Parse.UE4.Objects.Meshes;
using CUE4Parse.UE4.Objects.RenderCore;
using CUE4Parse.UE4.Objects.UObject;
using CUE4Parse.UE4.VirtualFileSystem;
using CUE4Parse.Utils;
using Serilog;
using PakViewer.Snooper;
using PakViewer.Snooper.Animations;
using PakViewer.Snooper.Buffers;
using PakViewer.Snooper.Lights;
using GridModel = PakViewer.Snooper.Models.Grid;
using PakViewer.Snooper.Models;
using PakViewer.Snooper.Shading;
using OpenTK.Windowing.GraphicsLibraryFramework;
using UModel = PakViewer.Snooper.Models.UModel;

namespace PakViewer.Snooper;

public enum VertexColor
{
    Default,
    Sections,
    Colors,
    Normals,
    TextureCoordinates
}

public class Renderer : IDisposable
{
    private readonly Skybox _skybox;
    private readonly GridModel _grid;
    private Shader _shader;
    private Shader _outline;
    private Shader _light;
    private Shader _bone;
    private Shader _collision;
    private bool _saveCameraMode;
    private bool _disposed;
    private readonly Dictionary<string, UMaterialInterface?> _materialResolveCache = new(StringComparer.OrdinalIgnoreCase);

    public bool ShowSkybox;
    public bool ShowGrid;
    public bool ShowLights;
    public bool AnimateWithRotationOnly;
    public bool IsSkeletonTreeOpen;
    public VertexColor Color;

    public Camera CameraOp { get; }
    public PickingTexture Picking { get; }
    public Options Options { get; }

    public Renderer(int width, int height)
    {
        _skybox = new Skybox();
        _grid = new GridModel();

        CameraOp = new Camera();
        Picking = new PickingTexture(width, height);
        Options = new Options();

        ShowSkybox = UserSettings.Default.ShowSkybox;
        ShowGrid = UserSettings.Default.ShowGrid;
        AnimateWithRotationOnly = UserSettings.Default.AnimateWithRotationOnly;
        Color = VertexColor.Default;
    }

    public void Load(CancellationToken cancellationToken, UObject dummy, Lazy<UObject> export)
    {
        ShowLights = false;
        Color = VertexColor.Default;
        _saveCameraMode = dummy is not UWorld and not UBlueprintGeneratedClass;
        switch (dummy)
        {
            case UStaticMesh when export.Value is UStaticMesh st:
                LoadStaticMesh(st, UserSettings.Default.NaniteMeshExportFormat);
                break;
            case USkeletalMesh when export.Value is USkeletalMesh sk:
                LoadSkeletalMesh(sk);
                break;
            case USkeleton when export.Value is USkeleton skel:
                LoadSkeleton(skel);
                break;
            case UMaterialInstance when export.Value is UMaterialInstance mi:
                LoadMaterialInstance(mi);
                break;
            case UWorld when export.Value is UWorld wd:
                LoadWorld(cancellationToken, wd, Transform.Identity);
                break;
            case UBlueprintGeneratedClass when export.Value is UBlueprintGeneratedClass bp:
                LoadJunoWorld(cancellationToken, bp, Transform.Identity);
                Color = VertexColor.Colors;
                break;
            case UPaperSprite when export.Value is UPaperSprite ps:
                LoadPaperSprite(ps);
                break;
        }
        CameraOp.Mode = _saveCameraMode ? UserSettings.Default.CameraMode : Camera.WorldMode.FlyCam;
        SetupCamera();
    }

    public void Swap(UMaterialInstance unrealMaterial)
    {
        if (!Options.TryGetModel(out var model) || !Options.TryGetSection(model, out var section)) return;

        model.Materials[section.MaterialIndex].SwapMaterial(unrealMaterial);
        model.Materials[section.MaterialIndex].Setup(Options, model.UvCount);
    }

    public void Animate(UObject anim)
    {
        if (!SnooperContext.ModelIsWaitingAnimation)
        {
            if (anim is UAnimSequenceBase animBase)
            {

if (animBase.Skeleton.TryLoad(out USkeleton skeleton))
                {
                    LoadSkeleton(skeleton);
                }
            }
            else return;
        }

        Animate(anim, Options.SelectedModel);
    }
    private void Animate(UObject anim, FGuid guid)
    {
        if (anim is not UAnimSequenceBase animBase || !animBase.Skeleton.TryLoad(out USkeleton skeleton) ||
            !Options.TryGetModel(guid, out var m) || m is not SkeletalModel model)
            return;

        CAnimSet animSet = null;
        try
        {
            animSet = animBase switch
            {
                UAnimSequence animSequence => skeleton.ConvertAnims(animSequence),
                UAnimMontage animMontage => skeleton.ConvertAnims(animMontage),
                UAnimComposite animComposite => skeleton.ConvertAnims(animComposite),
                _ => throw new ArgumentException("Unknown animation type")
            };
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to convert animation");
            return;
        }

        if (animSet == null)
        {
            Log.Warning("Failed to convert animation: animSet is null");
            return;
        }

        if (model.Skeleton == null)
        {
            Log.Warning("Failed to animate: model.Skeleton is null");
            return;
        }

        var animation = new Animation(anim, animSet, guid);
        model.Skeleton.Animate(animSet);
        Options.AddAnimation(animation);

        if (animBase.Notifies != null)
        {
            foreach (var notifyEvent in animBase.Notifies)
            {
                if (notifyEvent?.NotifyStateClass == null ||
                    !notifyEvent.NotifyStateClass.TryLoad(out UObject notifyClass) ||
                    !notifyClass.TryGetValue(out UObject export, "SkeletalMeshProp", "StaticMeshProp", "Mesh", "SkeletalMeshTemplate"))
                    continue;

            var t = Transform.Identity;
            if (notifyClass.TryGetValue(out FTransform offset, "Offset"))
            {
                t.Rotation = offset.Rotation;
                t.Position = offset.Translation * Constants.SCALE_DOWN_RATIO;
                t.Scale = offset.Scale3D;
            }

            UModel addedModel = null;
            switch (export)
            {
                case UStaticMesh st:
                {
                    guid = st.LightingGuid;
                    if (Options.TryGetModel(guid, out addedModel))
                    {
                        addedModel.AddInstance(t);
                    }
                    else if (st.TryConvert(out var mesh))
                    {
                        addedModel = new StaticModel(st, mesh, t);
                        Options.Models[guid] = addedModel;
                    }
                    break;
                }
                case USkeletalMesh sk:
                {
                    guid = Guid.NewGuid();
                    if (!Options.Models.ContainsKey(guid) && sk.TryConvert(out var mesh))
                    {
                        addedModel = new SkeletalModel(sk, mesh, t);
                        Options.Models[guid] = addedModel;
                    }
                    break;
                }
            }

            if (addedModel == null)
                throw new ArgumentException("Unknown model type");

            addedModel.IsProp = true;
            if (notifyClass.TryGetValue(out UObject skeletalMeshPropAnimation, "SkeletalMeshPropAnimation", "Animation", "AnimToPlay"))
                Animate(skeletalMeshPropAnimation, guid);
            if (notifyClass.TryGetValue(out FName socketName, "SocketName"))
            {
                t = Transform.Identity;
                if (notifyClass.TryGetValue(out FVector location, "LocationOffset", "Location"))
                    t.Position = location * Constants.SCALE_DOWN_RATIO;
                if (notifyClass.TryGetValue(out FRotator rotation, "RotationOffset", "Rotation"))
                    t.Rotation = rotation.Quaternion();
                if (notifyClass.TryGetValue(out FVector scale, "Scale"))
                    t.Scale = scale;

                var s = new Socket($"ANIM_{addedModel.Name}", socketName, t, true);
                model.Sockets.Add(s);
                addedModel.Attachments.Attach(model, addedModel.GetTransform(), s,
                    new SocketAttachementInfo { Guid = guid, Instance = addedModel.SelectedInstance });
            }
        }
        }

        Options.Tracker.IsPaused = false;
        Options.Tracker.SafeSetMaxElapsedTime(animation.TotalElapsedTime);
    }

    public void Setup()
    {
        _skybox.Setup();
        _grid.Setup();

        _shader = new Shader();
        _outline = new Shader("outline");
        _light = new Shader("light");
        _bone = new Shader("bone");
        _collision = new Shader("collision", "bone");

        Picking.Setup();
        Options.SetupModelsAndLights();
    }

    public Shader GetMainShader()
    {
        return _shader;
    }

    public void Render()
    {
        if (_disposed || _shader == null || _outline == null || _light == null || _bone == null || _collision == null)
            return;

        var viewMatrix = CameraOp.GetViewMatrix();
        var projMatrix = CameraOp.GetProjectionMatrix();

        if (ShowSkybox) _skybox.Render(viewMatrix, projMatrix);
        if (ShowGrid) _grid.Render(viewMatrix, projMatrix, CameraOp.Near, CameraOp.Far);

        _shader.Render(viewMatrix, CameraOp.Position, projMatrix);
        for (int i = 0; i < 5; i++)
            _shader.SetUniform($"bVertexColors[{i}]", i == (int) Color);

        foreach (var model in Options.Models.Values)
        {
            if (!model.IsVisible) continue;
            model.Render(_shader, Color == VertexColor.TextureCoordinates ? Options.Icons["checker"] : null);
        }

        {
            var uNumLights = Math.Min(Options.Lights.Count, 100);
            _shader.SetUniform("uNumLights", ShowLights ? uNumLights : 0);

            if (ShowLights)
                for (int i = 0; i < uNumLights; i++)
                    Options.Lights[i].Render(i, _shader);

            _light.Render(viewMatrix, projMatrix);
            for (int i = 0; i < uNumLights; i++)
                Options.Lights[i].Render(_light);
        }

        if (Options.TryGetModel(out var selected) && selected.IsVisible)
        {
            if (IsSkeletonTreeOpen && selected is SkeletalModel skeletalModel)
            {
                _bone.Render(viewMatrix, projMatrix);
                skeletalModel.RenderBones(_bone);
            }
            else if (selected.ShowCollisions)
            {
                _collision.Render(viewMatrix, projMatrix);
                selected.RenderCollision(_collision);
            }

            _outline.Render(viewMatrix, CameraOp.Position, projMatrix);
            selected.Render(_outline, Color == VertexColor.TextureCoordinates ? Options.Icons["checker"] : null, true);
        }

        if (IsSkeletonTreeOpen && !(Options.TryGetModel(out var sel) && sel is SkeletalModel))
        {
            var drewAnyBones = false;
            foreach (var animation in Options.Animations)
            {
                foreach (var guid in animation.AttachedModels)
                {
                    if (!Options.TryGetModel(guid, out var m) || m is not SkeletalModel animModel || !animModel.IsVisible) continue;
                    if (!drewAnyBones)
                    {
                        _bone.Render(viewMatrix, projMatrix);
                        drewAnyBones = true;
                    }
                    animModel.RenderBones(_bone);
                }
            }

            if (!drewAnyBones)
            {
                foreach (var model in Options.Models.Values)
                {
                    if (model is not SkeletalModel skel || !skel.IsVisible) continue;
                    _bone.Render(viewMatrix, projMatrix);
                    skel.RenderBones(_bone);
                }
            }
        }

        Picking.Render(viewMatrix, projMatrix, Options.Models);
    }

    public void Update(IRendererWindow wnd, float deltaSeconds)
    {
        if (_disposed)
            return;

        if (Options.Animations.Count > 0) Options.Tracker.Update(deltaSeconds);
        foreach (var animation in Options.Animations)
        {
            animation.TimeCalculation(Options.Tracker.ElapsedTime);
            foreach (var guid in animation.AttachedModels)
            {
                if (!Options.TryGetModel(guid, out var m) || m is not SkeletalModel skeletalModel) continue;
                skeletalModel.Skeleton.UpdateAnimationMatrices(animation, AnimateWithRotationOnly);
            }
        }

        {
            foreach (var model in Options.Models.Values)
            {
                model.Update(Options);
            }

            if (IsSkeletonTreeOpen)
            {
                if (Options.TryGetModel(out var selected) && selected is SkeletalModel { IsVisible: true } selSkeletal)
                {
                    selSkeletal.Skeleton.UpdateVertices();
                }

                foreach (var animation in Options.Animations)
                {
                    foreach (var guid in animation.AttachedModels)
                    {
                        if (!Options.TryGetModel(guid, out var m) || m is not SkeletalModel animModel || !animModel.IsVisible) continue;
                        if (Options.SelectedModel == guid) continue;
                        animModel.Skeleton.UpdateVertices();
                    }
                }
            }
        }

        CameraOp.Modify(wnd.KeyboardState, deltaSeconds);

        if (wnd.KeyboardState.IsKeyPressed(Keys.Z) &&
            Options.TryGetModel(out var selectedModel) &&
            selectedModel is SkeletalModel)
        {
            Options.RemoveAnimations();
            Options.AnimateMesh(true);
        }

        if (wnd.KeyboardState.IsKeyPressed(Keys.Delete))
            Options.RemoveModel(Options.SelectedModel);
        if (wnd.KeyboardState.IsKeyPressed(Keys.Escape))
            wnd.WindowShouldClose(true, true);
    }

    private void LoadStaticMesh(UStaticMesh original, ENaniteMeshFormat naniteFormat = ENaniteMeshFormat.NoNanite)
    {
        var guid = original.LightingGuid;
        if (Options.TryGetModel(guid, out var model))
        {
            model.AddInstance(Transform.Identity);
            model.SetupInstances();
            return;
        }

        if (!original.TryConvert(out var mesh, EMeshQuality.All, naniteFormat))
        {

            Log.Warning("StaticMesh {Name} could not be rendered. The CUE4Parse type table does not know " +
                        "every struct this asset uses — see the 'Could not read StaticMesh' warning above for the " +
                        "exact deserialization position. The same file loads in FModel only because it ships a " +
                        "CUE4Parse fork with extra game-specific struct patches.",
                        original.Name);
            return;
        }

        var staticModel = new StaticModel(original, mesh);
        ApplyFallbackTexturesStatic(staticModel, original);
        Options.Models[guid] = staticModel;
        Options.SelectModel(guid);
    }

    private void LoadSkeletalMesh(USkeletalMesh original)
    {
        var guid = new FGuid((uint) original.GetFullName().GetHashCode());
        if (Options.Models.ContainsKey(guid)) return;

        var maxLodHint = UserSettings.Default.MaxSkeletalMeshLodLevel;
        SkeletalMeshDto? mesh = null;
        SkeletalModel? skeletalModel = null;
        Exception? lastError = null;

        for (var attempt = 0; attempt <= 4; attempt++)
        {
            try
            {
                if (maxLodHint >= 0)
                {

                }

                if (!original.TryConvert(out mesh))
                {

                    if (original.LODModels == null || original.LODModels.Length == 0)
                    {
                        Log.Warning("SkeletalMesh {Name} has no LODs — likely a deserialization failure " +
                                    "(custom struct or unsupported game property). See earlier 'Could not read' warnings.",
                                    original.Name);
                    }
                    else
                    {
                        Log.Warning("SkeletalMesh {Name} could not be converted (attempt {Attempt}).", original.Name, attempt + 1);
                    }
                    break;
                }
                skeletalModel = new SkeletalModel(original, mesh!);
                lastError = null;
                break;
            }
            catch (OutOfMemoryException oom)
            {
                lastError = oom;

                mesh = null;
                skeletalModel = null;
                GC.Collect();
                GC.WaitForPendingFinalizers();
                GC.Collect();

                Log.Warning(oom, "Out of memory while loading SkeletalMesh {Name}, dropping higher LODs (attempt {Attempt})",
                    original.Name, attempt + 1);
            }
            catch (Exception ex)
            {
                lastError = ex;
                Log.Error(ex, "Failed to load SkeletalMesh {Name}", original.Name);
                break;
            }
        }

        if (skeletalModel == null || mesh == null)
        {
            if (lastError is OutOfMemoryException)
            {
                Log.Error("Could not load SkeletalMesh {Name}: out of memory even after dropping high-detail LODs. " +
                          "Try closing other applications or use a lower-LOD asset.", original.Name);
            }
            return;
        }

        ApplyFallbackTextures(skeletalModel, original);

        Options.Models[guid] = skeletalModel;
        Options.SelectModel(guid);
    }

    private void ApplyFallbackTextures(SkeletalModel model, USkeletalMesh original)
    {
        if (SnooperContext.Provider == null) return;

        var hasUnresolvedMaterials = model.Materials.Any(m => m.Name == "" || m.Parameters.IsNull);
        if (!hasUnresolvedMaterials) return;

        var meshPath = original.GetPathName();
        var packagePath = meshPath.Contains('.') ? meshPath[..meshPath.LastIndexOf('.')] : meshPath;

        for (int m = 0; m < model.Materials.Length && m < original.SkeletalMaterials.Length; m++)
        {
            var material = model.Materials[m];
            if (material.Name != "" && !material.Parameters.IsNull) continue;

            var slotName = original.SkeletalMaterials[m].MaterialSlotName?.Text;
            if (string.IsNullOrEmpty(slotName)) continue;

            TryResolveMaterialFromGameFiles(material, original.SkeletalMaterials[m].MaterialInterface?.ResolvedObject, slotName, packagePath);
        }

        hasUnresolvedMaterials = model.Materials.Any(m => m.Name == "" || m.Parameters.IsNull);
        if (!hasUnresolvedMaterials) return;

        ResolveVfsAndLoadTextures(packagePath, original.Name, out var diffuseTextures, out var normalTextures, out var specTextures);

        var allSlotWords = new List<List<string>>();
        for (int i = 0; i < model.Materials.Length && i < original.SkeletalMaterials.Length; i++)
        {
            var sn = original.SkeletalMaterials[i].MaterialSlotName?.Text;
            if (string.IsNullOrEmpty(sn)) continue;
            allSlotWords.Add(sn.Split(new[] { ' ', '_', '-' }, StringSplitOptions.RemoveEmptyEntries)
                .Where(w => w.Length > 2 && !CommonTextureSuffixes.Contains(w)).ToList());
        }
        var wordCounts = allSlotWords.SelectMany(w => w).GroupBy(w => w, StringComparer.OrdinalIgnoreCase)
            .ToDictionary(g => g.Key, g => g.Count(), StringComparer.OrdinalIgnoreCase);
        var genericWords = new HashSet<string>(wordCounts.Where(kv => kv.Value >= 3).Select(kv => kv.Key),
            StringComparer.OrdinalIgnoreCase);

        var meshWords = original.Name.Split(new[] { ' ', '_', '-' }, StringSplitOptions.RemoveEmptyEntries)
            .Where(w => w.Length > 2 && !CommonTextureSuffixes.Contains(w) && !genericWords.Contains(w))
            .ToList();

        const int placeholderCandidateLimit = 200;

        for (int m = 0; m < model.Materials.Length && m < original.SkeletalMaterials.Length; m++)
        {
            var material = model.Materials[m];
            if (material.Name != "" && !material.Parameters.IsNull) continue;

            var slotName = original.SkeletalMaterials[m].MaterialSlotName?.Text;
            if (string.IsNullOrEmpty(slotName)) continue;

            var isPlaceholderSlot = PlaceholderSlotNames.Contains(slotName);
            if (isPlaceholderSlot && (diffuseTextures.Count + normalTextures.Count) > placeholderCandidateLimit)
            {
                Log.Information($"  Slot '{slotName}': skipped (placeholder material + too many candidates)");
                continue;
            }

            var slotWords = isPlaceholderSlot
                ? meshWords
                : slotName.Split(new[] { ' ', '_', '-' }, StringSplitOptions.RemoveEmptyEntries)
                    .Where(w => w.Length > 2)
                    .ToList();

            var isGenericSlot = !isPlaceholderSlot && slotWords.Any(w => genericWords.Contains(w));
            var specificSlotWords = slotWords
                .Where(w => IsSpecificTextureWord(w) && !genericWords.Contains(w))
                .ToList();
            var lookupWords = specificSlotWords.Count > 0 ? specificSlotWords : slotWords;
            var useGlobalFallback = diffuseTextures.Count == 0 && normalTextures.Count == 0 && specTextures.Count == 0;
            var requireSlotMatch = useGlobalFallback && isGenericSlot && specificSlotWords.Count > 0;
            var slotDiffuseTextures = useGlobalFallback ? CollectGlobalTextureCandidates(lookupWords, IsDiffuseName) : diffuseTextures;
            var slotNormalTextures = useGlobalFallback ? CollectGlobalTextureCandidates(lookupWords, IsNormalName) : normalTextures;
            var slotSpecTextures = useGlobalFallback ? CollectGlobalTextureCandidates(lookupWords, IsSpecularMaskName) : specTextures;

            UTexture2D? bestDiffuse = FindBestTexture(slotName, slotWords, slotDiffuseTextures, packagePath, meshWords, isGenericSlot, requireSlotMatch, useGlobalFallback, genericWords);
            UTexture2D? bestNormal = FindBestTexture(slotName, slotWords, slotNormalTextures, packagePath, meshWords, isGenericSlot, requireSlotMatch, useGlobalFallback, genericWords);
            UTexture2D? bestSpec = FindBestTexture(slotName, slotWords, slotSpecTextures, packagePath, meshWords, isGenericSlot, requireSlotMatch, useGlobalFallback, genericWords);

            if (bestDiffuse != null || bestNormal != null || bestSpec != null)
            {
                material.Name = slotName;
                material.Path = $"Fallback:{slotName}";
                material.UsedFallbackTextureAssignment = true;

                material.Parameters.ShadingModel = EMaterialShadingModel.MSM_DefaultLit;
                if (slotName.Contains("Hair", StringComparison.OrdinalIgnoreCase) ||
                    slotName.Contains("Stocking", StringComparison.OrdinalIgnoreCase) ||
                    slotName.Contains("Transparent", StringComparison.OrdinalIgnoreCase) ||
                    slotName.Contains("Glass", StringComparison.OrdinalIgnoreCase))
                {
                    material.Parameters.BlendMode = EBlendMode.BLEND_Masked;
                }

                if (bestDiffuse != null)
                {
                    material.Parameters.Textures[CMaterialParams2.FallbackDiffuse] = new ResolvedLoadedObject(bestDiffuse);
                }
                if (bestNormal != null)
                {
                    material.Parameters.Textures[CMaterialParams2.FallbackNormals] = new ResolvedLoadedObject(bestNormal);
                }

                if (bestSpec != null)
                {
                    material.Parameters.Textures[CMaterialParams2.FallbackSpecularMasks] = new ResolvedLoadedObject(bestSpec);
                }

                Log.Information($"  Slot '{slotName}': diffuse={bestDiffuse?.Name ?? "None"}, normal={bestNormal?.Name ?? "None"}, spec={bestSpec?.Name ?? "None"}");
            }
        }
    }

    private bool TryResolveMaterialFromGameFiles(Material material, ResolvedObject? materialRef, string slotName, string meshPackagePath)
    {
        if (materialRef != null && materialRef.TryLoad(out var loaded) && loaded is UMaterialInterface directMaterial)
        {
            material.SwapMaterial(directMaterial);
            Log.Information($"  Slot '{slotName}': resolved material={directMaterial.Name}");
            return !material.Parameters.IsNull;
        }

        var resolved = FindMaterialInMountedFiles(materialRef, slotName, meshPackagePath);
        if (resolved == null) return false;

        material.SwapMaterial(resolved);
        Log.Information($"  Slot '{slotName}': resolved material={resolved.Name}");
        return !material.Parameters.IsNull;
    }

    private UMaterialInterface? FindMaterialInMountedFiles(ResolvedObject? materialRef, string slotName, string meshPackagePath)
    {
        var cacheKey = $"{materialRef?.GetPathName() ?? string.Empty}|{slotName}";
        if (_materialResolveCache.TryGetValue(cacheKey, out var cached)) return cached;

        UMaterialInterface? resolved = null;
        if (materialRef != null)
        {
            resolved = TryLoadMaterialPath(materialRef.GetPathName());
            if (resolved != null)
            {
                _materialResolveCache[cacheKey] = resolved;
                return resolved;
            }
        }

        if (SnooperContext.Provider == null)
        {
            _materialResolveCache[cacheKey] = null;
            return null;
        }

        var names = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        AddCandidateName(names, slotName);
        AddCandidateName(names, materialRef?.Name.Text);

        var candidates = new List<(string path, string name, int score)>();
        foreach (var kvp in SnooperContext.Provider.Files)
        {
            if (!kvp.Key.EndsWith(".uasset", StringComparison.OrdinalIgnoreCase)) continue;

            var path = kvp.Key.SubstringBeforeLast('.');
            var name = path.SubstringAfterLast('/');
            if (!names.Contains(name)) continue;

            candidates.Add((path, name, ScoreMaterialCandidate(path, name, slotName, meshPackagePath)));
        }

        if (candidates.Count == 0)
        {

            var trimmedNames = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
            foreach (var baseName in names)
            {
                var current = baseName;
                for (var depth = 1; depth <= 2; depth++)
                {
                    var cut = current.LastIndexOf('_');
                    if (cut < 4) break;
                    current = current[..cut];
                    trimmedNames.TryAdd(current, depth);
                }
            }

            if (trimmedNames.Count > 0)
            {
                foreach (var kvp in SnooperContext.Provider.Files)
                {
                    if (!kvp.Key.EndsWith(".uasset", StringComparison.OrdinalIgnoreCase)) continue;

                    var path = kvp.Key.SubstringBeforeLast('.');
                    var name = path.SubstringAfterLast('/');
                    if (!trimmedNames.TryGetValue(name, out var depth)) continue;

                    candidates.Add((path, name, ScoreMaterialCandidate(path, name, slotName, meshPackagePath) - depth * 10));
                }

                for (var i = 0; i < candidates.Count; i++)
                {
                    var stem = GetMaterialStem(candidates[i].name);
                    var materialDir = candidates[i].path.Contains('/') ? candidates[i].path[..candidates[i].path.LastIndexOf('/')] : string.Empty;
                    var rootDir = materialDir.EndsWith("/Materials", StringComparison.OrdinalIgnoreCase) && materialDir.Contains('/')
                        ? materialDir[..materialDir.LastIndexOf('/')]
                        : materialDir;
                    if (rootDir.Length == 0 || stem.Length < 4) continue;

                    var affinity = 0;
                    foreach (var kvp in SnooperContext.Provider.Files)
                    {
                        if (!kvp.Key.StartsWith(rootDir + "/", StringComparison.OrdinalIgnoreCase)) continue;
                        if (!kvp.Key.EndsWith(".uasset", StringComparison.OrdinalIgnoreCase)) continue;

                        var fileName = kvp.Key.SubstringAfterLast('/').SubstringBefore('.');
                        if (fileName.Contains(stem, StringComparison.OrdinalIgnoreCase)) affinity++;
                    }

                    candidates[i] = (candidates[i].path, candidates[i].name, candidates[i].score + Math.Min(affinity, 10) * 3);
                }
            }
        }

        foreach (var candidate in candidates.OrderByDescending(c => c.score).Take(20))
        {
            resolved = TryLoadMaterialPath(candidate.path);
            if (resolved != null) break;
        }

        _materialResolveCache[cacheKey] = resolved;
        return resolved;
    }

    private UMaterialInterface? TryLoadMaterialPath(string? objectPath)
    {
        if (SnooperContext.Provider == null || string.IsNullOrWhiteSpace(objectPath)) return null;

        var normalized = NormalizeObjectPath(objectPath);
        if (string.IsNullOrEmpty(normalized)) return null;

        try
        {
            if (SnooperContext.Provider.TryLoadPackageObject(normalized, out UMaterialInterface material))
                return material;
        }
        catch
        {

        }

        return null;
    }

    private static string NormalizeObjectPath(string objectPath)
    {
        var path = objectPath.Trim().Trim('\'').Replace('\\', '/');
        var classSeparator = path.LastIndexOf(' ');
        if (classSeparator >= 0) path = path[(classSeparator + 1)..].Trim('\'');
        if (path.EndsWith(".uasset", StringComparison.OrdinalIgnoreCase)) path = path[..^7];
        if (path.Contains('.')) path = path[..path.LastIndexOf('.')];
        return path.TrimStart('/');
    }

    private static void AddCandidateName(HashSet<string> names, string? name)
    {
        if (string.IsNullOrWhiteSpace(name)) return;

        var cleaned = name.Replace('\\', '/').SubstringAfterLast('/').SubstringBefore('.');
        if (!string.IsNullOrWhiteSpace(cleaned) && !cleaned.Equals("None", StringComparison.OrdinalIgnoreCase))
            names.Add(cleaned);
    }

    private static string GetMaterialStem(string materialName)
    {
        if (materialName.StartsWith("MI_", StringComparison.OrdinalIgnoreCase)) return materialName[3..];
        if (materialName.StartsWith("M_", StringComparison.OrdinalIgnoreCase)) return materialName[2..];
        return materialName;
    }

    private static int ScoreMaterialCandidate(string path, string name, string slotName, string meshPackagePath)
    {
        var score = name.Equals(slotName, StringComparison.OrdinalIgnoreCase) ? 100 : 0;
        var meshDir = meshPackagePath.Contains('/') ? meshPackagePath[..meshPackagePath.LastIndexOf('/')] : string.Empty;
        score += SharedPathPrefix(meshDir, path.SubstringBeforeLast('/')).Count(c => c == '/') * 2;

        foreach (var word in slotName.Split(new[] { ' ', '_', '-' }, StringSplitOptions.RemoveEmptyEntries).Where(IsSpecificTextureWord))
        {
            if (name.Contains(word, StringComparison.OrdinalIgnoreCase)) score += 30;
            if (path.Contains(word, StringComparison.OrdinalIgnoreCase)) score += 5;
        }

        return score;
    }

    private List<(string path, string name)> CollectGlobalTextureCandidates(List<string> slotWords, Func<string, bool> textureNamePredicate)
    {
        if (SnooperContext.Provider == null) return new List<(string path, string name)>();

        var specificWords = slotWords.Where(IsSpecificTextureWord).Distinct(StringComparer.OrdinalIgnoreCase).ToArray();
        if (specificWords.Length == 0) return new List<(string path, string name)>();

        var candidates = new List<(string path, string name)>();
        foreach (var kvp in SnooperContext.Provider.Files)
        {
            if (!kvp.Key.EndsWith(".uasset", StringComparison.OrdinalIgnoreCase)) continue;

            var path = kvp.Key.SubstringBeforeLast('.');
            var name = path.SubstringAfterLast('/');
            if (!textureNamePredicate(name)) continue;
            if (!specificWords.Any(w => path.Contains(w, StringComparison.OrdinalIgnoreCase))) continue;

            candidates.Add((path, name));
            if (candidates.Count >= 400) break;
        }

        return candidates;
    }

    private static List<(string path, string name)> MergeTextureCandidates(
        List<(string path, string name)> primary,
        List<(string path, string name)> secondary)
    {
        if (primary.Count == 0) return secondary;
        if (secondary.Count == 0) return primary;

        var merged = new List<(string path, string name)>(primary.Count + secondary.Count);
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var candidate in primary.Concat(secondary))
        {
            if (seen.Add(candidate.path)) merged.Add(candidate);
        }

        return merged;
    }

    private static bool IsSpecificTextureWord(string word)
    {
        return word.Length > 2
               && !int.TryParse(word, out _)
               && !CommonTextureSuffixes.Contains(word)
               && !CommonMaterialSlotWords.Contains(word);
    }

    private bool ResolveVfsAndLoadTextures(
        string packagePath, string meshName,
        out List<(string path, string name)> diffuseCandidates,
        out List<(string path, string name)> normalCandidates,
        out List<(string path, string name)> specularMaskCandidates)
    {
        diffuseCandidates = new();
        normalCandidates = new();
        specularMaskCandidates = new();

        IVfsReader? modVfs = null;
        if (SnooperContext.Provider!.TryGetGameFile(packagePath, out var meshFile) && meshFile is VfsEntry vfsEntry)
            modVfs = vfsEntry.Vfs;

        IVfsReader? searchVfs = modVfs;
        var fixedMeshPath = SnooperContext.Provider.FixPath(packagePath);
        if (modVfs != null && SnooperContext.Provider is CUE4Parse.FileProvider.Vfs.IVfsFileProvider vfsProvider)
        {
            foreach (var vfs in vfsProvider.MountedVfs)
            {
                if (ReferenceEquals(vfs, modVfs)) continue;
                if (vfs.Files.ContainsKey(fixedMeshPath))
                {
                    Log.Information($"FallbackTextures: mesh {meshName} exists in base game VFS '{vfs.Name}', using base game textures");
                    searchVfs = vfs;
                    break;
                }
            }
        }

        var meshDirectory = fixedMeshPath.Contains('/') ? fixedMeshPath[..fixedMeshPath.LastIndexOf('/')] : "";
        var isBaseGameVfs = searchVfs != null && searchVfs != modVfs;

        foreach (var kvp in SnooperContext.Provider.Files)
        {
            if (!kvp.Key.EndsWith(".uasset")) continue;
            if (searchVfs != null)
            {
                if (kvp.Value is VfsEntry entry && entry.Vfs != searchVfs) continue;
                if (isBaseGameVfs && !kvp.Key.StartsWith(meshDirectory, StringComparison.OrdinalIgnoreCase)) continue;
            }
            else
            {
                if (!kvp.Key.StartsWith(meshDirectory, StringComparison.OrdinalIgnoreCase)) continue;
            }

            var path = kvp.Key.SubstringBeforeLast('.');
            var name = path.SubstringAfterLast('/');

            if (IsDiffuseName(name))
                diffuseCandidates.Add((path, name));
            else if (IsNormalName(name))
                normalCandidates.Add((path, name));
            else if (IsSpecularMaskName(name))
                specularMaskCandidates.Add((path, name));
        }

        if (diffuseCandidates.Count == 0 && normalCandidates.Count == 0 && specularMaskCandidates.Count == 0) return false;
        Log.Information($"FallbackTextures: found {diffuseCandidates.Count} diffuse, {normalCandidates.Count} normal, {specularMaskCandidates.Count} spec candidates for mesh {meshName}");
        return true;
    }

    private static bool IsDiffuseName(string name)
    {
        return name.Contains("BaseColor", StringComparison.OrdinalIgnoreCase)
            || name.Contains("Base_Color", StringComparison.OrdinalIgnoreCase)
            || name.Contains("Diffuse", StringComparison.OrdinalIgnoreCase)
            || name.Contains("_BC", StringComparison.OrdinalIgnoreCase)
            || name.Contains("Albedo", StringComparison.OrdinalIgnoreCase)
            || name.EndsWith("_C", StringComparison.OrdinalIgnoreCase)
            || name.EndsWith("_D", StringComparison.OrdinalIgnoreCase)
            || (name.Contains("Color", StringComparison.OrdinalIgnoreCase)
                && !name.Contains("ORM", StringComparison.OrdinalIgnoreCase)
                && !name.Contains("SSS", StringComparison.OrdinalIgnoreCase)
                && !name.Contains("Mask", StringComparison.OrdinalIgnoreCase)
                && !name.Contains("ID", StringComparison.OrdinalIgnoreCase));
    }

    private static bool IsNormalName(string name)
    {
        return name.Contains("Normal", StringComparison.OrdinalIgnoreCase)
            || name.Contains("_N_", StringComparison.OrdinalIgnoreCase)
            || name.Contains("_NM", StringComparison.OrdinalIgnoreCase)
            || name.Contains("_NRM", StringComparison.OrdinalIgnoreCase);
    }

    private static bool IsSpecularMaskName(string name)
    {
        return HasTextureToken(name, "ORM", "RMA", "MRA", "ARM", "MRO", "MRAO", "MRAS", "MROA", "SRO", "SRM", "PAK")
            || name.Contains("OcclusionRoughnessMetallic", StringComparison.OrdinalIgnoreCase)
            || (name.Contains("Rough", StringComparison.OrdinalIgnoreCase)
                && (name.Contains("Metal", StringComparison.OrdinalIgnoreCase) || name.Contains("Occ", StringComparison.OrdinalIgnoreCase)));
    }

    private static bool HasTextureToken(string name, params string[] tokens)
    {
        foreach (var part in name.Split(new[] { ' ', '_', '-', '.', '/', '\\' }, StringSplitOptions.RemoveEmptyEntries))
        {
            foreach (var token in tokens)
            {
                if (part.Equals(token, StringComparison.OrdinalIgnoreCase))
                    return true;

                if (part.Length > token.Length && part.EndsWith(token, StringComparison.OrdinalIgnoreCase) &&
                    (part.StartsWith("Tex", StringComparison.OrdinalIgnoreCase) ||
                     part.StartsWith("Texture", StringComparison.OrdinalIgnoreCase)))
                    return true;
            }
        }

        return false;
    }

    private static readonly HashSet<string> CommonTextureSuffixes = new(StringComparer.OrdinalIgnoreCase)
    {
        "BaseColor", "Base_Color", "Base", "Color", "Diffuse", "Albedo", "BC", "D",
        "Normal", "NM", "NRM", "N",
        "ORM", "AO", "Mask", "ID", "RMA", "MRA", "ARM", "MRO", "MRAO", "MRAS", "MROA", "SRO", "SRM", "PAK", "Pack",
        "Roughness", "Metallic", "Specular", "Emissive",
    };

    private static readonly HashSet<string> CommonMaterialSlotWords = new(StringComparer.OrdinalIgnoreCase)
    {
        "MI", "MAT", "M", "CH", "SK", "SKM", "SM", "1P", "2P", "3P", "LOD", "Mesh",
    };

    private static readonly HashSet<string> PlaceholderSlotNames = new(StringComparer.OrdinalIgnoreCase)
    {
        "lambert1", "initialShadingGroup", "lambert",
        "phong1", "phongE1", "blinn1",
        "standardSurface1", "usdPreviewSurface",
    };

    private UTexture2D? FindBestTexture(
        string slotName,
        List<string> slotWords,
        List<(string path, string name)> candidates,
        string meshGamePath,
        List<string> meshWords,
        bool isGenericSlot,
        bool requireSlotMatch = false,
        bool requireSemanticMatch = false,
        HashSet<string>? ignoredSpecificSlotWords = null)
    {
        if (candidates.Count == 0) return null;

        var meshDir = "";
        if (SnooperContext.Provider != null)
        {
            var fixedPath = SnooperContext.Provider.FixPath(meshGamePath);
            meshDir = fixedPath.Contains('/') ? fixedPath[..fixedPath.LastIndexOf('/')] : "";
        }

        var bestScore = int.MinValue;
        var bestHasSemanticMatch = false;
        string? bestPath = null;

        foreach (var (path, name) in candidates)
        {
            var score = 0;
            var candidateHasSemanticMatch = false;
            var texDir = path.SubstringBeforeLast('/');

            var sharedPrefix = SharedPathPrefix(meshDir, texDir);
            var sharedDepth = sharedPrefix.Count(c => c == '/');
            score += sharedDepth * 10;

            var texWords = name.Split(new[] { ' ', '_', '-' }, StringSplitOptions.RemoveEmptyEntries)
                .Where(w => w.Length > 2 && !CommonTextureSuffixes.Contains(w))
                .ToList();

            var texDirName = texDir.SubstringAfterLast('/');
            var matchedSpecificSlotWord = false;

            foreach (var slotWord in slotWords)
            {
                var matchedSlotWord = false;
                if (name.Contains(slotWord, StringComparison.OrdinalIgnoreCase))
                {
                    score += 25;
                    matchedSlotWord = true;
                }
                if (texDir.Contains(slotWord, StringComparison.OrdinalIgnoreCase))
                {
                    score += 5;
                    matchedSlotWord = true;
                }
                if (texDirName.Contains(slotWord, StringComparison.OrdinalIgnoreCase))
                {
                    score += 15;
                    matchedSlotWord = true;
                }

                foreach (var texWord in texWords)
                {
                    if (slotWord.Contains(texWord, StringComparison.OrdinalIgnoreCase) ||
                        texWord.Contains(slotWord, StringComparison.OrdinalIgnoreCase))
                    {
                        score += 20;
                        matchedSlotWord = true;
                        break;
                    }
                }

                if (matchedSlotWord)
                    candidateHasSemanticMatch = true;

                if (matchedSlotWord && IsSpecificTextureWord(slotWord) &&
                    (ignoredSpecificSlotWords == null || !ignoredSpecificSlotWords.Contains(slotWord)))
                    matchedSpecificSlotWord = true;
            }

            if (requireSlotMatch && !matchedSpecificSlotWord) continue;

            foreach (var texWord in texWords)
            {
                var matchesSlot = slotName.Contains(texWord, StringComparison.OrdinalIgnoreCase);
                var matchesMesh = !isGenericSlot && meshWords.Any(mw =>
                    mw.Contains(texWord, StringComparison.OrdinalIgnoreCase) ||
                    texWord.Contains(mw, StringComparison.OrdinalIgnoreCase));

                if (matchesSlot)
                    continue;

                if (matchesMesh)
                {

                    candidateHasSemanticMatch = true;
                    score += 15;
                }
                else
                {
                    score -= texWord.Length * 3;
                }
            }

            if (score > bestScore)
            {
                bestScore = score;
                bestHasSemanticMatch = candidateHasSemanticMatch;
                bestPath = path;
            }
        }

        if (bestPath == null || (requireSemanticMatch && !bestHasSemanticMatch)) return null;

        try
        {
            if (SnooperContext.Provider?.TryLoadPackageObject(bestPath, out UTexture2D texture) == true)
                return texture;
        }
        catch {  }

        return null;
    }

    private static string SharedPathPrefix(string a, string b)
    {
        var minLen = Math.Min(a.Length, b.Length);
        var i = 0;
        while (i < minLen && a[i] == b[i]) i++;

        while (i > 0 && a[i - 1] != '/') i--;
        return a[..i];
    }

    private void ApplyFallbackTexturesStatic(StaticModel model, UStaticMesh original)
    {
        if (SnooperContext.Provider == null) return;

        var hasUnresolvedMaterials = model.Materials.Any(m => m.Name == "" || m.Parameters.IsNull);
        if (!hasUnresolvedMaterials) return;

        var meshPath = original.GetPathName();
        var packagePath = meshPath.Contains('.') ? meshPath[..meshPath.LastIndexOf('.')] : meshPath;

        var slotNames = original.StaticMaterials?
            .Select(sm => sm.MaterialSlotName?.Text)
            .ToArray() ?? Array.Empty<string>();

        for (int m = 0; m < model.Materials.Length && m < slotNames.Length && original.StaticMaterials != null; m++)
        {
            var material = model.Materials[m];
            if (material.Name != "" && !material.Parameters.IsNull) continue;

            var slotName = slotNames[m];
            if (string.IsNullOrEmpty(slotName)) continue;

            TryResolveMaterialFromGameFiles(material, original.StaticMaterials[m].MaterialInterface?.ResolvedObject, slotName, packagePath);
        }

        hasUnresolvedMaterials = model.Materials.Any(m => m.Name == "" || m.Parameters.IsNull);
        if (!hasUnresolvedMaterials) return;

        ResolveVfsAndLoadTextures(packagePath, original.Name, out var diffuseTextures, out var normalTextures, out var specTextures);

        var meshWords = original.Name.Split(new[] { ' ', '_', '-' }, StringSplitOptions.RemoveEmptyEntries)
            .Where(w => w.Length > 2 && !CommonTextureSuffixes.Contains(w))
            .ToList();

        const int placeholderCandidateLimit = 200;

        for (int m = 0; m < model.Materials.Length && m < slotNames.Length; m++)
        {
            var material = model.Materials[m];
            if (material.Name != "" && !material.Parameters.IsNull) continue;

            var slotName = slotNames[m];
            if (string.IsNullOrEmpty(slotName)) continue;

            var isPlaceholderSlot = PlaceholderSlotNames.Contains(slotName);
            if (isPlaceholderSlot && (diffuseTextures.Count + normalTextures.Count) > placeholderCandidateLimit)
            {
                Log.Information($"  Slot '{slotName}': skipped (placeholder material + too many candidates)");
                continue;
            }

            var slotWords = isPlaceholderSlot
                ? meshWords
                : slotName.Split(new[] { ' ', '_', '-' }, StringSplitOptions.RemoveEmptyEntries)
                    .Where(w => w.Length > 2)
                    .ToList();

            var useGlobalFallback = diffuseTextures.Count == 0 && normalTextures.Count == 0 && specTextures.Count == 0;
            var slotDiffuseTextures = useGlobalFallback ? CollectGlobalTextureCandidates(slotWords, IsDiffuseName) : diffuseTextures;
            var slotNormalTextures = useGlobalFallback ? CollectGlobalTextureCandidates(slotWords, IsNormalName) : normalTextures;
            var slotSpecTextures = useGlobalFallback ? CollectGlobalTextureCandidates(slotWords, IsSpecularMaskName) : specTextures;

            UTexture2D? bestDiffuse = FindBestTexture(slotName, slotWords, slotDiffuseTextures, packagePath, meshWords, false, false, useGlobalFallback);
            UTexture2D? bestNormal = FindBestTexture(slotName, slotWords, slotNormalTextures, packagePath, meshWords, false, false, useGlobalFallback);
            UTexture2D? bestSpec = FindBestTexture(slotName, slotWords, slotSpecTextures, packagePath, meshWords, false, false, useGlobalFallback);

            if (bestDiffuse != null || bestNormal != null || bestSpec != null)
            {
                material.Name = slotName;
                material.Path = $"Fallback:{slotName}";

                material.Parameters.ShadingModel = EMaterialShadingModel.MSM_DefaultLit;
                if (slotName.Contains("Hair", StringComparison.OrdinalIgnoreCase) ||
                    slotName.Contains("Stocking", StringComparison.OrdinalIgnoreCase) ||
                    slotName.Contains("Transparent", StringComparison.OrdinalIgnoreCase) ||
                    slotName.Contains("Glass", StringComparison.OrdinalIgnoreCase))
                {
                    material.Parameters.BlendMode = EBlendMode.BLEND_Masked;
                }

                if (bestDiffuse != null)
                {
                    material.Parameters.Textures[CMaterialParams2.FallbackDiffuse] = new ResolvedLoadedObject(bestDiffuse);
                }
                if (bestNormal != null)
                {
                    material.Parameters.Textures[CMaterialParams2.FallbackNormals] = new ResolvedLoadedObject(bestNormal);
                }

                if (bestSpec != null)
                {
                    material.Parameters.Textures[CMaterialParams2.FallbackSpecularMasks] = new ResolvedLoadedObject(bestSpec);
                }

                Log.Information($"  Slot '{slotName}': diffuse={bestDiffuse?.Name ?? "None"}, normal={bestNormal?.Name ?? "None"}, spec={bestSpec?.Name ?? "None"}");
            }
        }
    }

    private void LoadSkeleton(USkeleton original)
    {
        var guid = original.Guid;
        if (Options.Models.ContainsKey(guid) || !original.TryConvert(out _, out var box)) return;

        var fakeSkeletalModel = new SkeletalModel(original, box);
        Options.Models[guid] = fakeSkeletalModel;
        Options.SelectModel(guid);
        IsSkeletonTreeOpen = true;
    }

    private void LoadMaterialInstance(UMaterialInstance original)
    {

        var guid = new FGuid(0x12345678, 0x9ABCDEF0, 0x11111111, 0x22222222);

        if (Options.TryGetModel(guid, out var model))
        {
            model.Materials[0].SwapMaterial(original);
            model.Materials[0].Setup(Options, model.UvCount);
            return;
        }

        var mesh = CreateProceduralCube(original);
        Options.Models[guid] = new StaticModel(original, mesh);
        Options.SelectModel(guid);
    }

    private MeshLodDto<MeshVertex> CreateProceduralCube(UObject owner)
    {

        const float s = 50f;

        var vertices = new[]
        {

            new MeshVertex(new FVector(s, -s, -s), new FPackedNormal(new FVector(1, 0, 0)), new FPackedNormal(new FVector(0, 1, 0)), new FMeshUVFloat(0, 1)),
            new MeshVertex(new FVector(s, s, -s), new FPackedNormal(new FVector(1, 0, 0)), new FPackedNormal(new FVector(0, 1, 0)), new FMeshUVFloat(1, 1)),
            new MeshVertex(new FVector(s, s, s), new FPackedNormal(new FVector(1, 0, 0)), new FPackedNormal(new FVector(0, 1, 0)), new FMeshUVFloat(1, 0)),
            new MeshVertex(new FVector(s, -s, s), new FPackedNormal(new FVector(1, 0, 0)), new FPackedNormal(new FVector(0, 1, 0)), new FMeshUVFloat(0, 0)),

            new MeshVertex(new FVector(-s, s, -s), new FPackedNormal(new FVector(-1, 0, 0)), new FPackedNormal(new FVector(0, -1, 0)), new FMeshUVFloat(0, 1)),
            new MeshVertex(new FVector(-s, -s, -s), new FPackedNormal(new FVector(-1, 0, 0)), new FPackedNormal(new FVector(0, -1, 0)), new FMeshUVFloat(1, 1)),
            new MeshVertex(new FVector(-s, -s, s), new FPackedNormal(new FVector(-1, 0, 0)), new FPackedNormal(new FVector(0, -1, 0)), new FMeshUVFloat(1, 0)),
            new MeshVertex(new FVector(-s, s, s), new FPackedNormal(new FVector(-1, 0, 0)), new FPackedNormal(new FVector(0, -1, 0)), new FMeshUVFloat(0, 0)),

            new MeshVertex(new FVector(-s, s, -s), new FPackedNormal(new FVector(0, 1, 0)), new FPackedNormal(new FVector(1, 0, 0)), new FMeshUVFloat(0, 1)),
            new MeshVertex(new FVector(s, s, -s), new FPackedNormal(new FVector(0, 1, 0)), new FPackedNormal(new FVector(1, 0, 0)), new FMeshUVFloat(1, 1)),
            new MeshVertex(new FVector(s, s, s), new FPackedNormal(new FVector(0, 1, 0)), new FPackedNormal(new FVector(1, 0, 0)), new FMeshUVFloat(1, 0)),
            new MeshVertex(new FVector(-s, s, s), new FPackedNormal(new FVector(0, 1, 0)), new FPackedNormal(new FVector(1, 0, 0)), new FMeshUVFloat(0, 0)),

            new MeshVertex(new FVector(s, -s, -s), new FPackedNormal(new FVector(0, -1, 0)), new FPackedNormal(new FVector(-1, 0, 0)), new FMeshUVFloat(0, 1)),
            new MeshVertex(new FVector(-s, -s, -s), new FPackedNormal(new FVector(0, -1, 0)), new FPackedNormal(new FVector(-1, 0, 0)), new FMeshUVFloat(1, 1)),
            new MeshVertex(new FVector(-s, -s, s), new FPackedNormal(new FVector(0, -1, 0)), new FPackedNormal(new FVector(-1, 0, 0)), new FMeshUVFloat(1, 0)),
            new MeshVertex(new FVector(s, -s, s), new FPackedNormal(new FVector(0, -1, 0)), new FPackedNormal(new FVector(-1, 0, 0)), new FMeshUVFloat(0, 0)),

            new MeshVertex(new FVector(-s, -s, s), new FPackedNormal(new FVector(0, 0, 1)), new FPackedNormal(new FVector(1, 0, 0)), new FMeshUVFloat(0, 1)),
            new MeshVertex(new FVector(s, -s, s), new FPackedNormal(new FVector(0, 0, 1)), new FPackedNormal(new FVector(1, 0, 0)), new FMeshUVFloat(1, 1)),
            new MeshVertex(new FVector(s, s, s), new FPackedNormal(new FVector(0, 0, 1)), new FPackedNormal(new FVector(1, 0, 0)), new FMeshUVFloat(1, 0)),
            new MeshVertex(new FVector(-s, s, s), new FPackedNormal(new FVector(0, 0, 1)), new FPackedNormal(new FVector(1, 0, 0)), new FMeshUVFloat(0, 0)),

            new MeshVertex(new FVector(-s, s, -s), new FPackedNormal(new FVector(0, 0, -1)), new FPackedNormal(new FVector(1, 0, 0)), new FMeshUVFloat(0, 1)),
            new MeshVertex(new FVector(s, s, -s), new FPackedNormal(new FVector(0, 0, -1)), new FPackedNormal(new FVector(1, 0, 0)), new FMeshUVFloat(1, 1)),
            new MeshVertex(new FVector(s, -s, -s), new FPackedNormal(new FVector(0, 0, -1)), new FPackedNormal(new FVector(1, 0, 0)), new FMeshUVFloat(1, 0)),
            new MeshVertex(new FVector(-s, -s, -s), new FPackedNormal(new FVector(0, 0, -1)), new FPackedNormal(new FVector(1, 0, 0)), new FMeshUVFloat(0, 0)),
        };

        var indices = new uint[]
        {
            0, 1, 2, 0, 2, 3,
            4, 5, 6, 4, 6, 7,
            8, 9, 10, 8, 10, 11,
            12, 13, 14, 12, 14, 15,
            16, 17, 18, 16, 18, 19,
            20, 21, 22, 20, 22, 23
        };

        var meshLodOwner = new ProceduralMeshDto(owner);
        var meshLodType = typeof(MeshLodDto<MeshVertex>);
        var constructor = meshLodType.GetConstructors(
            System.Reflection.BindingFlags.Instance |
            System.Reflection.BindingFlags.Public |
            System.Reflection.BindingFlags.NonPublic)
            .OrderByDescending(c => c.GetParameters().Length)
            .First();

        var sections = new[] { new MeshSectionDto(index: 0, firstIndex: 0, numFaces: 12, castShadow: true) };
        var constructorParameters = constructor.GetParameters();
        var arguments = new object?[constructorParameters.Length];
        for (var i = 0; i < constructorParameters.Length; i++)
        {
            var parameter = constructorParameters[i];
            arguments[i] = parameter.Name switch
            {
                "owner" => meshLodOwner,
                "options" => UserSettings.Default.ExportOptions,
                "sourceLodIndex" => 0u,
                "indices" => indices,
                "vertices" => vertices,
                "sections" => sections,
                "extraUvs" or "extraUv" => Array.Empty<FMeshUVFloat[]>(),
                "vertexColors" => null,
                "screenSize" => 0.0f,
                "isTwoSided" => false,
                "isNanite" => false,
                _ => parameter.HasDefaultValue ? parameter.DefaultValue : null
            };
        }

        var meshLod = (MeshLodDto<MeshVertex>)constructor.Invoke(arguments);

        return meshLod;
    }

    private void LoadPaperSprite(UPaperSprite original)
    {
        if (!(original.BakedSourceTexture?.TryLoad(out UTexture2D texture) ?? false))
            return;

        var guid = texture.LightingGuid;
        if (Options.TryGetModel(guid, out var model))
        {
            model.AddInstance(Transform.Identity);
            model.SetupInstances();
            return;
        }

        Options.Models[guid] = new StaticModel(original, texture);
        Options.SelectModel(guid);
    }

    private void SetupCamera()
    {
        if (Options.TryGetModel(out var model))
            CameraOp.Setup(model.Box);
    }

    private void LoadWorld(CancellationToken cancellationToken, UWorld original, Transform transform)
    {
        CameraOp.Setup(new FBox(FVector.ZeroVector, new FVector(0, 10, 10)));
        if (original.PersistentLevel.Load<ULevel>() is not { } persistentLevel)
            return;

        if (persistentLevel.TryGetValue(out FSoftObjectPath runtimeCell, "WorldPartitionRuntimeCell") &&
            runtimeCell.TryLoad(out UObject worldPartition))
        {
            var position = worldPartition.GetOrDefault("Position", FVector.ZeroVector) * Constants.SCALE_DOWN_RATIO;
            var box = worldPartition.GetOrDefault("ContentBounds", new FBox(FVector.ZeroVector, FVector.OneVector));
            box *= MathF.Pow(Constants.SCALE_DOWN_RATIO, 2);
            CameraOp.Teleport(new Vector3(position.X, position.Z, position.Y), box, true);
        }

        var length = persistentLevel.Actors.Length;
        for (var i = 0; i < length; i++)
        {
            cancellationToken.ThrowIfCancellationRequested();

            if (persistentLevel.Actors[i].Load() is not { } actor || actor.ExportType is "LODActor")
                continue;

            SnooperContext.UpdateStatus($"{original.Name} ... {i}/{length}");
            WorldCamera(actor);
            WorldLight(actor);
            WorldMesh(actor, transform);
            AdditionalWorlds(actor, transform.Matrix, cancellationToken);
        }
        SnooperContext.UpdateStatus($"{original.Name} ... {length}/{length}");
    }

    private void LoadJunoWorld(CancellationToken cancellationToken, UBlueprintGeneratedClass original, Transform transform)
    {
        CameraOp.Setup(new FBox(FVector.ZeroVector, new FVector(0, 10, 10)));

        var length = 0;
        FPackageIndex[] allNodes = [];
        IPropertyHolder[] records = [];
        if (original.TryGetValue(out FPackageIndex simpleConstructionScript, "SimpleConstructionScript") &&
            simpleConstructionScript.TryLoad(out var scs) && scs.TryGetValue(out allNodes, "AllNodes"))
            length = allNodes.Length;
        else if (original.TryGetValue(out FPackageIndex inheritableComponentHandler, "InheritableComponentHandler") &&
            inheritableComponentHandler.TryLoad(out var ich) && ich.TryGetValue(out records, "Records"))
            length = records.Length;

        for (var i = 0; i < length; i++)
        {
            cancellationToken.ThrowIfCancellationRequested();

            IPropertyHolder actor;
            if (allNodes is {Length: > 0} && allNodes[i].TryLoad(out UObject node))
            {
                actor = node;
            }
            else if (records is {Length: > 0})
            {
                actor = records[i];
            }
            else continue;

            SnooperContext.UpdateStatus($"{original.Name} ... {i}/{length}");
            WorldMesh(actor, transform, true);
        }
        SnooperContext.UpdateStatus($"{original.Name} ... {length}/{length}");

        if (Options.Models.Count == 1)
        {
            var (guid, model) = Options.Models.First();
            Options.SelectModel(guid);
            CameraOp.Setup(model.Box);
            _saveCameraMode = true;
        }
    }

    private void WorldCamera(UObject actor)
    {
        if (actor.ExportType != "LevelBounds" || !actor.TryGetValue(out FPackageIndex boxComponent, "BoxComponent") ||
            boxComponent.Load() is not { } boxObject) return;

        var direction = boxObject.GetOrDefault("RelativeLocation", FVector.ZeroVector) * Constants.SCALE_DOWN_RATIO;
        var position = boxObject.GetOrDefault("RelativeScale3D", FVector.OneVector) / 2f * Constants.SCALE_DOWN_RATIO;
        CameraOp.Setup(new FBox(direction, position));
    }

    private void WorldLight(UObject actor)
    {
        if (!actor.TryGetValue(out FPackageIndex lightComponent, "LightComponent") ||
            lightComponent.Load() is not { } lightObject) return;

        switch (actor.ExportType)
        {
            case "PointLight":
                Options.Lights.Add(new PointLight(Options.Icons["pointlight"], lightObject));
                break;
            case "SpotLight":
                Options.Lights.Add(new SpotLight(Options.Icons["spotlight"], lightObject));
                break;
            case "RectLight":
            case "SkyLight":
            case "DirectionalLight":
                break;
        }
    }

    private void WorldMesh(IPropertyHolder actor, Transform transform, bool forceShow = false)
    {
        if (actor.TryGetValue(out FPackageIndex[] instanceComponents, "InstanceComponents"))
        {
            foreach (var component in instanceComponents)
            {
                if (!component.TryLoad(out UStaticMeshComponent staticMeshComp) ||
                    !staticMeshComp.GetStaticMesh().TryLoad(out UStaticMesh m) || m.Materials.Length < 1)
                    continue;

                var relation = CalculateTransform(staticMeshComp, transform);
                if (staticMeshComp is UInstancedStaticMeshComponent { PerInstanceSMData.Length: > 0 } instancedStaticMeshComp)
                {

                    foreach (var perInstanceData in instancedStaticMeshComp.PerInstanceSMData)
                    {
                        ProcessMesh(actor, instancedStaticMeshComp, m, new Transform
                        {
                            Relation = relation.Matrix,
                            Position = perInstanceData.TransformData.Translation * Constants.SCALE_DOWN_RATIO,
                            Rotation = perInstanceData.TransformData.Rotation,
                            Scale = perInstanceData.TransformData.Scale3D
                        });
                    }
                }
                else ProcessMesh(actor, staticMeshComp, m, relation);
            }
        }
        else if (actor.TryGetValue(out FPackageIndex componentTemplate, "ComponentTemplate") &&
                 componentTemplate.TryLoad(out UObject compTemplate))
        {
            UGeometryCollection geometryCollection = null;
            if (!compTemplate.TryGetValue(out UStaticMesh m, "StaticMesh") &&
                compTemplate.TryGetValue(out FPackageIndex restCollection, "RestCollection") &&
                restCollection.TryLoad(out geometryCollection) && geometryCollection.RootProxyData is { ProxyMeshes.Length: > 0 } rootProxyData)
            {
                rootProxyData.ProxyMeshes[0].TryLoad(out m);
            }

            if (m is { Materials.Length: > 0 })
            {
                OverrideJunoVertexColors(m, geometryCollection);
                ProcessMesh(actor, compTemplate, m, CalculateTransform(compTemplate, transform), forceShow);
            }
        }
        else if (actor.TryGetValue(out FPackageIndex staticMeshComponent, "StaticMeshComponent", "ComponentTemplate", "StaticMesh", "Mesh", "LightMesh", "SplineMesh") &&
                 staticMeshComponent.TryLoad(out UStaticMeshComponent staticMeshComp) &&
                 staticMeshComp.GetStaticMesh().TryLoad(out UStaticMesh m) && m.Materials.Length > 0)
        {
            ProcessMesh(actor, staticMeshComp, m, CalculateTransform(staticMeshComp, transform));
        }
    }

    private void ProcessMesh(IPropertyHolder actor, UStaticMeshComponent staticMeshComp, UStaticMesh m, Transform transform)
    {
        OverrideVertexColors(staticMeshComp, m);
        ProcessMesh(actor, staticMeshComp, m, transform, false);
    }
    private void ProcessMesh(IPropertyHolder actor, UObject staticMeshComp, UStaticMesh m, Transform transform, bool forceShow)
    {
        var bSpline = staticMeshComp is USplineMeshComponent;
        var guid = m.LightingGuid;
        if (Options.TryGetModel(guid, out var model))
        {
            model.AddInstance(transform);
            if (bSpline && model is SplineModel splineModel)
                splineModel.AddComponent((USplineMeshComponent)staticMeshComp);
        }
        else
        {
            try
            {
                var mesh = new StaticMeshDto(m, UserSettings.Default.ExportOptions);
                model = bSpline ? new SplineModel(m, mesh, (USplineMeshComponent)staticMeshComp, transform) : new StaticModel(m, mesh, transform);
                model.IsTwoSided = actor.GetOrDefault("bMirrored", staticMeshComp.GetOrDefault("bDisallowMeshPaintPerInstance", model.IsTwoSided));

                if (actor.TryGetAllValues(out FPackageIndex[] textureData, "TextureData"))
                {
                    var material = model.Materials.FirstOrDefault();
                    if (material is { IsUsed: true })
                    {
                        for (int j = 0; j < textureData.Length; j++)
                        {
                            if (textureData[j]?.Load() is not { } textureDataIdx)
                                continue;

                            if (textureDataIdx.TryGetValue(out FPackageIndex overrideMaterial, "OverrideMaterial") &&
                                overrideMaterial.TryLoad(out var oMaterial) && oMaterial is UMaterialInterface oUnrealMaterial)
                                material.SwapMaterial(oUnrealMaterial);

                        WorldTextureData(material, textureDataIdx, "Diffuse", j switch
                        {
                            0 => "Diffuse",
                            > 0 => $"Diffuse_Texture_{j + 1}",
                            _ => CMaterialParams2.FallbackDiffuse
                        });
                        WorldTextureData(material, textureDataIdx, "Normal", j switch
                        {
                            0 => "Normals",
                            > 0 => $"Normals_Texture_{j + 1}",
                            _ => CMaterialParams2.FallbackNormals
                        });
                        WorldTextureData(material, textureDataIdx, "Specular", j switch
                        {
                            0 => "SpecularMasks",
                            > 0 => $"SpecularMasks_{j + 1}",
                            _ => CMaterialParams2.FallbackNormals
                        });
                    }
                }
            }

            if (staticMeshComp.TryGetValue(out FPackageIndex[] overrideMaterials, "OverrideMaterials"))
            {
                for (var j = 0; j < overrideMaterials.Length && j < model.Sections.Length; j++)
                {
                    var matIndex = model.Sections[j].MaterialIndex;
                    if (matIndex < 0 || matIndex >= model.Materials.Length || matIndex >= overrideMaterials.Length ||
                        overrideMaterials[matIndex].Load() is not UMaterialInterface unrealMaterial) continue;

                    model.Materials[matIndex].SwapMaterial(unrealMaterial);
                }
            }

            if (forceShow)
            {
                foreach (var section in model.Sections)
                {
                    section.Show = true;
                }
            }
            Options.Models[guid] = model;
            }
            catch (Exception e)
            {
                Log.Error(e, "Failed to convert static mesh {Name}", m.Name);
                return;
            }
        }

        if (actor.TryGetValue(out FPackageIndex treasureLight, "PointLight", "TreasureLight") &&
            treasureLight.TryLoad(out var pl1) && pl1.Template.TryLoad(out var pl2))
        {
            Options.Lights.Add(new PointLight(guid, Options.Icons["pointlight"], pl1, pl2, transform));
        }
        if (actor.TryGetValue(out FPackageIndex spotLight, "SpotLight") &&
            spotLight.TryLoad(out var sl1) && sl1.Template.TryLoad(out var sl2))
        {
            Options.Lights.Add(new SpotLight(guid, Options.Icons["spotlight"], sl1, sl2, transform));
        }
    }

    private Transform CalculateTransform(IPropertyHolder staticMeshComp, Transform relation)
    {
        if (staticMeshComp.TryGetValue(out FPackageIndex ap, "AttachParent") && ap.TryLoad(out UObject component))
        {
            relation = CalculateTransform(component, relation);
        }

        return new Transform
        {
            Relation = relation.Matrix,
            Position = staticMeshComp.GetOrDefault("RelativeLocation", FVector.ZeroVector) * Constants.SCALE_DOWN_RATIO,
            Rotation = staticMeshComp.GetOrDefault("RelativeRotation", FRotator.ZeroRotator).Quaternion(),
            Scale = staticMeshComp.GetOrDefault("RelativeScale3D", FVector.OneVector)
        };
    }

    private void OverrideJunoVertexColors(UStaticMesh staticMesh, UGeometryCollection geometryCollection = null)
    {
        if (staticMesh.RenderData is not { LODs.Length: > 0 } || staticMesh.RenderData.LODs[0].ColorVertexBuffer == null)
            return;

        var dico = new Dictionary<byte, FColor>();
        if (geometryCollection?.Materials is not { Length: > 0 })
        {
            var distinctReds = new HashSet<byte>();
            for (int i = 0; i < staticMesh.RenderData.LODs[0].ColorVertexBuffer.Data.Length; i++)
            {
                ref var vertexColor = ref staticMesh.RenderData.LODs[0].ColorVertexBuffer.Data[i];
                var indexAsByte = vertexColor.R;
                if (indexAsByte == 255) indexAsByte = vertexColor.A;
                distinctReds.Add(indexAsByte);
            }

            foreach (var indexAsByte in distinctReds)
            {
                var path = string.Concat("/JunoAtomAssets/Materials/MI_LegoStandard_", indexAsByte, ".MI_LegoStandard_", indexAsByte);
                if (!SnooperContext.TryLoadObject(path, out UMaterialInterface unrealMaterial))
                    continue;

                var parameters = new CMaterialParams2();
                unrealMaterial.GetParams(parameters, EMaterialDepth.TopLayerOnly);

                if (!parameters.TryGetLinearColor(out var color, "Color"))
                    color = FLinearColor.Gray;

                dico[indexAsByte] = color.ToFColor(true);
            }
        }
        else foreach (var material in geometryCollection.Materials)
        {
            if (!material.TryLoad(out UMaterialInterface unrealMaterial)) continue;

            var parameters = new CMaterialParams2();
            unrealMaterial.GetParams(parameters, EMaterialDepth.TopLayerOnly);

            if (!byte.TryParse(material.Name.SubstringAfterLast("_"), out var indexAsByte))
                indexAsByte = byte.MaxValue;
            if (!parameters.TryGetLinearColor(out var color, "Color"))
                color = FLinearColor.Gray;

            dico[indexAsByte] = color.ToFColor(true);
        }

        for (int i = 0; i < staticMesh.RenderData.LODs[0].ColorVertexBuffer.Data.Length; i++)
        {
            ref var vertexColor = ref staticMesh.RenderData.LODs[0].ColorVertexBuffer.Data[i];
            vertexColor = dico.TryGetValue(vertexColor.R, out var color) ? color : FColor.Gray;
        }
    }

    private void OverrideVertexColors(UStaticMeshComponent staticMeshComp, UStaticMesh staticMesh)
    {
        if (staticMeshComp.LODData is not { Length: > 0 } || staticMesh.RenderData is not { LODs.Length: > 0 })
            return;

        for (var lod = 0; lod < staticMeshComp.LODData.Length; lod++)
        {
            var vertexColors = staticMeshComp.LODData[lod].OverrideVertexColors;
            if (vertexColors == null) continue;

            staticMesh.RenderData.LODs[lod].ColorVertexBuffer = vertexColors;
        }
    }

    private void WorldTextureData(Material material, UObject textureData, string name, string key)
    {
        if (textureData.TryGetValue(out FPackageIndex package, name) && package.Load() is UTexture2D texture)
            material.Parameters.Textures[key] = new ResolvedLoadedObject(texture);
    }

    private void AdditionalWorlds(UObject actor, Matrix4x4 relation, CancellationToken cancellationToken)
    {
        if (!actor.TryGetValue(out FSoftObjectPath[] additionalWorlds, "AdditionalWorlds") ||
            !actor.TryGetValue(out FPackageIndex staticMeshComponent, "StaticMeshComponent", "Mesh") ||
            staticMeshComponent.Load() is not { } staticMeshComp)
            return;

        var transform = new Transform
        {
            Relation = relation,
            Position = staticMeshComp.GetOrDefault("RelativeLocation", FVector.ZeroVector) * Constants.SCALE_DOWN_RATIO,
            Rotation = staticMeshComp.GetOrDefault("RelativeRotation", FRotator.ZeroRotator).Quaternion()
        };

        for (int j = 0; j < additionalWorlds.Length; j++)
            if (SnooperContext.TryLoadObject(additionalWorlds[j].AssetPathName.Text, out UWorld w))
                LoadWorld(cancellationToken, w, transform);
    }

    public void WindowResized(int width, int height)
    {
        if (_disposed)
            return;

        CameraOp.AspectRatio = width / (float) height;
        Picking.WindowResized(width, height);
    }

    public void Save()
    {
        Options.ResetModelsLightsAnimations();
        Options.SelectModel(Guid.Empty);
        Options.SwapMaterial(false);
        Options.AnimateMesh(false);

        if (_saveCameraMode) UserSettings.Default.CameraMode = CameraOp.Mode;
        UserSettings.Default.ShowSkybox = ShowSkybox;
        UserSettings.Default.ShowGrid = ShowGrid;
        UserSettings.Default.AnimateWithRotationOnly = AnimateWithRotationOnly;
    }

    public void Dispose()
    {
        if (_disposed)
            return;

        _disposed = true;

        _skybox?.Dispose();
        _grid?.Dispose();
        _shader?.Dispose();
        _outline?.Dispose();
        _light?.Dispose();
        _bone?.Dispose();
        _collision?.Dispose();
        Picking?.Dispose();
        Options?.Dispose();

        _shader = null;
        _outline = null;
        _light = null;
        _bone = null;
        _collision = null;
    }

    private sealed class ProceduralMeshDto : StaticMeshDto
    {
        public ProceduralMeshDto(UObject owner) : base(owner, [])
        {
        }
    }
}
