using System;
using System.Collections.Generic;
using System.Linq;
using System.IO;
using System.Threading;
using CUE4Parse.FileProvider;
using CUE4Parse.UE4.Assets;
using CUE4Parse.UE4.Assets.Exports;
using CUE4Parse.UE4.Assets.Exports.Material;
using CUE4Parse.UE4.Assets.Exports.Texture;
using CUE4Parse_Conversion.Textures;
using SkiaSharp;
using CUE4Parse.UE4.Objects.Core.Misc;
using OpenTK.Graphics.OpenGL4;
using OpenTK.Mathematics;
using OpenTK.Windowing.Common;
using OpenTK.Windowing.Desktop;
using OpenTK.Windowing.GraphicsLibraryFramework;
using PakViewer.Snooper;
using PakViewer.Helpers;
using PakViewer.Services;
using PakViewer.Snooper.Shading;
using Serilog;
using SixLabors.ImageSharp.PixelFormats;
using SixLabors.ImageSharp.Formats.Png;
using Windows.Storage.Streams;
using PakViewer.Snooper.Models;
using PakViewer.Models;
using CUE4Parse.UE4.Assets.Exports.SkeletalMesh;
using CUE4Parse.UE4.Assets.Exports.StaticMesh;
using CUE4Parse.UE4.VirtualFileSystem;

using System.Runtime.InteropServices;
using System.Text.RegularExpressions;
using WinRT.Interop;

namespace PakViewer.Views;

