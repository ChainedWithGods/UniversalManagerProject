using System;
using System.Numerics;
using System.IO;
using System.Reflection;
using CUE4Parse_Conversion.Textures;
using CUE4Parse.UE4.Assets.Exports.Texture;
using CUE4Parse.UE4.Objects.Core.Math;
using CUE4Parse.UE4.Objects.Core.Misc;
using OpenTK.Graphics.OpenGL4;
using SixLaborsImage = SixLabors.ImageSharp.Image;
using SixLabors.ImageSharp.PixelFormats;
using SkiaSharp;

namespace PakViewer.Snooper.Shading;

public class Texture : IDisposable
{
    private readonly int _handle;
    private readonly TextureType _type;
    private readonly TextureTarget _target;

    public readonly string Type;
    public readonly FGuid Guid;
    public readonly string Name;
    public readonly string Path;
    public readonly EPixelFormat Format;
    public readonly uint ImportedWidth;
    public readonly uint ImportedHeight;
    public int Width;
    public int Height;
    public bool IsShared { get; set; } = false;

    private const int DisabledChannel = (int)BlendingFactor.Zero;
    private readonly bool[] _values = [true, true, true, true];
    private readonly string[] _labels = ["R", "G", "B", "A"];
    public int[] SwizzleMask =
    [
        (int) PixelFormat.Red,
        (int) PixelFormat.Green,
        (int) PixelFormat.Blue,
        (int) PixelFormat.Alpha
    ];

    private Texture(TextureType type)
    {
        _handle = GL.GenTexture();
        _type = type;
        _target = _type switch
        {
            TextureType.Cubemap => TextureTarget.TextureCubeMap,
            TextureType.MsaaFramebuffer => TextureTarget.Texture2DMultisample,
            _ => TextureTarget.Texture2D
        };

        Guid = new FGuid();
    }

    public Texture(uint width, uint height) : this(TextureType.MsaaFramebuffer)
    {
        Width = (int) width;
        Height = (int) height;
        Bind(TextureUnit.Texture0);

        GL.TexImage2DMultisample(TextureTargetMultisample.Texture2DMultisample, Constants.SAMPLES_COUNT, PixelInternalFormat.Rgb, Width, Height, true);
        GL.FramebufferTexture2D(FramebufferTarget.Framebuffer, FramebufferAttachment.ColorAttachment0, _target, _handle, 0);
    }

    public Texture(int width, int height) : this(TextureType.Framebuffer)
    {
        Width = width;
        Height = height;
        Bind(TextureUnit.Texture0);

        GL.TexImage2D(_target, 0, PixelInternalFormat.Rgb, Width, Height, 0, PixelFormat.Rgb, PixelType.UnsignedByte, IntPtr.Zero);

        GL.TexParameter(_target, TextureParameterName.TextureMinFilter, (int) TextureMinFilter.Linear);
        GL.TexParameter(_target, TextureParameterName.TextureMagFilter, (int) TextureMagFilter.Linear);
        GL.TexParameter(_target, TextureParameterName.TextureWrapS, (int) TextureWrapMode.ClampToEdge);
        GL.TexParameter(_target, TextureParameterName.TextureWrapT, (int) TextureWrapMode.ClampToEdge);

        GL.FramebufferTexture2D(FramebufferTarget.Framebuffer, FramebufferAttachment.ColorAttachment0, _target, _handle, 0);
    }

    public Texture(SKBitmap bitmap, UTexture texture2D, bool forceLinear = false) : this(TextureType.Normal)
    {
        Type = texture2D.ExportType;
        Guid = texture2D.LightingGuid;
        Name = texture2D.Name;
        Path = texture2D.GetPathName();
        Format = texture2D.Format;
        Width = bitmap.Width;
        Height = bitmap.Height;
        Bind(TextureUnit.Texture0);

        var useSrgb = !forceLinear && texture2D.SRGB;

        var internalFormat = bitmap.ColorType switch
        {
            SKColorType.Gray8 => PixelInternalFormat.R8,
            _ => useSrgb ? PixelInternalFormat.Srgb : PixelInternalFormat.Rgb
        };

        var pixelFormat = bitmap.ColorType switch
        {
            SKColorType.Gray8 => PixelFormat.Red,
            SKColorType.Bgra8888 => PixelFormat.Bgra,
            _ => PixelFormat.Rgba
        };

        GL.TexImage2D(_target, 0, internalFormat, Width, Height, 0, pixelFormat, PixelType.UnsignedByte, bitmap.Bytes);
        GL.TexParameter(_target, TextureParameterName.TextureMinFilter, (int) TextureMinFilter.Linear);
        GL.TexParameter(_target, TextureParameterName.TextureMagFilter, (int) TextureMagFilter.Linear);
        GL.TexParameter(_target, TextureParameterName.TextureBaseLevel, 0);
        GL.TexParameter(_target, TextureParameterName.TextureMaxLevel, 4);

        GL.GenerateMipmap(GenerateMipmapTarget.Texture2D);
        bitmap.Dispose();
    }

