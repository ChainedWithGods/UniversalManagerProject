using OpenTK.Graphics.OpenGL4;

namespace PakViewer.Snooper.Shading;

public static class TextureHelper
{
    public static void FixChannels(Texture texture, string? hint)
    {
        if (texture == null)
            return;

        var h = (hint ?? string.Empty).ToUpperInvariant();
        if (string.IsNullOrWhiteSpace(h))
            return;

        if (h.Contains("_ORM") || h.Contains("ORM") || h.Contains("OCCLUSIONROUGHNESSMETALLIC") || h.Contains("ARM") || h.Contains("METALROUGH") || h.Contains("METALROUGHNESS") || h.Contains("ROUGHNESSMETALLIC"))
        {
            texture.SwizzleMask =
            [
                (int) PixelFormat.Red,
                (int) PixelFormat.Blue,
                (int) PixelFormat.Green,
                (int) PixelFormat.Alpha
            ];
            texture.Swizzle();
            return;
        }

        if (h.Contains("MRA") || h.Contains("MRO") || h.Contains("MGA") || h.Contains("METROUGH") || h.Contains("METALLICROUGHNESS"))
        {
            texture.SwizzleMask =
            [
                (int) PixelFormat.Blue,
                (int) PixelFormat.Red,
                (int) PixelFormat.Green,
                (int) PixelFormat.Alpha
            ];
            texture.Swizzle();
            return;
        }

        if (h.Contains("RMA") || h.Contains("RMO") || h.Contains("ROUGHNESSMETALLICAO") || h.Contains("ROUGHNESSMETALLICAO"))
        {
            texture.SwizzleMask =
            [
                (int) PixelFormat.Blue,
                (int) PixelFormat.Green,
                (int) PixelFormat.Red,
                (int) PixelFormat.Alpha
            ];
            texture.Swizzle();
        }
    }
}