public class PakViewerWindow : GameWindow, IRendererWindow
{
    private const int MaxPreviewTargetMaterials = 32;
    private static readonly HashSet<string> DiffuseParameterNames = CMaterialParams2.Diffuse
        .SelectMany(names => names)
        .ToHashSet(StringComparer.OrdinalIgnoreCase);
    private static readonly HashSet<string> NormalParameterNames = CMaterialParams2.Normals
        .SelectMany(names => names)
        .ToHashSet(StringComparer.OrdinalIgnoreCase);
    private static readonly HashSet<string> SpecularParameterNames = CMaterialParams2.SpecularMasks
        .SelectMany(names => names)
        .ToHashSet(StringComparer.OrdinalIgnoreCase);
    private static readonly HashSet<string> EmissiveParameterNames = CMaterialParams2.Emissive
        .SelectMany(names => names)
        .ToHashSet(StringComparer.OrdinalIgnoreCase);
    private static readonly Regex DiffuseParameterRegex = new(CMaterialParams2.RegexDiffuse, RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex NormalParameterRegex = new(CMaterialParams2.RegexNormals, RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex SpecularParameterRegex = new(CMaterialParams2.RegexSpecularMasks, RegexOptions.IgnoreCase | RegexOptions.Compiled);
    private static readonly Regex EmissiveParameterRegex = new(CMaterialParams2.RegexEmissive, RegexOptions.IgnoreCase | RegexOptions.Compiled);

    private readonly IFileProvider _provider;
    private readonly PakViewer.Snooper.Renderer _renderer;
    private readonly System.Collections.Concurrent.ConcurrentQueue<Action> _actionQueue = new();

    private readonly Dictionary<string, bool> _blockedMaterialVisibility = new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, string> _autoBlockedMaterialReasons = new(StringComparer.OrdinalIgnoreCase);

    private ViewerSettingsWindow? _settingsWindow;
    private string _screenshotPath;
    private MaterialPreviewData? _materialData;
    private PakViewer.Snooper.Shading.Texture? _previewTargetIDsTexture;
    private UObject? _currentExport;
    private readonly IntPtr _parentWindowHandle;

    private System.Numerics.Vector3 _initialCameraPos;
    private System.Numerics.Vector3 _initialCameraDir;
    private float _initialCameraZoom;
    private bool _hasInitialCamera;

    private Vector2 _lastMousePos;
    private bool _firstMouseMove = true;

    private Vector2 _leftDownPos;
    private bool _leftDown;
    private bool _leftDragging;
    private const float ClickDragThresholdPx = 4f;
    private bool _showWireframe;
    private double _fpsAccumulator;
    private int _fpsFrameCount;
    private int _currentFps;

    public PakViewerWindow(GameWindowSettings gwSettings, NativeWindowSettings nwSettings, IFileProvider provider, IntPtr parentWindowHandle = default)
        : base(gwSettings, nwSettings)
    {
        _provider = provider;
        _parentWindowHandle = parentWindowHandle;

        SnooperContext.Provider = provider;
        SnooperContext.Status = static msg => Log.Information(msg);

        _renderer = new PakViewer.Snooper.Renderer(ClientSize.X, ClientSize.Y);

        _screenshotPath = UserSettings.Default.ModelDirectory;

        TryLoadWindowIcon();
    }

    private void TryLoadWindowIcon()
    {
        try
        {
            var iconPath = Path.Combine(AppContext.BaseDirectory, "Icons", "3Ds_Viewer.ico");
            if (!File.Exists(iconPath))
            {
                iconPath = Path.Combine(AppContext.BaseDirectory, "Icons", "PakViewer.ico");
                if (!File.Exists(iconPath))
                    return;
            }

            IntPtr hwnd;
            unsafe { hwnd = GetWin32Window((IntPtr)WindowPtr); }
            if (hwnd == IntPtr.Zero) return;

            var hIconLarge = LoadImage(IntPtr.Zero, iconPath, IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_LOADFROMFILE);
            var hIconSmall = LoadImage(IntPtr.Zero, iconPath, IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE);

            if (hIconLarge != IntPtr.Zero)
                SendMessage(hwnd, WM_SETICON, ICON_BIG, hIconLarge);
            if (hIconSmall != IntPtr.Zero)
                SendMessage(hwnd, WM_SETICON, ICON_SMALL, hIconSmall);
        }
        catch
        {
        }
    }

    private const int IMAGE_ICON = 1;
    private const int LR_LOADFROMFILE = 0x00000010;
    private const int SM_CXICON = 11;
    private const int SM_CYICON = 12;
    private const int SM_CXSMICON = 49;
    private const int SM_CYSMICON = 50;
    private const int WM_SETICON = 0x0080;
    private static readonly IntPtr ICON_SMALL = IntPtr.Zero;
    private static readonly IntPtr ICON_BIG = new(1);

    [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern IntPtr LoadImage(IntPtr hInst, string lpszName, int uType, int cxDesired, int cyDesired, int fuLoad);

    [DllImport("user32.dll")]
    private static extern int GetSystemMetrics(int nIndex);

    [DllImport("user32.dll")]
    private static extern IntPtr SendMessage(IntPtr hWnd, int Msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern IntPtr SetWindowLongPtr(IntPtr hWnd, int nIndex, IntPtr dwNewLong);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern IntPtr SetActiveWindow(IntPtr hWnd);

    [DllImport("glfw3.dll", EntryPoint = "glfwGetWin32Window", CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr GetWin32Window(IntPtr window);

    private const int GWLP_HWNDPARENT = -8;

    KeyboardState IRendererWindow.KeyboardState => KeyboardState;

    public void SetMaterialData(MaterialPreviewData? materialData)
    {
        _materialData = materialData;
    }

    public MaterialPreviewData? GetMaterialData()
    {
        return _materialData;
    }

    public void WindowShouldClose(bool close, bool viaEscape)
    {
        if (close)
        {
            Close();
        }
    }

    public bool LoadStaticMesh(CUE4Parse.UE4.Assets.Exports.StaticMesh.UStaticMesh mesh)
    {
        return LoadExport(mesh);
    }

    public bool LoadSkeletalMesh(CUE4Parse.UE4.Assets.Exports.SkeletalMesh.USkeletalMesh mesh)
    {
        return LoadExport(mesh);
    }

    public bool LoadExport(UObject export)
    {
        try
        {

            if (_materialData != null)
            {
                _materialData.Dispose();
                _materialData = null;
            }

            _currentExport = export;
            LogExportPreflight(export);

            var lazy = new Lazy<UObject>(() => export);
            _renderer.Load(CancellationToken.None, export, lazy);
            UpdateAutoBlockedMaterials();
            ApplyBlockedMaterialVisibilityToLoadedModels();
            RefreshSettingsWindow();

            if (_renderer.Options.Icons.ContainsKey("checker"))
            {
                _renderer.Options.SetupModelsAndLights();
            }

            Log.Debug("Model loaded: Name={Name} Models={ModelCount} Lights={LightCount} Anims={AnimCount}",
                export.Name,
                _renderer.Options.Models.Count,
                _renderer.Options.Lights.Count,
                _renderer.Options.Animations.Count);

            LogLoadedTextures();

            CaptureInitialCameraIfNeeded();

            return _renderer.Options.Models.Count > 0;
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to load export into PakViewerWindow");
            return false;
        }
    }

    public bool LoadFromFilePath(string filePath)
    {
        try
        {
            Log.Debug("Opening package: {FilePath}", filePath);
            var package = _provider.LoadPackage(filePath);

            UObject? meshExport = null;
            UObject? skeletonExport = null;
            var animExports = new List<UObject>();

            foreach (var exportLazy in package.ExportsLazy)
            {
                var obj = exportLazy?.Value;
                if (obj is CUE4Parse.UE4.Assets.Exports.StaticMesh.UStaticMesh or
                    CUE4Parse.UE4.Assets.Exports.SkeletalMesh.USkeletalMesh or
                    CUE4Parse.UE4.Assets.Exports.Material.UMaterialInstance)
                {
                    meshExport = obj;
                }
                else if (obj is CUE4Parse.UE4.Assets.Exports.Animation.USkeleton)
                {
                    skeletonExport = obj;
                }
                else if (obj is CUE4Parse.UE4.Assets.Exports.Animation.UAnimSequenceBase)
                {
                    animExports.Add(obj);
                }
            }

            UObject? primaryExport = meshExport ?? skeletonExport;
            if (primaryExport == null && animExports.Count == 0) return false;

            var result = primaryExport != null && LoadExport(primaryExport);

            if (!result && animExports.Count > 0 && animExports[0] is CUE4Parse.UE4.Assets.Exports.Animation.UAnimSequenceBase animBase)
            {
                if (animBase.Skeleton.TryLoad(out var skeleton))
                {
                    result = LoadExport(skeleton);
                    if (result)
                        primaryExport = skeleton;
                }
            }

            if (result && animExports.Count > 0 && (meshExport is CUE4Parse.UE4.Assets.Exports.SkeletalMesh.USkeletalMesh || primaryExport is CUE4Parse.UE4.Assets.Exports.Animation.USkeleton))
            {
                Log.Information("Found {Count} animation(s) in package, auto-loading...", animExports.Count);

                SnooperContext.ModelIsWaitingAnimation = true;
                foreach (var anim in animExports)
                {
                    try
                    {
                        _renderer.Animate(anim);
                    }
                    catch (Exception ex)
                    {
                        Log.Warning(ex, "Failed to auto-load animation {Name}", anim.Name);
                    }
                }
                SnooperContext.ModelIsWaitingAnimation = false;
            }

            return result;
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to load model from file path {FilePath}", filePath);
            return false;
        }
    }

    protected override void OnLoad()
    {
        base.OnLoad();

        VSync = VSyncMode.Off;

        unsafe
        {
            GLFW.SetWindowAttrib(WindowPtr, WindowAttribute.Floating, false);

            if (_parentWindowHandle != IntPtr.Zero)
            {
                try
                {
                    var hwnd = GetWin32Window((IntPtr)WindowPtr);
                    if (hwnd != IntPtr.Zero)
                    {
                        SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, _parentWindowHandle);
                    }
                }
                catch (Exception ex)
                {
                    Log.Error(ex, "Failed to set window owner");
                }
            }
        }

        GL.ClearColor(new Color4(0.06f, 0.07f, 0.09f, 1.0f));
        GL.Enable(EnableCap.Blend);
        GL.Enable(EnableCap.CullFace);
        GL.Enable(EnableCap.DepthTest);
        GL.Enable(EnableCap.Multisample);
        GL.Enable(EnableCap.VertexProgramPointSize);
        GL.StencilOp(StencilOp.Keep, StencilOp.Replace, StencilOp.Replace);
        GL.BlendFunc(BlendingFactor.SrcAlpha, BlendingFactor.OneMinusSrcAlpha);

        _renderer.Setup();
        LogRendererPostSetup();

        BringToFront();
    }

    public void BringToFront()
    {
        IsVisible = true;
        Focus();
        unsafe
        {
            GLFW.RestoreWindow(WindowPtr);
            GLFW.FocusWindow(WindowPtr);
        }
    }

    private void LogExportPreflight(UObject export)
    {

        Log.Debug("Loading export: Type={Type} Name={Name}", export.ExportType, export.Name);
    }

    private MaterialPreviewData ExtractMaterialDataFromModels()
    {
        var materialData = new MaterialPreviewData();

        try
        {
            int materialIndex = 0;
            foreach (var model in _renderer.Options.Models.Values)
            {
                if (model.Materials == null) continue;

                foreach (var material in model.Materials)
                {

                    var materialGroup = new MaterialGroup
                    {
                        Name = material.Name ?? $"Material_{materialIndex}",
                        MaterialIndex = materialIndex++,
                        IsExpanded = materialData.Materials.Count == 0,
                        Textures = new List<MaterialTextureParam>(),
                        Scalars = new List<MaterialScalarParam>(),
                        Colors = new List<MaterialVectorParam>()
                    };

                    var uniqueTextures = new Dictionary<string, MaterialTextureParam>();

                    if (material.Parameters.Textures != null && material.Parameters.Textures.Count > 0)
                    {
                        foreach (var texParam in material.Parameters.Textures)
                        {
                            var textureName = texParam.Key;
                            var textureObj = texParam.Value;

                            if (textureObj == null) continue;

                            var texturePath = textureObj.GetObjectPathName();

                            if (uniqueTextures.ContainsKey(texturePath))
                                continue;

                            Microsoft.UI.Xaml.Media.Imaging.BitmapImage? thumbnail = null;

                            try
                            {
                                if (textureObj.TryLoad(out UTexture2D texture2D))
                                {
                                    const int thumbnailSize = 64;
                                    const int maxSourceSize = 256;

                                    var decoded = texture2D.Decode(maxSourceSize, UserSettings.Default.ExportOptions);

                                    if (decoded != null)
                                    {
                                        var skBitmap = decoded.ToSkBitmap();

                                        if (skBitmap != null)
                                        {
                                            SkiaSharp.SKBitmap? resized = null;
                                            if (skBitmap.Width > thumbnailSize || skBitmap.Height > thumbnailSize)
                                            {
                                                resized = skBitmap.Resize(new SkiaSharp.SKImageInfo(thumbnailSize, thumbnailSize), SkiaSharp.SKSamplingOptions.Default);
                                            }

                                            var finalBitmap = resized ?? skBitmap;
                                            thumbnail = ConvertSkBitmapToBitmapImage(finalBitmap);
                                            resized?.Dispose();
                                            skBitmap.Dispose();
                                        }
                                    }
                                }
                            }
                            catch (Exception ex)
                            {
                                Log.Debug(ex, "Failed to generate thumbnail for {TextureName}", textureName);
                            }

                            var textureParam = new MaterialTextureParam
                            {
                                Name = textureName,
                                Path = texturePath,
                                Thumbnail = thumbnail,
                                MaterialNames = new List<string> { material.Name }
                            };

                            uniqueTextures[texturePath] = textureParam;
                        }

                        foreach (var texture in uniqueTextures.Values)
                        {
                            materialGroup.Textures.Add(texture);
                        }
                    }

                    if (material.Parameters.Scalars != null)
                    {
                        foreach (var scalarParam in material.Parameters.Scalars)
                        {
                            materialGroup.Scalars.Add(new MaterialScalarParam
                            {
                                Name = scalarParam.Key,
                                Value = scalarParam.Value
                            });
                        }
                    }

                    if (material.Parameters.Colors != null)
                    {
                        foreach (var colorParam in material.Parameters.Colors)
                        {
                            var color = colorParam.Value;
                            materialGroup.Colors.Add(new MaterialVectorParam
                            {
                                Name = colorParam.Key,
                                R = color.R,
                                G = color.G,
                                B = color.B,
                                A = color.A
                            });
                        }
                    }

                    materialGroup.TextureCount = materialGroup.Textures.Count;
                    materialGroup.ScalarCount = materialGroup.Scalars.Count;
                    materialGroup.ColorCount = materialGroup.Colors.Count;

                    materialData.Materials.Add(materialGroup);
                }
            }

            Log.Information("Extracted {MaterialCount} materials with {TotalTextures} total textures, {TotalScalars} scalars, {TotalColors} colors",
                materialData.Materials.Count,
                materialData.Materials.Sum(m => m.TextureCount),
                materialData.Materials.Sum(m => m.ScalarCount),
                materialData.Materials.Sum(m => m.ColorCount));
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to extract material data from models");
        }

        return materialData;
    }

    private Microsoft.UI.Xaml.Media.Imaging.BitmapImage? ConvertSkBitmapToBitmapImage(SkiaSharp.SKBitmap skBitmap)
    {
        try
        {
            using var stream = new MemoryStream();
            skBitmap.Encode(stream, SkiaSharp.SKEncodedImageFormat.Png, 100);
            stream.Position = 0;

            var bitmapImage = new Microsoft.UI.Xaml.Media.Imaging.BitmapImage();
            bitmapImage.SetSource(stream.AsRandomAccessStream());
            return bitmapImage;
        }
        catch (Exception ex)
        {
            Log.Warning(ex, "Failed to convert SKBitmap to BitmapImage");
            return null;
        }
    }

    private void LogRendererPostSetup()
    {

        Log.Information("Model loaded: Models={ModelCount} Textures={TextureCount}",
            _renderer.Options.Models.Count,
            _renderer.Options.Textures.Count);
    }

    private void LogLoadedTextures()
    {

        if (!UserSettings.Default.EnableDetailedTextureLogging)
        {
            return;
        }

        Log.Information("═══════════════════════════════════════════════════════");
        Log.Information("LOADED TEXTURES - For debugging");
        Log.Information("═══════════════════════════════════════════════════════");

        foreach (var model in _renderer.Options.Models.Values)
        {
            Log.Information("Model: {ModelName}", model.Name);

            if (model.Materials == null || model.Materials.Length == 0)
            {
                Log.Warning("No materials found on this model");
                continue;
            }

            for (int i = 0; i < model.Materials.Length; i++)
            {
                var material = model.Materials[i];
                Log.Information("  ┌─ Material [{Index}]: {MaterialName}", i, material.Name ?? "(unnamed)");

                if (material.Parameters.Textures != null && material.Parameters.Textures.Count > 0)
                {
                    Log.Information("  │ Textures ({Count}):", material.Parameters.Textures.Count);

                    foreach (var texParam in material.Parameters.Textures)
                    {
                        var paramName = texParam.Key;
                        var textureObj = texParam.Value;

                        if (textureObj == null)
                        {
                            Log.Information("  │    • {ParamName}: NULL", paramName);
                            continue;
                        }

                        var texturePath = textureObj.GetObjectPathName();
                        var textureName = textureObj.GetObjectName();

                        var textureType = ClassifyTextureParameter(paramName);

                        Log.Information("  │    • {ParamName} [{Type}]:", paramName, textureType);
                        Log.Information("  │        Name: {TextureName}", textureName);
                        Log.Information("  │        Path: {TexturePath}", texturePath);

                        if (textureObj.TryLoad(out UTexture2D texture2D))
                        {
                            var mip = texture2D.GetFirstMip();
                            if (mip != null)
                            {
                                Log.Information("  │        Size: {Width}x{Height}", mip.SizeX, mip.SizeY);
                            }
                        }
                    }
                }
                else
                {
                    Log.Information("  │ No textures assigned");
                }

                if (material.Parameters.Scalars != null && material.Parameters.Scalars.Count > 0)
                {
                    Log.Information("  │ Scalars: {Count} parameters", material.Parameters.Scalars.Count);
                }

                if (material.Parameters.Colors != null && material.Parameters.Colors.Count > 0)
                {
                    Log.Information("  │ Colors: {Count} parameters", material.Parameters.Colors.Count);
                }

                Log.Information("  └─");
            }

            Log.Information("");
        }

        Log.Information("═══════════════════════════════════════════════════════");
    }

    private string ClassifyTextureParameter(string paramName)
    {
        var lowerName = paramName.ToLowerInvariant();

        if (lowerName.Contains("diffuse") || lowerName.Contains("albedo") || lowerName.Contains("basecolor") || lowerName.Contains("_d"))
            return "DIFFUSE";
        if (lowerName.Contains("normal") || lowerName.Contains("_n"))
            return "NORMAL";
        if (lowerName.Contains("specular") || lowerName.Contains("spec") || lowerName.Contains("_s"))
            return "SPECULAR";
        if (lowerName.Contains("roughness") || lowerName.Contains("rough") || lowerName.Contains("_r"))
            return "ROUGHNESS";
        if (lowerName.Contains("metallic") || lowerName.Contains("metal") || lowerName.Contains("_m"))
            return "METALLIC";
        if (lowerName.Contains("orm") || lowerName.Contains("rma"))
            return "PACKED(ORM/RMA)";
        if (lowerName.Contains("emissive") || lowerName.Contains("emission") || lowerName.Contains("_e"))
            return "EMISSIVE";
        if (lowerName.Contains("opacity") || lowerName.Contains("alpha") || lowerName.Contains("_a"))
            return "OPACITY";
        if (lowerName.Contains("ao") || lowerName.Contains("occlusion"))
            return "AMBIENT_OCCLUSION";
        if (lowerName.Contains("mask"))
            return "MASK";
        if (lowerName.Contains("id"))
            return "ID_MAP";

        return "UNKNOWN";
    }

    protected override void OnRenderFrame(FrameEventArgs args)
    {
        base.OnRenderFrame(args);

        GL.Clear(ClearBufferMask.ColorBufferBit | ClearBufferMask.DepthBufferBit | ClearBufferMask.StencilBufferBit);
        _renderer.Render();
        DrawStatsOverlay();

        SwapBuffers();
    }

    protected override void OnUpdateFrame(FrameEventArgs args)
    {
        base.OnUpdateFrame(args);

        while (_actionQueue.TryDequeue(out var action))
        {
            action();
        }

        var delta = (float)args.Time;

        if (KeyboardState.IsKeyPressed(Keys.Escape))
        {
            Close();
            return;
        }
        if (KeyboardState.IsKeyPressed(Keys.G))
        {
            _renderer.ShowGrid = !_renderer.ShowGrid;
            Log.Information("Grid: {State}", _renderer.ShowGrid ? "ON" : "OFF");
        }
        if (KeyboardState.IsKeyPressed(Keys.H))
        {
            ShowHelpDialog();
        }
        if (KeyboardState.IsKeyPressed(Keys.F12))
        {
            TakeScreenshot();
        }
        if (KeyboardState.IsKeyPressed(Keys.Space))
        {
            ToggleAllSections();
        }
        if (KeyboardState.IsKeyPressed(Keys.R))
        {
            ResetCamera();
        }
        if (KeyboardState.IsKeyPressed(Keys.Backspace))
        {
            _renderer.Options.RemoveAnimations();
            Log.Information("Animations cleared");
        }
        if (KeyboardState.IsKeyPressed(Keys.N))
        {
            CycleAnimation(1);
        }
        if (KeyboardState.IsKeyPressed(Keys.B))
        {
            CycleAnimation(-1);
        }

        var shiftDown = KeyboardState.IsKeyDown(Keys.LeftShift) || KeyboardState.IsKeyDown(Keys.RightShift);
        for (var i = 0; i <= 9; i++)
        {
            var key = Keys.D0 + i;
            if (KeyboardState.IsKeyPressed(key))
            {
                var sectionIndex = shiftDown ? i + 10 : i;
                ToggleSection(sectionIndex);
            }
        }

        if (KeyboardState.IsKeyPressed(Keys.F1))
            SetCameraPreset(1, "Front");
        if (KeyboardState.IsKeyPressed(Keys.F2))
            SetCameraPreset(2, "Back");
        if (KeyboardState.IsKeyPressed(Keys.F3))
            SetCameraPreset(3, "Left");
        if (KeyboardState.IsKeyPressed(Keys.F4))
            SetCameraPreset(4, "Right");
        if (KeyboardState.IsKeyPressed(Keys.F5))
            SetCameraPreset(5, "Top");
        if (KeyboardState.IsKeyPressed(Keys.F6))
            SetCameraPreset(6, "Bottom");

        if (KeyboardState.IsKeyPressed(Keys.J))
        {
            _showWireframe = !_showWireframe;
            foreach (var model in _renderer.Options.Models.Values)
                model.ShowWireframe = _showWireframe;
            Log.Information("Wireframe: {State}", _showWireframe ? "ON" : "OFF");
        }

        _fpsAccumulator += args.Time;
        _fpsFrameCount++;
        if (_fpsAccumulator >= 1.0)
        {
            _currentFps = _fpsFrameCount;
            _fpsFrameCount = 0;
            _fpsAccumulator = 0;
        }

        _renderer.Update(this, delta);
    }

    private void CaptureInitialCameraIfNeeded()
    {
        if (_hasInitialCamera) return;
        _initialCameraPos = _renderer.CameraOp.Position;
        _initialCameraDir = _renderer.CameraOp.Direction;
        _initialCameraZoom = _renderer.CameraOp.Zoom;
        _hasInitialCamera = true;
    }

    private void ResetCamera()
    {
        if (!_hasInitialCamera)
            CaptureInitialCameraIfNeeded();

        _renderer.CameraOp.Position = _initialCameraPos;
        _renderer.CameraOp.Direction = _initialCameraDir;
        _renderer.CameraOp.Zoom = _initialCameraZoom;
        Log.Information("Camera reset");
    }

    private void SetCameraPreset(int preset, string viewName)
    {
        var model = _renderer.Options.Models.Values.FirstOrDefault();
        if (model == null) return;

        _renderer.CameraOp.SetPresetView(preset, model.Box);
        Log.Information("Camera: {ViewName} view", viewName);
    }

    private void ToggleAllSections()
    {
        var anyHidden = false;
        foreach (var model in _renderer.Options.Models.Values)
        {
            if (model.Sections == null) continue;
            for (var i = 0; i < model.Sections.Length; i++)
            {
                if (!model.Sections[i].Show)
                {
                    anyHidden = true;
                    break;
                }
            }
            if (anyHidden) break;
        }

        var newState = anyHidden;
        foreach (var model in _renderer.Options.Models.Values)
        {
            if (model.Sections == null) continue;
            for (var i = 0; i < model.Sections.Length; i++)
                model.Sections[i].Show = newState;
        }

        Log.Information(newState ? "All sections visible" : "All sections hidden");
        RefreshSettingsWindow();
    }

    private void ToggleSection(int index)
    {
        foreach (var model in _renderer.Options.Models.Values)
        {
            if (model.Sections == null) continue;
            if (index < 0 || index >= model.Sections.Length) continue;

            model.Sections[index].Show = !model.Sections[index].Show;
            var state = model.Sections[index].Show ? "visible" : "hidden";
            Log.Information("Section {Index} {State}", index, state);
        }
        RefreshSettingsWindow();
    }

    private void SetMaterialVisibility(string materialName, bool show)
    {
        _blockedMaterialVisibility[materialName] = show;
        ApplyBlockedMaterialVisibilityToLoadedModels();
        Log.Information("Material '{MaterialName}' visibility set to {State}", materialName, show ? "shown" : "hidden");
        RefreshSettingsWindow();
    }

    private void RemoveBlockedMaterial(string materialName)
    {
        _blockedMaterialVisibility.Remove(materialName);
        ApplyBlockedMaterialVisibilityToLoadedModels();
        Log.Information("Material '{MaterialName}' removed from blocked list", materialName);
        RefreshSettingsWindow();
    }

    private void UpdateAutoBlockedMaterials()
    {
        _autoBlockedMaterialReasons.Clear();

        foreach (var model in _renderer.Options.Models.Values)
        {
            if (model.Materials == null) continue;

            foreach (var material in model.Materials)
            {
                if (string.IsNullOrWhiteSpace(material.Name))
                    continue;

                var isFallbackOnlyHelper = material.UsedFallbackTextureAssignment && !material.HasAuthoredTextureData;
                var isAnalyzedHelper = material.Analysis == MaterialAnalysis.Fallback || material.Analysis == MaterialAnalysis.NoTextures;
                if (!isFallbackOnlyHelper && !isAnalyzedHelper)
                    continue;

                var reason = isFallbackOnlyHelper
                    ? "Material had no extracted authored textures, so the viewer had to guess fallback textures."
                    : $"Material analysis: {material.Analysis}";

                _autoBlockedMaterialReasons[material.Name] = reason;
                _blockedMaterialVisibility[material.Name] = false;
                Log.Information("Auto-blocked material '{MaterialName}': {Reason}", material.Name, reason);
            }
        }
    }

    private void ApplyBlockedMaterialVisibilityToLoadedModels()
    {
        foreach (var model in _renderer.Options.Models.Values)
        {
            if (model.Sections == null || model.Materials == null) continue;

            for (var i = 0; i < model.Sections.Length; i++)
            {
                var matIndex = model.Sections[i].MaterialIndex;
                if (matIndex < 0 || matIndex >= model.Materials.Length) continue;

                var materialName = model.Materials[matIndex].Name;
                var show = true;

                foreach (var blockedMaterial in _blockedMaterialVisibility)
                {
                    if (!materialName.Contains(blockedMaterial.Key, StringComparison.OrdinalIgnoreCase))
                        continue;

                    show = blockedMaterial.Value;
                    break;
                }

                model.Sections[i].Show = show;
            }
        }
    }

    private List<BlockedMaterialEntry> BuildBlockedMaterialEntries()
    {
        var loadedMaterialNames = new List<string>();
        var seenNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        foreach (var model in _renderer.Options.Models.Values)
        {
            if (model.Materials == null) continue;

            foreach (var material in model.Materials)
            {
                if (string.IsNullOrWhiteSpace(material.Name) || !seenNames.Add(material.Name))
                    continue;

                loadedMaterialNames.Add(material.Name);
            }
        }

        var blockedEntries = new List<BlockedMaterialEntry>();
        foreach (var blockedMaterial in _blockedMaterialVisibility)
        {
            if (!loadedMaterialNames.Any(name => name.Contains(blockedMaterial.Key, StringComparison.OrdinalIgnoreCase)))
                continue;

            blockedEntries.Add(new BlockedMaterialEntry
            {
                Name = blockedMaterial.Key,
                IsShown = blockedMaterial.Value,
                IsAutoDetected = _autoBlockedMaterialReasons.ContainsKey(blockedMaterial.Key),
                Reason = _autoBlockedMaterialReasons.GetValueOrDefault(blockedMaterial.Key, string.Empty)
            });
        }

        return blockedEntries;
    }

    private void CycleAnimation(int direction)
    {
        var anims = _renderer.Options.Animations;
        if (anims.Count == 0) return;

        var current = _renderer.Options.SelectedAnimation + direction;
        if (current < 0) current = anims.Count - 1;
        if (current >= anims.Count) current = 0;

        _renderer.Options.SelectAnimation(current);
        var name = anims[current].Name;
        Log.Information("Animation: {Name} ({Index}/{Total})", name, current + 1, anims.Count);
    }

    public bool LoadAnimationFromFilePath(string filePath)
    {
        try
        {
            Log.Debug("Opening animation package: {FilePath}", filePath);
            var package = _provider.LoadPackage(filePath);

            foreach (var exportLazy in package.ExportsLazy)
            {
                var obj = exportLazy?.Value;
                if (obj is CUE4Parse.UE4.Assets.Exports.Animation.UAnimSequenceBase anim)
                {
                    _renderer.Animate(anim);
                    Log.Information("Loaded animation: {Name}", anim.Name);
                    return true;
                }
            }

            return false;
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to load animation from file path {FilePath}", filePath);
            return false;
        }
    }

    private void RefreshSettingsWindow()
    {
        if (_settingsWindow == null) return;

        Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread().TryEnqueue(() =>
        {
            var meshSectionsList = new List<MeshSectionInfo>();
            var model = _renderer.Options.Models.Values.FirstOrDefault();
            var sections = model?.Sections;
            if (sections != null)
            {
                for (var i = 0; i < sections.Length; i++)
                {
                    var matName = model?.Materials != null && sections[i].MaterialIndex >= 0 && sections[i].MaterialIndex < model.Materials.Length
                        ? model.Materials[sections[i].MaterialIndex].Name
                        : $"Section {i}";

                    var index = i;
                    meshSectionsList.Add(new MeshSectionInfo
                    {
                        Shortcut = i.ToString(),
                        Name = matName,
                        Status = sections[i].Show ? "Visible" : "Hidden",
                        SectionIndex = i,
                        ToggleAction = () => ToggleSection(index)
                    });
                }
            }
            _settingsWindow.UpdateMeshSections(meshSectionsList);
            _settingsWindow.UpdateBlockedMaterials(BuildBlockedMaterialEntries());
        });
    }

    private void OnTextureClicked(string path, int channelMode, string? targetMaterialName)
    {
        _actionQueue.Enqueue(() => LoadTexture(path, channelMode, targetMaterialName));
    }

    private void OnExportTexture(string path, string saveFilePath)
    {
        _actionQueue.Enqueue(() => ExportTextureAsPng(path, saveFilePath));
    }

    private void OnLoadCustomTexture(string originalTexturePath, string customFilePath)
    {
        _actionQueue.Enqueue(() => LoadCustomTextureFromFile(originalTexturePath, customFilePath));
    }

    private void OnResetOriginalTextures()
    {
        _actionQueue.Enqueue(() =>
        {
            if (_currentExport == null) return;

            try
            {
                var shader = _renderer.GetMainShader();
                shader.Use();
                shader.TrySetUniform("uPreviewMode", false);
                shader.TrySetUniform("uNumTargetMaterialIDs", 0);
                shader.TrySetUniform("uDebugShowMaterialIDs", false);
                shader.TrySetUniform("uShowPreviewBounds", false);
                shader.TrySetUniform("uPreviewOpacity", 1.0f);
                shader.TrySetUniform("uChannelMode", 0);
            }
            catch (Exception ex)
            {
                Log.Warning(ex, "Failed to disable preview mode in shader");
            }

            _previewTargetIDsTexture?.Dispose();
            _previewTargetIDsTexture = null;

            _renderer.Options.ResetModelsLightsAnimations();
            LoadExport(_currentExport);
            Log.Information("Reset to original textures");
        });
    }

    private void OnMaterialVisibilityToggled(string materialName, bool isVisible)
    {
        _actionQueue.Enqueue(() => SetMaterialVisibility(materialName, isVisible));
    }

    private void LoadTexture(string path, int channelMode, string? targetMaterialName = null)
    {
        try
        {
            Log.Information("LoadTexture called: path={Path}, channelMode={ChannelMode}, targetMaterial={Material}",
                path, channelMode, targetMaterialName ?? "(all materials)");

            if (!_provider.TryLoadPackageObject(path, out var obj) || obj is not UTexture2D texture)
            {
                Log.Warning("Failed to load texture object from path: {Path}", path);
                return;
            }

            var cTexture = texture.Decode(UserSettings.Default.ExportOptions);
            if (cTexture == null)
            {
                Log.Warning("Failed to decode texture: {Name}", texture.Name);
                return;
            }

            var bitmap = cTexture.ToSkBitmap();
            var glTex = new PakViewer.Snooper.Shading.Texture(bitmap, texture);

            var textureName = texture.Name;
            var texturePathLower = path.ToLowerInvariant();
            var matchedMaterialKeys = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            var matchedMaterialIDs = new List<System.Numerics.Vector3>();
            var matchedMaterialNames = new List<string>();
            int? resolvedTextureType = null;

            foreach (var model in _renderer.Options.Models.Values)
            {
                if (model.Materials == null) continue;

                foreach (var mat in model.Materials)
                {

                    if (!string.IsNullOrEmpty(targetMaterialName) &&
                        !string.Equals(mat.Name, targetMaterialName, StringComparison.OrdinalIgnoreCase))
                    {
                        continue;
                    }

                    if (!TryFindMatchingTextureType(mat, textureName, texturePathLower, out var textureType))
                    {
                        continue;
                    }

                    resolvedTextureType ??= textureType;
                    if (resolvedTextureType != textureType)
                    {
                        Log.Debug(
                            "Texture '{Texture}' matched parameter type {Type} on material '{Material}', but preview is already using type {ResolvedType}",
                            texture.Name,
                            textureType,
                            mat.Name,
                            resolvedTextureType.Value);
                    }

                    var materialKey = !string.IsNullOrWhiteSpace(mat.Path) ? mat.Path : mat.Name;
                    if (!matchedMaterialKeys.Add(materialKey))
                    {
                        continue;
                    }

                    mat.EnsureProceduralIDMap();
                    matchedMaterialIDs.Add(mat.ProceduralMaterialID);
                    matchedMaterialNames.Add(mat.Name);

                    if (matchedMaterialIDs.Count == MaxPreviewTargetMaterials)
                    {
                        Log.Warning(
                            "Preview target limit reached for texture '{Texture}'. Only the first {Count} matching materials will be previewed.",
                            texture.Name,
                            MaxPreviewTargetMaterials);
                        break;
                    }
                }

                if (matchedMaterialIDs.Count == MaxPreviewTargetMaterials)
                {
                    break;
                }
            }

            if (matchedMaterialIDs.Count == 0)
            {
                Log.Information("No materials found using texture: {Name}", texture.Name);
                return;
            }

            UpdatePreviewShaderUniforms(glTex, matchedMaterialIDs, resolvedTextureType ?? 0, channelMode);

            var channelName = channelMode switch
            {
                1 => "Red",
                2 => "Green",
                3 => "Blue",
                4 => "Alpha",
                5 => "Grayscale",
                _ => "RGB"
            };

            Log.Information(
                "Preview enabled for {Count} material(s) with texture: {Texture} [{Materials}] - Channel: {Channel}",
                matchedMaterialNames.Count,
                texture.Name,
                string.Join(", ", matchedMaterialNames),
                channelName);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to load preview texture");
        }
    }

    private void ExportTextureAsPng(string path, string saveFilePath)
    {
        try
        {
            if (!_provider.TryLoadPackageObject(path, out var obj) || obj is not UTexture2D texture)
            {
                Log.Warning("Failed to load texture object for export: {Path}", path);
                return;
            }

            var decoded = texture.Decode(UserSettings.Default.ExportOptions);
            if (decoded == null)
            {
                Log.Warning("Failed to decode texture for export: {Name}", texture.Name);
                return;
            }

            var bitmap = decoded.ToSkBitmap();

            using var stream = File.Create(saveFilePath);
            bitmap.Encode(stream, SkiaSharp.SKEncodedImageFormat.Png, 100);
            bitmap.Dispose();

            Log.Information("Exported texture: {Name} → {Path}", texture.Name, saveFilePath);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to export texture as PNG");
        }
    }

    private void LoadCustomTextureFromFile(string originalTexturePath, string customFilePath)
    {
        try
        {

            using var stream = File.OpenRead(customFilePath);
            var bitmap = SkiaSharp.SKBitmap.Decode(stream);

            if (bitmap == null)
            {
                Log.Error("Failed to decode custom texture file: {Path}", customFilePath);
                return;
            }

            var customTextureName = System.IO.Path.GetFileNameWithoutExtension(customFilePath);
            var glTex = new PakViewer.Snooper.Shading.Texture(bitmap, customTextureName, forceLinear: false, generateMipmaps: true);

            var matchedMaterialKeys = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            var matchedMaterialIDs = new List<System.Numerics.Vector3>();
            var matchedMaterialNames = new List<string>();
            int? resolvedTextureType = null;

            foreach (var model in _renderer.Options.Models.Values)
            {
                if (model.Materials == null) continue;

                foreach (var mat in model.Materials)
                {

                    var usesTexture = false;
                    int textureType = 0;

                    if (mat.Parameters.Textures != null)
                    {
                        foreach (var texParam in mat.Parameters.Textures)
                        {
                            if (texParam.Value != null &&
                                string.Equals(texParam.Value.GetObjectPathName(), originalTexturePath, StringComparison.OrdinalIgnoreCase))
                            {
                                usesTexture = true;
                                textureType = InferTextureType(texParam.Key, texParam.Value.GetObjectName());
                                break;
                            }
                        }
                    }

                    if (!usesTexture)
                    {
                        if (mat.Diffuse?.Any(t => string.Equals(t?.Path, originalTexturePath, StringComparison.OrdinalIgnoreCase)) ?? false)
                        {
                            usesTexture = true;
                            textureType = 0;
                        }
                        else if (mat.Normals?.Any(t => string.Equals(t?.Path, originalTexturePath, StringComparison.OrdinalIgnoreCase)) ?? false)
                        {
                            usesTexture = true;
                            textureType = 1;
                        }
                        else if (mat.SpecularMasks?.Any(t => string.Equals(t?.Path, originalTexturePath, StringComparison.OrdinalIgnoreCase)) ?? false)
                        {
                            usesTexture = true;
                            textureType = 2;
                        }
                        else if (mat.Emissive?.Any(t => string.Equals(t?.Path, originalTexturePath, StringComparison.OrdinalIgnoreCase)) ?? false)
                        {
                            usesTexture = true;
                            textureType = 3;
                        }
                    }

                    if (!usesTexture) continue;

                    resolvedTextureType ??= textureType;
                    var materialKey = !string.IsNullOrWhiteSpace(mat.Path) ? mat.Path : mat.Name;
                    if (!matchedMaterialKeys.Add(materialKey)) continue;

                    mat.EnsureProceduralIDMap();
                    matchedMaterialIDs.Add(mat.ProceduralMaterialID);
                    matchedMaterialNames.Add(mat.Name);

                    if (matchedMaterialIDs.Count == MaxPreviewTargetMaterials) break;
                }

                if (matchedMaterialIDs.Count == MaxPreviewTargetMaterials) break;
            }

            if (matchedMaterialIDs.Count == 0)
            {
                Log.Warning("No materials found using original texture path: {Path}", originalTexturePath);
                return;
            }

            UpdatePreviewShaderUniforms(glTex, matchedMaterialIDs, resolvedTextureType ?? 0, 0);
            Log.Information(
                "Loaded custom texture from {File} → Preview on {Count} material(s) [{Materials}]",
                System.IO.Path.GetFileName(customFilePath),
                matchedMaterialNames.Count,
                string.Join(", ", matchedMaterialNames));
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to load custom texture from file");
        }
    }

    private static bool TextureMatches(UTexture texture, string textureName, string texturePathLower)
    {
        return string.Equals(texture.GetPathName(), texturePathLower, StringComparison.OrdinalIgnoreCase) ||
               string.Equals(texture.Name, textureName, StringComparison.OrdinalIgnoreCase);
    }

    private bool TryFindMatchingTextureType(
        PakViewer.Snooper.Shading.Material material,
        string textureName,
        string texturePathLower,
        out int textureType)
    {
        if (material.Parameters.Textures != null)
        {
            foreach (var texParam in material.Parameters.Textures)
            {
                if (texParam.Value.TryLoad(out UTexture texture) && TextureMatches(texture, textureName, texturePathLower))
                {
                    textureType = InferTextureType(texParam.Key, texture.Name);
                    return true;
                }
            }
        }

        var usesInDiffuse = material.Diffuse?.Any(t =>
            t?.Path?.ToLowerInvariant() == texturePathLower ||
            string.Equals(t?.Name, textureName, StringComparison.OrdinalIgnoreCase)) ?? false;
        if (usesInDiffuse)
        {
            textureType = 0;
            return true;
        }

        var usesInNormals = material.Normals?.Any(t =>
            t?.Path?.ToLowerInvariant() == texturePathLower ||
            string.Equals(t?.Name, textureName, StringComparison.OrdinalIgnoreCase)) ?? false;
        if (usesInNormals)
        {
            textureType = 1;
            return true;
        }

        var usesInSpecular = material.SpecularMasks?.Any(t =>
            t?.Path?.ToLowerInvariant() == texturePathLower ||
            string.Equals(t?.Name, textureName, StringComparison.OrdinalIgnoreCase)) ?? false;
        if (usesInSpecular)
        {
            textureType = 2;
            return true;
        }

        var usesInEmissive = material.Emissive?.Any(t =>
            t?.Path?.ToLowerInvariant() == texturePathLower ||
            string.Equals(t?.Name, textureName, StringComparison.OrdinalIgnoreCase)) ?? false;
        if (usesInEmissive)
        {
            textureType = 3;
            return true;
        }

        textureType = 0;
        return false;
    }

    private static int InferTextureType(string parameterName, string textureName)
    {
        if (MatchesParameterType(parameterName, textureName, EmissiveParameterNames, EmissiveParameterRegex))
            return 3;
        if (MatchesParameterType(parameterName, textureName, NormalParameterNames, NormalParameterRegex))
            return 1;
        if (MatchesParameterType(parameterName, textureName, SpecularParameterNames, SpecularParameterRegex))
            return 2;
        if (MatchesParameterType(parameterName, textureName, DiffuseParameterNames, DiffuseParameterRegex))
            return 0;

        var upperTextureName = textureName?.ToUpperInvariant() ?? string.Empty;
        if (upperTextureName.Contains("_N") || upperTextureName.Contains("NORMAL"))
            return 1;
        if (upperTextureName.Contains("_E") || upperTextureName.Contains("EMISS"))
            return 3;
        if (upperTextureName.Contains("RMA") || upperTextureName.Contains("MRA") || upperTextureName.Contains("ORM") ||
            upperTextureName.Contains("SPEC") || upperTextureName.Contains("MASK") || upperTextureName.Contains("_PAK") ||
            upperTextureName.Contains("ROUGH") || upperTextureName.Contains("METAL"))
            return 2;

        return 0;
    }

    private static bool MatchesParameterType(
        string parameterName,
        string textureName,
        HashSet<string> knownNames,
        Regex regex)
    {
        if (knownNames.Contains(parameterName))
            return true;

        if (regex.IsMatch(parameterName))
            return true;

        return regex.IsMatch(textureName ?? string.Empty);
    }

    private PakViewer.Snooper.Shading.Texture BuildTargetMaterialIDsTexture(IReadOnlyList<System.Numerics.Vector3> materialIDs)
    {
        var width = Math.Max(1, materialIDs.Count);
        var bitmap = new SKBitmap(width, 1, SKColorType.Bgra8888, SKAlphaType.Opaque);

        for (var i = 0; i < width; i++)
        {
            var id = i < materialIDs.Count ? materialIDs[i] : System.Numerics.Vector3.Zero;
            var color = new SKColor(
                (byte)Math.Clamp((int)Math.Round(id.X * 255f), 0, 255),
                (byte)Math.Clamp((int)Math.Round(id.Y * 255f), 0, 255),
                (byte)Math.Clamp((int)Math.Round(id.Z * 255f), 0, 255),
                255);
            bitmap.SetPixel(i, 0, color);
        }

        return new PakViewer.Snooper.Shading.Texture(bitmap, "PreviewTargetMaterialIDs", forceLinear: true, generateMipmaps: false);
    }

    private PakViewer.Snooper.Shading.Texture? LoadMaterialIDMap(PakViewer.Snooper.Shading.Material material)
    {

        if (material.Parameters.TryGetTexture2d(out var idTexture, "MaterialIDMap", "IDMap", "ID", "MaterialID", "MatID"))
        {
            if (_renderer.Options.TryGetTexture(idTexture, false, false, out var glTexture))
            {
                Log.Information("Loaded Material ID map from asset: {Name}", idTexture.Name);
                return glTexture;
            }
        }

        material.EnsureProceduralIDMap();
        if (material.ProceduralIDMap != null)
        {
            return material.ProceduralIDMap;
        }

        Log.Warning("Could not obtain Material ID map for '{Name}'", material.Name);
        return null;
    }

    private void UpdatePreviewShaderUniforms(
        PakViewer.Snooper.Shading.Texture previewTexture,
        IReadOnlyList<System.Numerics.Vector3> materialIDs,
        int textureType,
        int channelMode)
    {
        try
        {
            var shader = _renderer.GetMainShader();
            shader.Use();

            shader.TrySetUniform("uPreviewMode", true);

            shader.TrySetUniform("uDebugShowMaterialIDs", false);
            shader.TrySetUniform("uShowPreviewBounds", false);
            shader.TrySetUniform("uPreviewOpacity", 1.0f);

            shader.TrySetUniform("uChannelMode", channelMode);

            _previewTargetIDsTexture?.Dispose();
            _previewTargetIDsTexture = BuildTargetMaterialIDsTexture(materialIDs);
            _previewTargetIDsTexture.Bind(OpenTK.Graphics.OpenGL4.TextureUnit.Texture22);
            shader.TrySetUniform("uTargetMaterialIDsMap", 22);

            previewTexture.Bind(OpenTK.Graphics.OpenGL4.TextureUnit.Texture21);

            shader.TrySetUniform("uPreviewDiffuse", 21);
            shader.TrySetUniform("uPreviewNormals", 21);
            shader.TrySetUniform("uPreviewSpecular", 21);
            shader.TrySetUniform("uPreviewEmissive", 21);

            switch (textureType)
            {
                case 0:
                    Log.Information("Preview texture bound to Diffuse channel");
                    break;
                case 1:
                    Log.Information("Preview texture bound to Normal channel");
                    break;
                case 2:
                    Log.Information("Preview texture bound to Specular channel");
                    break;
                case 3:
                    Log.Information("Preview texture bound to Emissive channel");
                    break;
            }

            var targetCount = Math.Min(materialIDs.Count, MaxPreviewTargetMaterials);
            shader.TrySetUniform("uNumTargetMaterialIDs", targetCount);

            shader.TrySetUniform("uIDMatchTolerance", 0.1f);

            shader.TrySetUniform("uPreviewTextureType", textureType);

            Log.Information("Shader uniforms updated for preview mode ({Count} target material IDs)", targetCount);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to update shader uniforms for preview");
        }
    }

    public void SetDebugMaterialIDs(bool enabled)
    {
        _actionQueue.Enqueue(() =>
        {
            try
            {
                var shader = _renderer.GetMainShader();
                shader.Use();
                shader.TrySetUniform("uDebugShowMaterialIDs", enabled);
                Log.Information("Debug Material ID visualization: {State}", enabled ? "ON" : "OFF");
            }
            catch (Exception ex)
            {
                Log.Warning(ex, "Failed to toggle Material ID debug visualization");
            }
        });
    }

    public void SetPreviewBoundsVisible(bool enabled)
    {
        _actionQueue.Enqueue(() =>
        {
            try
            {
                var shader = _renderer.GetMainShader();
                shader.Use();
                shader.TrySetUniform("uShowPreviewBounds", enabled);
            }
            catch (Exception ex)
            {
                Log.Warning(ex, "Failed to toggle preview bounds visualization");
            }
        });
    }

    public void SetPreviewOpacity(float opacity)
    {
        var clamped = Math.Clamp(opacity, 0f, 1f);
        _actionQueue.Enqueue(() =>
        {
            try
            {
                var shader = _renderer.GetMainShader();
                shader.Use();
                shader.TrySetUniform("uPreviewOpacity", clamped);
            }
            catch (Exception ex)
            {
                Log.Warning(ex, "Failed to set preview opacity");
            }
        });
    }

    private List<DependencyNode> BuildDependencyGraph()
    {
        var nodes = new List<DependencyNode>();

        foreach (var kvp in _renderer.Options.Models)
        {
            var model = kvp.Value;
            var meshPath = model.Path;

            string? vfsName = null;
            var packagePath = meshPath.Contains('.') ? meshPath[..meshPath.LastIndexOf('.')] : meshPath;
            if (SnooperContext.Provider?.TryGetGameFile(packagePath, out var meshFile) == true && meshFile is VfsEntry vfsEntry)
                vfsName = vfsEntry.Vfs.Name;

            DependencyNode? node = null;
            if (model is SkeletalModel skModel && _currentExport is USkeletalMesh skMesh)
                node = DependencyGraphBuilder.BuildSkeletalGraph(skModel, skMesh, vfsName);
            else if (model is StaticModel stModel && _currentExport is UStaticMesh stMesh)
                node = DependencyGraphBuilder.BuildStaticGraph(stModel, stMesh, vfsName);

            if (node != null)
                nodes.Add(node);
        }

        return nodes;
    }

    private void ShowHelpDialog()
    {

        Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread().TryEnqueue(() =>
        {

            if (_settingsWindow != null)
            {
                _settingsWindow.Activate();
                return;
            }

            if (_materialData == null && _renderer.Options.Models.Count > 0)
            {
                _materialData = ExtractMaterialDataFromModels();
            }

            var meshSectionsList = new List<MeshSectionInfo>();
            var model = _renderer.Options.Models.Values.FirstOrDefault();
            var sections = model?.Sections;
            if (sections != null)
            {
                for (var i = 0; i < sections.Length; i++)
                {
                    var matName = model?.Materials != null && sections[i].MaterialIndex >= 0 && sections[i].MaterialIndex < model.Materials.Length
                        ? model.Materials[sections[i].MaterialIndex].Name
                        : $"Section {i}";

                    var index = i;
                    meshSectionsList.Add(new MeshSectionInfo
                    {
                        Shortcut = i.ToString(),
                        Name = matName,
                        Status = sections[i].Show ? "Visible" : "Hidden",
                        SectionIndex = i,
                        ToggleAction = () => ToggleSection(index)
                    });
                }
            }

            var dependencyNodes = BuildDependencyGraph();

            var loadedMaterialNames = new List<string>();
            var seenNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (var m in _renderer.Options.Models.Values)
            {
                if (m.Materials == null) continue;
                foreach (var mat in m.Materials)
                {
                    if (!string.IsNullOrEmpty(mat.Name) && seenNames.Add(mat.Name))
                        loadedMaterialNames.Add(mat.Name);
                }
            }

            _settingsWindow = new ViewerSettingsWindow(
                _screenshotPath,
                meshSectionsList,
                _materialData,
                dependencyNodes,
                loadedMaterialNames,
                BuildBlockedMaterialEntries());
            _settingsWindow.TextureClicked += OnTextureClicked;
            _settingsWindow.ResetOriginalTexturesClicked += OnResetOriginalTextures;
            _settingsWindow.BlockedMaterialVisibilityChanged += (matName, show) => SetMaterialVisibility(matName, show);
            _settingsWindow.BlockedMaterialRemoved += matName => RemoveBlockedMaterial(matName);
            _settingsWindow.ExportTextureClicked += OnExportTexture;
            _settingsWindow.LoadCustomTextureClicked += OnLoadCustomTexture;
            _settingsWindow.MaterialVisibilityToggled += OnMaterialVisibilityToggled;
            _settingsWindow.Closed += (s, e) =>
            {
                if (_settingsWindow != null)
                {
                     _screenshotPath = _settingsWindow.ScreenshotPath;
                     _settingsWindow = null;
                }

                if (_materialData != null)
                {
                    _materialData.Dispose();
                    _materialData = null;
                }
            };
            _settingsWindow.Activate();
        });
    }

    private void TakeScreenshot()
    {
        try
        {
            var width = ClientSize.X;
            var height = ClientSize.Y;
            var pixels = new byte[width * height * 4];

            GL.ReadPixels(0, 0, width, height, PixelFormat.Rgba, PixelType.UnsignedByte, pixels);

            var flipped = new byte[pixels.Length];
            var stride = width * 4;
            for (var y = 0; y < height; y++)
                Array.Copy(pixels, y * stride, flipped, (height - 1 - y) * stride, stride);

            Directory.CreateDirectory(_screenshotPath);

            var timestamp = DateTime.Now.ToString("yyyyMMdd_HHmmss");
            var filename = $"Screenshot_{timestamp}.png";
            var path = Path.Combine(_screenshotPath, filename);

            using var image = SixLabors.ImageSharp.Image.LoadPixelData<Rgba32>(flipped, width, height);
            using (var fs = File.Open(path, FileMode.Create, FileAccess.Write, FileShare.Read))
            {
                image.Save(fs, new PngEncoder());
            }

            Log.Information("Screenshot saved: {Path}", path);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to save screenshot");
        }
    }

    private void DrawStatsOverlay()
    {

        var model = _renderer.Options.Models.Values.FirstOrDefault();
        if (model == null) return;

        var totalVerts = model.Indices?.Length ?? 0;
        var totalPolys = totalVerts / 3;

        model.Box.GetCenterAndExtents(out var center, out var extents);
        var boundsX = extents.X * 2;
        var boundsY = extents.Y * 2;
        var boundsZ = extents.Z * 2;

        var animText = "";
        var anims = _renderer.Options.Animations;
        if (anims.Count > 0)
        {
            var selected = _renderer.Options.SelectedAnimation;
            var animName = selected >= 0 && selected < anims.Count ? anims[selected].Name : "?";
            animText = $" | Anim: {anims.Count} ({animName})";
        }

        Title = $"PakViewer - FPS: {_currentFps} | Verts: {totalVerts:N0} | Tris: {totalPolys:N0} | Bounds: {boundsX:F1}x{boundsY:F1}x{boundsZ:F1} | Wireframe: {(_showWireframe ? "ON" : "OFF")}{animText} | H Settings";
    }

    protected override void OnMouseMove(MouseMoveEventArgs e)
    {
        base.OnMouseMove(e);

        if (_firstMouseMove)
        {
            _lastMousePos = new Vector2(e.X, e.Y);
            _firstMouseMove = false;
            return;
        }

        var currentMousePos = new Vector2(e.X, e.Y);
        var delta = currentMousePos - _lastMousePos;
        _lastMousePos = currentMousePos;

        if (_leftDown && !_leftDragging)
        {
            var dragDelta = currentMousePos - _leftDownPos;
            if (dragDelta.Length >= ClickDragThresholdPx)
                _leftDragging = true;
        }

        if (MouseState.IsButtonDown(MouseButton.Left))
            _renderer.CameraOp.Modify(new System.Numerics.Vector2(delta.X, delta.Y));
        else if (MouseState.IsButtonDown(MouseButton.Middle))
            PanCamera(delta);
    }

    protected override void OnMouseDown(MouseButtonEventArgs e)
    {
        base.OnMouseDown(e);

        if (e.Button == MouseButton.Left)
        {
            _leftDown = true;
            _leftDragging = false;
            _leftDownPos = MouseState.Position;
        }
    }

    protected override void OnMouseUp(MouseButtonEventArgs e)
    {
        base.OnMouseUp(e);

        if (e.Button == MouseButton.Left)
        {
            var wasDragging = _leftDragging;
            _leftDown = false;
            _leftDragging = false;

            if (!wasDragging)
                SelectByPicking(MouseState.Position);

            Focus();
        }
    }

    protected override void OnMouseWheel(MouseWheelEventArgs e)
    {
        base.OnMouseWheel(e);

        if (e.OffsetY == 0)
            return;

        ZoomCameraByDistance(e.OffsetY);
    }

    private void PanCamera(Vector2 delta)
    {

        var cam = _renderer.CameraOp;

        var toTarget = cam.Direction - cam.Position;
        var distance = MathF.Max(0.001f, toTarget.Length());
        var moveAxis = System.Numerics.Vector3.Normalize(toTarget);
        var right = System.Numerics.Vector3.Normalize(System.Numerics.Vector3.Cross(moveAxis, System.Numerics.Vector3.UnitY));
        var up = System.Numerics.Vector3.UnitY;

        var panSpeed = distance * 0.001f;
        var pan = (-right * delta.X + up * delta.Y) * panSpeed;

        cam.Position += pan;
        cam.Direction += pan;
    }

    private void ZoomCameraByDistance(float wheelDelta)
    {
        var cam = _renderer.CameraOp;

        var toTarget = cam.Direction - cam.Position;
        var distance = MathF.Max(0.001f, toTarget.Length());
        var moveAxis = System.Numerics.Vector3.Normalize(toTarget);

        var zoomSpeed = distance * 0.08f;
        var newDistance = MathF.Max(0.05f, distance - wheelDelta * zoomSpeed);
        cam.Position = cam.Direction - moveAxis * newDistance;
    }

    private void SelectByPicking(Vector2 mousePos)
    {

        var guid = _renderer.Picking.ReadPixel(
            new System.Numerics.Vector2(mousePos.X, mousePos.Y),
            new System.Numerics.Vector2(0, 0),
            new System.Numerics.Vector2(ClientSize.X, ClientSize.Y));

        _renderer.Options.SelectModel(guid);
    }

    protected override void OnResize(ResizeEventArgs e)
    {
        base.OnResize(e);

        GL.Viewport(0, 0, e.Width, e.Height);
        _renderer.WindowResized(e.Width, e.Height);
    }

    protected override void OnClosing(System.ComponentModel.CancelEventArgs e)
    {
        _previewTargetIDsTexture?.Dispose();
        _previewTargetIDsTexture = null;
        _renderer.Save();
        _renderer.Dispose();
        SnooperContext.Status = null;

        if (_settingsWindow != null)
        {
            try { _settingsWindow.Close(); } catch { }
            _settingsWindow = null;
        }
        _materialData?.Dispose();
        _materialData = null;
        _currentExport = null;

        GC.Collect();
        GC.WaitForPendingFinalizers();
        GC.Collect();

        base.OnClosing(e);
    }
}