    public Texture(SKBitmap bitmap, string name, bool forceLinear = true, bool generateMipmaps = false) : this(TextureType.Normal)
    {
        Type = "Runtime";
        Name = name;
        Path = name;
        Width = bitmap.Width;
        Height = bitmap.Height;
        Bind(TextureUnit.Texture0);

        var internalFormat = bitmap.ColorType switch
        {
            SKColorType.Gray8 => PixelInternalFormat.R8,
            _ => forceLinear ? PixelInternalFormat.Rgba : PixelInternalFormat.SrgbAlpha
        };

        var pixelFormat = bitmap.ColorType switch
        {
            SKColorType.Gray8 => PixelFormat.Red,
            SKColorType.Bgra8888 => PixelFormat.Bgra,
            _ => PixelFormat.Rgba
        };

        GL.TexImage2D(_target, 0, internalFormat, Width, Height, 0, pixelFormat, PixelType.UnsignedByte, bitmap.Bytes);
        GL.TexParameter(_target, TextureParameterName.TextureMinFilter, (int)(generateMipmaps ? TextureMinFilter.LinearMipmapLinear : TextureMinFilter.Nearest));
        GL.TexParameter(_target, TextureParameterName.TextureMagFilter, (int)TextureMagFilter.Nearest);
        GL.TexParameter(_target, TextureParameterName.TextureWrapS, (int)TextureWrapMode.ClampToEdge);
        GL.TexParameter(_target, TextureParameterName.TextureWrapT, (int)TextureWrapMode.ClampToEdge);
        GL.TexParameter(_target, TextureParameterName.TextureBaseLevel, 0);
        GL.TexParameter(_target, TextureParameterName.TextureMaxLevel, generateMipmaps ? 4 : 0);

        if (generateMipmaps)
            GL.GenerateMipmap(GenerateMipmapTarget.Texture2D);

        bitmap.Dispose();
    }

    public Texture(FLinearColor color) : this(TextureType.Normal)
    {
        Type = "LinearColor";
        Name = color.Hex;
        Width = 1;
        Height = 1;
        Bind(TextureUnit.Texture0);

        GL.TexImage2D(_target, 0, PixelInternalFormat.Rgba, Width, Height, 0, PixelFormat.Rgba, PixelType.Float, ref color);
        GL.TexParameter(_target, TextureParameterName.TextureMinFilter, (int) TextureMinFilter.LinearMipmapLinear);
        GL.TexParameter(_target, TextureParameterName.TextureMagFilter, (int) TextureMagFilter.Linear);
        GL.TexParameter(_target, TextureParameterName.TextureBaseLevel, 0);
        GL.TexParameter(_target, TextureParameterName.TextureMaxLevel, 8);

        GL.GenerateMipmap(GenerateMipmapTarget.Texture2D);
    }

    public Texture(string[] textures) : this(TextureType.Cubemap)
    {
        Bind(TextureUnit.Texture0);

        for (int t = 0; t < textures.Length; t++)
        {
            ProcessPixels(textures[t], TextureTarget.TextureCubeMapPositiveX + t);
        }

        GL.TexParameter(_target, TextureParameterName.TextureMinFilter, (int) TextureMinFilter.LinearMipmapLinear);
        GL.TexParameter(_target, TextureParameterName.TextureMagFilter, (int) TextureMagFilter.Linear);
        GL.TexParameter(_target, TextureParameterName.TextureWrapR, (int) TextureWrapMode.ClampToEdge);
        GL.TexParameter(_target, TextureParameterName.TextureWrapS, (int) TextureWrapMode.ClampToEdge);
        GL.TexParameter(_target, TextureParameterName.TextureWrapT, (int) TextureWrapMode.ClampToEdge);

        GL.GenerateMipmap(GenerateMipmapTarget.TextureCubeMap);
    }

