using System;
using System.IO;
using System.Threading.Tasks;
using Microsoft.UI.Xaml.Media.Imaging;
using SkiaSharp;
using CUE4Parse.FileProvider;
using CUE4Parse.UE4.Assets.Exports.Texture;
using CUE4Parse_Conversion.Textures;
using PakViewer.Snooper;
using Serilog;
using Microsoft.UI.Dispatching;
using Svg.Skia;

namespace PakViewer.Helpers;

public static class TextureLoader
{
    public static async Task<BitmapImage?> LoadFullImageAsync(IFileProvider provider, string filePath, DispatcherQueue dispatcher)
    {
        if (dispatcher == null) return null;

        try
        {
            var ext = Path.GetExtension(filePath).ToLowerInvariant();
            MemoryStream? stream;

            if (IsStandaloneImageExtension(ext))
            {
                stream = await LoadStandaloneImageStreamAsync(provider, filePath, ext, targetMaxEdge: 0);
            }
            else
            {
                stream = await Task.Run(() => LoadTexturePackageStream(provider, filePath, targetMaxEdge: 0));
            }

            return await CreateBitmapImageOnUiThreadAsync(stream, dispatcher, "Failed to create BitmapImage");
        }
        catch (Exception ex)
        {
            Log.Error(ex, $"Failed to load image for {filePath}");
            return null;
        }
    }

    public static async Task<BitmapImage?> LoadThumbnailAsync(IFileProvider provider, string filePath, DispatcherQueue dispatcher)
    {
        if (dispatcher == null) return null;

        try
        {
            var ext = Path.GetExtension(filePath).ToLowerInvariant();
            MemoryStream? stream;

            if (IsStandaloneImageExtension(ext))
            {
                stream = await LoadStandaloneImageStreamAsync(provider, filePath, ext, targetMaxEdge: 256);
            }
            else
            {
                stream = await Task.Run(() => LoadTexturePackageStream(provider, filePath, targetMaxEdge: 256));
            }

            return await CreateBitmapImageOnUiThreadAsync(stream, dispatcher, "Failed to create BitmapImage for thumbnail");
        }
        catch (Exception ex)
        {
            Log.Error(ex, $"Failed to load thumbnail for {filePath}");
            return null;
        }
    }

    private static bool IsStandaloneImageExtension(string ext)
    {
        return ext == ".png" || ext == ".jpg" || ext == ".bmp" || ext == ".svg";
    }

    private static async Task<MemoryStream?> LoadStandaloneImageStreamAsync(
        IFileProvider provider,
        string filePath,
        string ext,
        int targetMaxEdge)
    {
        if (!provider.Files.TryGetValue(filePath, out var file))
            return null;

        try
        {
            var data = await file.ReadAsync();
            return DecodeStandaloneImageToPngStream(data, ext, targetMaxEdge);
        }
        catch (Exception ex)
        {
            Log.Error(ex, $"Error processing standalone image for {filePath}");
            return null;
        }
    }

    private static MemoryStream? LoadTexturePackageStream(IFileProvider provider, string filePath, int targetMaxEdge)
    {
        if (!provider.Files.ContainsKey(filePath))
            return null;

        try
        {
            var package = provider.LoadPackage(filePath);

            foreach (var exportLazy in package.ExportsLazy)
            {
                var obj = exportLazy.Value;
                if (obj is not UTexture2D texture)
                    continue;

                var decoded = texture.Decode(UserSettings.Default.ExportOptions);
                if (decoded == null)
                {
                    Log.Debug($"Texture.Decode() returned null for {filePath}");
                    continue;
                }

                using var bitmap = decoded.ToSkBitmap();
                if (bitmap == null)
                {
                    Log.Debug($"ToSkBitmap() returned null for {filePath}");
                    continue;
                }

                return EncodeBitmapToPng(bitmap, targetMaxEdge);
            }
        }
        catch (Exception ex)
        {
            Log.Error(ex, $"Error processing texture package image for {filePath}");
        }

        return null;
    }

    private static MemoryStream? DecodeStandaloneImageToPngStream(byte[] data, string ext, int targetMaxEdge)
    {
        try
        {
            if (ext == ".svg")
            {
                using var svgStream = new MemoryStream(data) { Position = 0 };
                var svg = new SKSvg();
                svg.Load(svgStream);
                if (svg.Picture == null)
                    return null;

                var size = targetMaxEdge > 0 ? targetMaxEdge : 1024;
                using var bitmap = new SKBitmap(size, size);
                using var canvas = new SKCanvas(bitmap);
                canvas.Clear(SKColors.Transparent);

                var bounds = svg.Picture.CullRect;
                if (bounds.Width <= 0 || bounds.Height <= 0)
                    return null;

                float scale = Math.Min(size / bounds.Width, size / bounds.Height);
                canvas.Scale(scale);
                canvas.Translate(-bounds.Left, -bounds.Top);
                canvas.DrawPicture(svg.Picture);

                return EncodeBitmapToPng(bitmap, targetMaxEdge);
            }

            using var rasterStream = new MemoryStream(data) { Position = 0 };
            using var bitmapDecoded = SKBitmap.Decode(rasterStream);
            if (bitmapDecoded == null)
                return null;

            return EncodeBitmapToPng(bitmapDecoded, targetMaxEdge);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to decode standalone image");
            return null;
        }
    }

    private static MemoryStream? EncodeBitmapToPng(SKBitmap sourceBitmap, int targetMaxEdge)
    {
        SKBitmap? resized = null;
        try
        {
            SKBitmap bitmapToEncode = sourceBitmap;
            if (targetMaxEdge > 0)
            {
                float scale = Math.Min((float)targetMaxEdge / sourceBitmap.Width, (float)targetMaxEdge / sourceBitmap.Height);
                if (scale < 1.0f)
                {
                    var info = new SKImageInfo((int)(sourceBitmap.Width * scale), (int)(sourceBitmap.Height * scale));
                    resized = sourceBitmap.Resize(info, SKSamplingOptions.Default);
                    if (resized != null)
                    {
                        bitmapToEncode = resized;
                    }
                }
            }

            var ms = new MemoryStream();
            if (!bitmapToEncode.Encode(ms, SKEncodedImageFormat.Png, 90))
            {
                ms.Dispose();
                return null;
            }

            ms.Position = 0;
            return ms;
        }
        finally
        {
            resized?.Dispose();
        }
    }

    private static async Task<BitmapImage?> CreateBitmapImageOnUiThreadAsync(
        MemoryStream? stream,
        DispatcherQueue dispatcher,
        string errorMessage)
    {
        if (stream == null)
            return null;

        if (dispatcher.HasThreadAccess)
        {
            try
            {
                var bitmapImage = new BitmapImage();
                bitmapImage.SetSource(stream.AsRandomAccessStream());
                return bitmapImage;
            }
            catch (Exception ex)
            {
                stream.Dispose();
                Log.Error(ex, errorMessage);
                return null;
            }
        }

        var tcs = new TaskCompletionSource<BitmapImage?>();
        bool enqueued = dispatcher.TryEnqueue(() =>
        {
            try
            {
                var bitmapImage = new BitmapImage();
                bitmapImage.SetSource(stream.AsRandomAccessStream());
                tcs.SetResult(bitmapImage);
            }
            catch (Exception ex)
            {
                stream.Dispose();
                Log.Error(ex, errorMessage);
                tcs.SetResult(null);
            }
        });

        if (!enqueued)
        {
            stream.Dispose();
            tcs.SetResult(null);
        }

        return await tcs.Task;
    }
}
