using System;
using System.Collections.Generic;
using System.Linq;
using System.Numerics;
using CUE4Parse_Conversion.Dto;
using CUE4Parse.UE4.Assets;
using CUE4Parse.UE4.Assets.Exports.Material;
using CUE4Parse.UE4.Assets.Exports.Paper2D;
using CUE4Parse.UE4.Assets.Exports.StaticMesh;
using CUE4Parse.UE4.Assets.Exports.Texture;
using CUE4Parse.UE4.Objects.Core.Math;
using CUE4Parse.UE4.Objects.PhysicsEngine;
using PakViewer.Snooper.Shading;
using OpenTK.Graphics.OpenGL4;

namespace PakViewer.Snooper.Models;

public class StaticModel : UModel<MeshVertex>
{
    public StaticModel(UMaterialInterface unrealMaterial, StaticMeshDto staticMesh) : base(unrealMaterial)
    {
        var lod = staticMesh.LODs[LodLevel];

        Indices = lod.Indices.ToArray();

        Vertices = new float[lod.Vertices.Length * VertexSize];
        for (var i = 0; i < lod.Vertices.Length; i++)
        {
            var count = 0;
            var baseIndex = i * VertexSize;
            var vert = lod.Vertices[i];
            Vertices[baseIndex + count++] = i;
            Vertices[baseIndex + count++] = vert.Position.X * Constants.SCALE_DOWN_RATIO;
            Vertices[baseIndex + count++] = vert.Position.Z * Constants.SCALE_DOWN_RATIO;
            Vertices[baseIndex + count++] = vert.Position.Y * Constants.SCALE_DOWN_RATIO;
            Vertices[baseIndex + count++] = vert.Normal.X;
            Vertices[baseIndex + count++] = vert.Normal.Z;
            Vertices[baseIndex + count++] = vert.Normal.Y;
            Vertices[baseIndex + count++] = vert.Tangent.X;
            Vertices[baseIndex + count++] = vert.Tangent.Z;
            Vertices[baseIndex + count++] = vert.Tangent.Y;
            Vertices[baseIndex + count++] = vert.Uv.U;
            Vertices[baseIndex + count++] = vert.Uv.V;
            Vertices[baseIndex + count++] = .5f;
        }

        Materials = new Material[1];
        Materials[0] = new Material(unrealMaterial) { IsUsed = true };

        Sections = new Section[1];
        Sections[0] = new Section(0, Indices.Length, 0);

        AddInstance(Transform.Identity);

        Box = staticMesh.Bounds * 1.5f * Constants.SCALE_DOWN_RATIO;
    }

    public StaticModel(UPaperSprite paperSprite, UTexture2D texture) : base(paperSprite)
    {
        Indices = new uint[paperSprite.BakedRenderData.Length];
        for (var i = 0; i < Indices.Length; i++)
        {
            Indices[i] = (uint) i;
        }

        Vertices = new float[paperSprite.BakedRenderData.Length * VertexSize];
        for (var i = 0; i < paperSprite.BakedRenderData.Length; i++)
        {
            var count = 0;
            var baseIndex = i * VertexSize;
            var vert = paperSprite.BakedRenderData[i];
            var u = vert.Z;
            var v = vert.W;

            Vertices[baseIndex + count++] = i;
            Vertices[baseIndex + count++] = vert.X * paperSprite.PixelsPerUnrealUnit * Constants.SCALE_DOWN_RATIO;
            Vertices[baseIndex + count++] = vert.Y * paperSprite.PixelsPerUnrealUnit * Constants.SCALE_DOWN_RATIO;
            Vertices[baseIndex + count++] = 0;
            Vertices[baseIndex + count++] = 0;
            Vertices[baseIndex + count++] = 0;
            Vertices[baseIndex + count++] = 0;
            Vertices[baseIndex + count++] = 0;
            Vertices[baseIndex + count++] = 0;
            Vertices[baseIndex + count++] = 0;
            Vertices[baseIndex + count++] = u;
            Vertices[baseIndex + count++] = v;
            Vertices[baseIndex + count++] = .5f;
        }

        Materials = new Material[1];
        if (paperSprite.DefaultMaterial?.TryLoad(out UMaterialInstance unrealMaterial) ?? false)
        {
            Materials[0] = new Material(unrealMaterial);
        }
        else
        {
            Materials[0] = new Material();
        }
        Materials[0].Parameters.Textures[CMaterialParams2.FallbackDiffuse] = new ResolvedLoadedObject(texture);
        Materials[0].IsUsed = true;

        Sections = new Section[1];
        Sections[0] = new Section(0, Indices.Length, 0);

        AddInstance(Transform.Identity);

        var backward = new FVector(0, Math.Max(paperSprite.BakedSourceDimension.X, paperSprite.BakedSourceDimension.Y) / 2, 0);
        Box = new FBox(-backward, backward) * Constants.SCALE_DOWN_RATIO;
    }