    public Texture(string texture) : this(TextureType.Normal)
    {
        Bind(TextureUnit.Texture0);

        ProcessPixels(texture, _target);

        GL.TexParameter(_target, TextureParameterName.TextureMinFilter, (int) TextureMinFilter.Linear);
        GL.TexParameter(_target, TextureParameterName.TextureMagFilter, (int) TextureMagFilter.Linear);
        GL.TexParameter(_target, TextureParameterName.TextureWrapR, (int) TextureWrapMode.ClampToEdge);
        GL.TexParameter(_target, TextureParameterName.TextureWrapS, (int) TextureWrapMode.ClampToEdge);
        GL.TexParameter(_target, TextureParameterName.TextureWrapT, (int) TextureWrapMode.ClampToEdge);
    }

    private void ProcessPixels(string texture, TextureTarget target)
    {

        var executingAssembly = Assembly.GetExecutingAssembly();
        var executingAssemblyName = executingAssembly.GetName().Name;

        Stream? stream = executingAssembly.GetManifestResourceStream($"{executingAssemblyName}.Resources.{texture}.png")
                       ?? executingAssembly.GetManifestResourceStream($"{executingAssemblyName}.Resources.Icons.{texture}.png")
                       ?? executingAssembly.GetManifestResourceStream($"{executingAssemblyName}.Resources.Textures.{texture}.png");

        if (stream == null)
        {
            Width = 1;
            Height = 1;
            Span<byte> pixel = stackalloc byte[4] { 255, 255, 255, 255 };
            GL.TexImage2D(target, 0, PixelInternalFormat.Rgba8, Width, Height, 0, PixelFormat.Rgba, PixelType.UnsignedByte, pixel.ToArray());
            return;
        }

        using var img = SixLaborsImage.Load<Rgba32>(stream);
        Width = img.Width;
        Height = img.Height;
        GL.TexImage2D(target, 0, PixelInternalFormat.Rgba8, Width, Height, 0, PixelFormat.Rgba, PixelType.UnsignedByte, IntPtr.Zero);
        img.ProcessPixelRows(accessor =>
        {
            for (int y = 0; y < accessor.Height; y++)
            {
                GL.TexSubImage2D(target, 0, 0, y, accessor.Width, 1, PixelFormat.Rgba, PixelType.UnsignedByte, accessor.GetRowSpan(y).ToArray());
            }
        });
    }

    public void Bind(TextureUnit textureSlot)
    {
        GL.ActiveTexture(textureSlot);
        Bind(_target);
    }

    public void Bind(TextureTarget target)
    {
        GL.BindTexture(target, _handle);
    }

    public void Bind()
    {
        GL.BindTexture(_target, _handle);
    }

    public void Swizzle()
    {
        Bind();
        GL.TexParameter(_target, TextureParameterName.TextureSwizzleRgba, SwizzleMask);
    }

    public IntPtr GetPointer() => (IntPtr) _handle;

    public void WindowResized(int width, int height)
    {
        Width = width;
        Height = height;

        Bind();
        switch (_type)
        {
            case TextureType.MsaaFramebuffer:
                GL.TexImage2DMultisample(TextureTargetMultisample.Texture2DMultisample, Constants.SAMPLES_COUNT, PixelInternalFormat.Rgb, Width, Height, true);
                GL.FramebufferTexture2D(FramebufferTarget.Framebuffer, FramebufferAttachment.ColorAttachment0, _target, _handle, 0);
                break;
            case TextureType.Framebuffer:
                GL.TexImage2D(_target, 0, PixelInternalFormat.Rgb, Width, Height, 0, PixelFormat.Rgb, PixelType.UnsignedByte, IntPtr.Zero);
                GL.FramebufferTexture2D(FramebufferTarget.Framebuffer, FramebufferAttachment.ColorAttachment0, _target, _handle, 0);
                break;
            default:
                throw new NotSupportedException();
        }
    }

    private bool _disposed;

    public void Dispose()
    {
        if (_disposed) return;
        GL.DeleteTexture(_handle);
        _disposed = true;
    }

    private Vector3 _scrolling = new (0.0f, 0.0f, 1.0f);

}

public enum TextureType
{
    Normal,
    Cubemap,
    Framebuffer,
    MsaaFramebuffer
}
