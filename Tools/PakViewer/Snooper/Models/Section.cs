using System;
using System.Numerics;
using PakViewer.Snooper.Shading;

namespace PakViewer.Snooper.Models;

public class Section
{
    public readonly int MaterialIndex;
    public readonly int FacesCount;
    public readonly int FirstFaceIndex;
    public readonly IntPtr FirstFaceIndexPtr;
    public readonly Vector3 Color;

    public bool Show;

    public Section(int index, int facesCount, int firstFaceIndex)
    {
        MaterialIndex = Math.Max(0, index);
        FacesCount = facesCount;
        FirstFaceIndex = firstFaceIndex;
        FirstFaceIndexPtr = new IntPtr(FirstFaceIndex * sizeof(uint));
        Color = Constants.COLOR_PALETTE[MaterialIndex % Constants.PALETTE_LENGTH];
    }

    public void SetupMaterial(Material material)
    {
        material.IsUsed = true;
        Show = true;
    }
}