    public StaticModel(UStaticMesh export, StaticMeshDto staticMesh, Transform transform = null)
        : base(export, staticMesh.LODs[LodLevel], (IReadOnlyList<ResolvedObject?>?)(export.Materials?.Select(m => m?.ResolvedObject).ToList()) ?? new List<ResolvedObject?>(), staticMesh.LODs.Count, transform)
    {
        if (export.BodySetup.TryLoad(out UBodySetup bodySetup) && bodySetup.AggGeom != null)
        {
            foreach (var convexElem in bodySetup.AggGeom.ConvexElems)
            {
                Collisions.Add(new Collision(convexElem));
            }
            foreach (var sphereElem in bodySetup.AggGeom.SphereElems)
            {
                Collisions.Add(new Collision(sphereElem));
            }
            foreach (var boxElem in bodySetup.AggGeom.BoxElems)
            {
                Collisions.Add(new Collision(boxElem));
            }
            foreach (var sphylElem in bodySetup.AggGeom.SphylElems)
            {
                Collisions.Add(new Collision(sphylElem));
            }
            foreach (var taperedCapsuleElem in bodySetup.AggGeom.TaperedCapsuleElems)
            {
                Collisions.Add(new Collision(taperedCapsuleElem));
            }
        }

        Box = staticMesh.Bounds * Constants.SCALE_DOWN_RATIO;
        for (var i = 0; i < export.Sockets.Length; i++)
        {
            if (export.Sockets[i].Load<UStaticMeshSocket>() is not { } socket) continue;
            Sockets.Add(new Socket(socket));
        }
    }

    public StaticModel(UStaticMesh export, MeshLodDto<MeshVertex> manualLod, Transform transform = null)
        : base(export, manualLod, (IReadOnlyList<ResolvedObject?>?)(export.Materials?.Select(m => m?.ResolvedObject).ToList()) ?? new List<ResolvedObject?>(), 1, transform)
    {
        Box = ComputeBoxFromVertices(manualLod);
    }

    public StaticModel(UMaterialInterface unrealMaterial, MeshLodDto<MeshVertex> manualLod, Transform transform = null)
        : base(unrealMaterial, manualLod, new List<ResolvedObject?> { null }, 1, transform)
    {
        var lod = manualLod;

        Indices = lod.Indices.ToArray();

        Vertices = new float[lod.Vertices.Length * VertexSize];
        for (var i = 0; i < lod.Vertices.Length; i++)
        {
            var count = 0;
            var baseIndex = i * VertexSize;
            var vert = lod.Vertices[i];
            Vertices[baseIndex + count++] = i;
            Vertices[baseIndex + count++] = vert.Position.X * Constants.SCALE_DOWN_RATIO;
            Vertices[baseIndex + count++] = vert.Position.Z * Constants.SCALE_DOWN_RATIO;
            Vertices[baseIndex + count++] = vert.Position.Y * Constants.SCALE_DOWN_RATIO;
            Vertices[baseIndex + count++] = vert.Normal.X;
            Vertices[baseIndex + count++] = vert.Normal.Z;
            Vertices[baseIndex + count++] = vert.Normal.Y;
            Vertices[baseIndex + count++] = vert.Tangent.X;
            Vertices[baseIndex + count++] = vert.Tangent.Z;
            Vertices[baseIndex + count++] = vert.Tangent.Y;
            Vertices[baseIndex + count++] = vert.Uv.U;
            Vertices[baseIndex + count++] = vert.Uv.V;
            Vertices[baseIndex + count++] = .5f;
        }

        Materials = new Material[1];
        Materials[0] = new Material(unrealMaterial) { IsUsed = true };

        Sections = new Section[1];
        Sections[0] = new Section(0, Indices.Length, 0);

        AddInstance(Transform.Identity);

        Box = ComputeBoxFromVertices(manualLod);
    }

    private static FBox ComputeBoxFromVertices(MeshLodDto<MeshVertex> lod)
    {
        if (lod.Vertices.Length == 0)
        {
            return new FBox(FVector.ZeroVector, FVector.ZeroVector);
        }

        var first = lod.Vertices[0].Position;
        float minX = first.X, minY = first.Y, minZ = first.Z;
        float maxX = first.X, maxY = first.Y, maxZ = first.Z;
        for (var i = 1; i < lod.Vertices.Length; i++)
        {
            var p = lod.Vertices[i].Position;
            if (p.X < minX) minX = p.X; else if (p.X > maxX) maxX = p.X;
            if (p.Y < minY) minY = p.Y; else if (p.Y > maxY) maxY = p.Y;
            if (p.Z < minZ) minZ = p.Z; else if (p.Z > maxZ) maxZ = p.Z;
        }
        return new FBox(new FVector(minX, minY, minZ), new FVector(maxX, maxY, maxZ)) * Constants.SCALE_DOWN_RATIO;
    }

    public override void RenderCollision(Shader shader)
    {
        base.RenderCollision(shader);

        GL.Disable(EnableCap.CullFace);
        GL.PolygonMode(MaterialFace.FrontAndBack, PolygonMode.Line);
        foreach (var collision in Collisions)
        {
            collision.Render(shader, Matrix4x4.Identity);
        }
        GL.PolygonMode(MaterialFace.FrontAndBack, PolygonMode.Fill);
        GL.Enable(EnableCap.CullFace);
    }
}
