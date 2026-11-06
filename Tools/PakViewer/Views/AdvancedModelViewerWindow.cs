using OpenTK.Graphics.OpenGL4;
using OpenTK.Mathematics;
using OpenTK.Windowing.Common;
using OpenTK.Windowing.Desktop;
using OpenTK.Windowing.GraphicsLibraryFramework;
using Serilog;
using System;
using System.Threading;
using CUE4Parse.UE4.Assets.Exports;
using CUE4Parse.UE4.Assets.Exports.SkeletalMesh;
using CUE4Parse.UE4.Assets.Exports.StaticMesh;
using PakViewer.Snooper;
using PakViewer.Snooper.Buffers;

namespace PakViewer.Views;

public class AdvancedModelViewerWindow : GameWindow
{
    public readonly FramebufferObject Framebuffer;
    public readonly Renderer Renderer;

    private bool _init;
    private Vector2 _lastMousePos;
    private bool _firstMouseMove = true;

    public AdvancedModelViewerWindow(GameWindowSettings gwSettings, NativeWindowSettings nwSettings)
        : base(gwSettings, nwSettings)
    {
        Framebuffer = new FramebufferObject(ClientSize);
        Renderer = new Renderer(ClientSize.X, ClientSize.Y);
        _init = false;
    }

    public bool TryLoadSkeletalMesh(USkeletalMesh mesh)
    {
        try
        {
            var lazyExport = new Lazy<UObject>(() => mesh);
            Renderer.Load(CancellationToken.None, mesh, lazyExport);
            return Renderer.Options.Models.Count > 0;
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to load skeletal mesh into renderer");
            return false;
        }
    }

    public bool TryLoadStaticMesh(UStaticMesh mesh)
    {
        try
        {
            var lazyExport = new Lazy<UObject>(() => mesh);
            Renderer.Load(CancellationToken.None, mesh, lazyExport);
            return Renderer.Options.Models.Count > 0;
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to load static mesh into renderer");
            return false;
        }
    }

    protected override void OnLoad()
    {
        if (_init)
        {
            Renderer.Options.SetupModelsAndLights();
            return;
        }

        base.OnLoad();
        CenterWindow();

        unsafe
        {
            GLFW.SetWindowAttrib(WindowPtr, WindowAttribute.Floating, true);
        }

        GL.ClearColor(Color4.Black);
        GL.Enable(EnableCap.Blend);
        GL.Enable(EnableCap.CullFace);
        GL.Enable(EnableCap.DepthTest);
        GL.Enable(EnableCap.Multisample);
        GL.Enable(EnableCap.VertexProgramPointSize);
        GL.StencilOp(StencilOp.Keep, StencilOp.Replace, StencilOp.Replace);
        GL.BlendFunc(BlendingFactor.SrcAlpha, BlendingFactor.OneMinusSrcAlpha);

        Framebuffer.Setup();
        Renderer.Setup();
        _init = true;

        Log.Information("Advanced Model Viewer Window initialized with OpenGL 4.6");
    }

    private void ClearWhatHasBeenDrawn()
    {
        GL.Clear(ClearBufferMask.ColorBufferBit | ClearBufferMask.DepthBufferBit | ClearBufferMask.StencilBufferBit);
    }

    protected override void OnRenderFrame(FrameEventArgs args)
    {
        base.OnRenderFrame(args);
        if (!IsVisible)
            return;

        ClearWhatHasBeenDrawn();
        Framebuffer.Bind();
        ClearWhatHasBeenDrawn();

        Renderer.Render();

        Framebuffer.BindMsaa();
        Framebuffer.Bind(0);

        SwapBuffers();
    }

    protected override void OnUpdateFrame(FrameEventArgs e)
    {
        base.OnUpdateFrame(e);
        if (!IsVisible)
            return;

        var delta = (float)e.Time;

        if (KeyboardState.IsKeyDown(Keys.Escape))
        {
            Close();
        }

        Renderer.CameraOp.Modify(KeyboardState, delta);

        if (MouseState.ScrollDelta.Y != 0)
        {
            var zoomAmount = MouseState.ScrollDelta.Y * 2f;
            Renderer.CameraOp.Zoom = Math.Clamp(Renderer.CameraOp.Zoom - zoomAmount, 1.0f, 89f);
        }
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

        if (MouseState.IsButtonDown(MouseButton.Right))
        {
            Renderer.CameraOp.Modify(new System.Numerics.Vector2(delta.X, delta.Y));
        }
    }

    protected override void OnResize(ResizeEventArgs e)
    {
        base.OnResize(e);

        GL.Viewport(0, 0, e.Width, e.Height);

        Framebuffer.WindowResized(e.Width, e.Height);
        Renderer.CameraOp.AspectRatio = (float)e.Width / e.Height;
    }

    protected override void OnUnload()
    {
        base.OnUnload();

        Framebuffer?.Dispose();
        Renderer?.Dispose();
    }
}
