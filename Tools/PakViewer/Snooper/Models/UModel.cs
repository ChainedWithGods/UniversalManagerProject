using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Numerics;
using CUE4Parse_Conversion;
using CUE4Parse_Conversion.Dto;
using CUE4Parse.UE4.Assets;
using CUE4Parse.UE4.Assets.Exports;
using CUE4Parse.UE4.Assets.Exports.Material;
using CUE4Parse.UE4.Objects.Core.Math;
using CUE4Parse.Utils;
using PakViewer.Snooper;
using PakViewer.Snooper.Buffers;
using PakViewer.Snooper.Shading;
using OpenTK.Graphics.OpenGL4;

namespace PakViewer.Snooper.Models;

public class VertexAttribute
{
    public int Size;
    public VertexAttribPointerType Type;
    public bool Enabled;
}

public abstract class UModel : IRenderableModel, IDisposable
{
    protected const int LodLevel = 0;

    protected readonly List<VertexAttribute> _vertexAttributes =
    [
        new VertexAttribute { Size = 1, Type = VertexAttribPointerType.Int, Enabled = false },
        new VertexAttribute { Size = 3, Type = VertexAttribPointerType.Float, Enabled = true },
        new VertexAttribute { Size = 3, Type = VertexAttribPointerType.Float, Enabled = false },
        new VertexAttribute { Size = 3, Type = VertexAttribPointerType.Float, Enabled = false },
        new VertexAttribute { Size = 2, Type = VertexAttribPointerType.Float, Enabled = false },
        new VertexAttribute { Size = 1, Type = VertexAttribPointerType.Float, Enabled = false },
        new VertexAttribute { Size = 1, Type = VertexAttribPointerType.Float, Enabled = false },
        new VertexAttribute { Size = 4, Type = VertexAttribPointerType.Float, Enabled = false },
        new VertexAttribute { Size = 4, Type = VertexAttribPointerType.Float, Enabled = false }
    ];

    private readonly UObject _export;
    public int Handle { get; set; }
    public BufferObject<uint> Ebo { get; set; }
    public BufferObject<float> Vbo { get; set; }
    public BufferObject<Matrix4x4> MatrixVbo { get; set; }
    public VertexArrayObject<float, uint> Vao { get; set; }

    public string Path { get; }
    public string Name { get; }
    public string Type { get; protected set; }
    public int UvCount { get; protected set; }
    public uint[] Indices { get; set; }
    public float[] Vertices { get; set; }
    public Section[] Sections { get; set; }
    public List<Transform> Transforms { get; }
    public Attachment Attachments { get; }

    public FBox Box;
    public readonly List<Socket> Sockets;
    public readonly List<Collision> Collisions;
    public Material[] Materials;
    public bool IsTwoSided;
    public bool IsProp;

    public int VertexSize => _vertexAttributes.Where(x => x.Enabled).Sum(x => x.Size);
    public bool HasVertexColors => _vertexAttributes[(int) EAttribute.Colors].Enabled;
    public bool HasSockets => Sockets.Count > 0;
    public bool HasCollisions => Collisions.Count > 0;
    public int TransformsCount => Transforms.Count;

    public bool IsSetup { get; set; }
    public bool IsVisible { get; set; }
    public bool IsSelected { get; set; }
    public bool ShowWireframe { get; set; }
    public bool ShowCollisions { get; set; }
    public int SelectedInstance;

    protected UModel()
    {
        _export = null;
        UvCount = 1;

        Box = new FBox(new FVector(-2f), new FVector(2f));
        Sockets = new List<Socket>();
        Collisions = new List<Collision>();
        Transforms = new List<Transform>();
    }

    protected UModel(UObject export)
    {
        _export = export;
        Path = _export.GetPathName();
        Name = Path.SubstringAfterLast('/').SubstringBefore('.');
        Type = export.ExportType;
        UvCount = 1;

        Box = new FBox(new FVector(-2f), new FVector(2f));
        Sockets = new List<Socket>();
        Collisions = new List<Collision>();
        Transforms = new List<Transform>();
        Attachments = new Attachment(Name);

        _vertexAttributes[(int) EAttribute.Index].Enabled =
            _vertexAttributes[(int) EAttribute.Normals].Enabled =
                _vertexAttributes[(int) EAttribute.Tangent].Enabled =
                    _vertexAttributes[(int) EAttribute.UVs].Enabled =
                        _vertexAttributes[(int) EAttribute.Layer].Enabled = true;
    }

    public virtual void Setup(Options options)
    {
        Handle = GL.CreateProgram();
        Ebo = new BufferObject<uint>(Indices, BufferTarget.ElementArrayBuffer);
        Vbo = new BufferObject<float>(Vertices, BufferTarget.ArrayBuffer);
        Vao = new VertexArrayObject<float, uint>(Vbo, Ebo);

        var offset = 0;
        var broken = GL.GetInteger(GetPName.MaxTextureCoords) == 0;
        for (var i = 0; i < _vertexAttributes.Count; i++)
        {
            var attribute = _vertexAttributes[i];
            if (!attribute.Enabled) continue;

            if (i != 5 || !broken)
            {
                Vao.VertexAttributePointer((uint) i, attribute.Size, attribute.Type, VertexSize, offset);
            }

            offset += attribute.Size;
        }

        SetupInstances();

        for (var i = 0; i < Materials.Length; i++)
        {
            if (!Materials[i].IsUsed) continue;
            Materials[i].Setup(options, broken ? 1 : UvCount);
        }

        foreach (var collision in Collisions)
        {
            collision.Setup();
        }

        if (options.Models.Count == 1 && Sections.All(x => !x.Show))
        {
            IsVisible = true;
            foreach (var section in Sections)
            {
                section.Show = true;
            }
        }
        else if (!IsVisible)
        {
            foreach (var section in Sections)
            {
                if (section.Show)
                {
                    IsVisible = true;
                    break;
                }
            }
        }
        else foreach (var section in Sections)
        {
            section.Show = true;
        }

        IsSetup = true;
    }

    public virtual void Render(Shader shader, Texture checker = null, bool outline = false)
    {
        if (outline) GL.Disable(EnableCap.DepthTest);

        if (IsTwoSided || UserSettings.Default.DisableBackfaceCulling) GL.Disable(EnableCap.CullFace);
        if (IsSelected)
        {
            GL.Enable(EnableCap.StencilTest);
            GL.StencilFunc(outline ? StencilFunction.Notequal : StencilFunction.Always, 1, 0xFF);
        }

        if (this is SkeletalModel skeletalModel) skeletalModel.Render(shader);
        else if (this is SplineModel splineModel) splineModel.Render(shader);
        else
        {
            shader.SetUniform("uIsAnimated", false);
            shader.SetUniform("uIsSpline", false);
        }

        if (!outline)
        {
            shader.SetUniform("uUvCount", UvCount);
            shader.SetUniform("uOpacity", ShowCollisions && IsSelected ? 0.75f : 1f);
            shader.SetUniform("uHasVertexColors", HasVertexColors);
        }

        Vao.Bind();
        GL.PolygonMode(MaterialFace.FrontAndBack, ShowWireframe ? PolygonMode.Line : PolygonMode.Fill);
        foreach (var section in Sections)
        {
            if (!section.Show) continue;

            var usePolygonOffset = false;
            if (!outline && checker == null)
            {
                var blendMode = Materials[section.MaterialIndex].Parameters.BlendMode;
                usePolygonOffset = blendMode != CUE4Parse.UE4.Assets.Exports.Material.EBlendMode.BLEND_Opaque;
            }

            if (usePolygonOffset)
            {
                GL.Enable(EnableCap.PolygonOffsetFill);
                GL.PolygonOffset(-1.0f, -1.0f);
            }

            if (!outline)
            {
                if (checker != null)
                {
                    shader.SetUniform("uParameters.Diffuse[0].Sampler", 0);
                    checker.Bind(TextureUnit.Texture0);
                }
                else
                {
                    shader.SetUniform("uSectionColor", section.Color);
                    Materials[section.MaterialIndex].Render(shader);
                }
            }

            GL.DrawElementsInstanced(PrimitiveType.Triangles, section.FacesCount, DrawElementsType.UnsignedInt, section.FirstFaceIndexPtr, TransformsCount);

            if (usePolygonOffset)
            {
                GL.Disable(EnableCap.PolygonOffsetFill);
            }
        }

        GL.PolygonMode(MaterialFace.FrontAndBack, PolygonMode.Fill);
        Vao.Unbind();

        if (IsSelected)
        {
            GL.StencilFunc(StencilFunction.Always, 0, 0xFF);
            GL.Disable(EnableCap.StencilTest);
        }

        if (IsTwoSided || UserSettings.Default.DisableBackfaceCulling)
            GL.Enable(EnableCap.CullFace);

        if (outline)
            GL.Enable(EnableCap.DepthTest);
    }

    public void PickingRender(Shader shader)
    {
        Vao.Bind();
        GL.PolygonMode(MaterialFace.FrontAndBack, PolygonMode.Fill);
        for (var s = 0; s < Sections.Length; s++)
        {
            if (!Sections[s].Show) continue;

            Ebo.Bind();
            GL.DrawElements(PrimitiveType.Triangles, Sections[s].FacesCount, DrawElementsType.UnsignedInt, Sections[s].FirstFaceIndexPtr);
        }
    }

    public virtual void RenderCollision(Shader shader)
    {
        foreach (var collision in Collisions)
        {
            collision?.Render(shader, Matrix4x4.Identity);
        }
    }

    public virtual void Update(Options options)
    {
        for (var i = 0; i < Materials.Length; i++)
        {
            if (Materials[i].IsUsed) Materials[i].Update(options);
        }
    }

    public void AddInstance(Transform transform)
    {
        SelectedInstance = TransformsCount;
        Transforms.Add(transform);
    }

    public void SetupInstances()
    {
        MatrixVbo = new BufferObject<Matrix4x4>(TransformsCount, BufferTarget.ArrayBuffer);
        for (int instance = 0; instance < TransformsCount; instance++)
        {
            Transforms[instance].Save();
            MatrixVbo.Update(instance, Transforms[instance].Matrix);
        }
        Vao.BindInstancing();
    }

    public Transform GetTransform() => Transforms[SelectedInstance];
    public Matrix4x4 GetSocketTransform(int index)
    {
        var socket = Sockets[index];
        var worldMatrix = GetTransform().Matrix;
        var boneMatrix = Matrix4x4.Identity;
        if (this is SkeletalModel skeletalModel && skeletalModel.Skeleton.BonesByLoweredName.TryGetValue(socket.BoneName.Text.ToLower(), out var bone))
            boneMatrix = skeletalModel.Skeleton.GetBoneMatrix(bone);

        var socketRelation = boneMatrix * worldMatrix;
        return socket.Transform.LocalMatrix * socketRelation;
    }

    public bool Save(out string label, out string savedFilePath)
    {

        label = string.Empty;
        savedFilePath = string.Empty;
        return false;
    }

    public virtual void Dispose()
    {
        Ebo?.Dispose();
        Vbo?.Dispose();
        MatrixVbo?.Dispose();
        Vao?.Dispose();

        foreach (var socket in Sockets)
        {
            socket?.Dispose();
        }
        Sockets.Clear();

        foreach (var collision in Collisions)
        {
            collision?.Dispose();
        }
        Collisions.Clear();

        if (Materials != null)
        {
            foreach (var material in Materials)
            {
                material?.Dispose();
            }
            Array.Clear(Materials, 0, Materials.Length);
            Materials = null;
        }

        Vertices = null;
        Indices = null;
        Sections = null;

        GL.DeleteProgram(Handle);
    }
}

public abstract class UModel<TVert> : UModel where TVert : struct, IMeshVertex
{
    private static readonly bool _usesBones = typeof(TVert) == typeof(SkinnedMeshVertex);

    protected UModel() : base() { }

    protected UModel(UObject export) : base(export) { }

    protected UModel(UObject export, MeshLodDto<TVert> lod, IReadOnlyList<ResolvedObject?> materials, int numLods, Transform transform = null) : this(export)
    {
        var hasCustomUvs = lod.ExtraUvs is { Length: > 0 };
        var numTexCoords = hasCustomUvs ? lod.ExtraUvs!.Length + 1 : 1;
        UvCount = hasCustomUvs ? Math.Max(numTexCoords, numLods) : numTexCoords;
        IsTwoSided = lod.IsTwoSided;

        Indices = lod.Indices.ToArray();

        Materials = new Material[materials.Count];
        for (var m = 0; m < Materials.Length; m++)
        {
            if ((materials[m]?.TryLoad(out var material) ?? false) && material is UMaterialInterface unrealMaterial)
                Materials[m] = new Material(unrealMaterial); else Materials[m] = new Material();
        }

        _vertexAttributes[(int) EAttribute.Colors].Enabled = lod.VertexColors is { Length: > 0 };
        _vertexAttributes[(int) EAttribute.BonesId].Enabled =
            _vertexAttributes[(int) EAttribute.BonesWeight].Enabled = _usesBones;

        var skinnedVertices = _usesBones ? lod.Vertices as SkinnedMeshVertex[] : null;
        var vertexColors = lod.VertexColors;

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
            Vertices[baseIndex + count++] = hasCustomUvs ? lod.ExtraUvs![0][i].U - 1 : .5f;

            if (HasVertexColors && vertexColors is { Length: > 0 })
            {
                Vertices[baseIndex + count++] = vertexColors[0].Colors[i].ToPackedARGB();
            }

            if (skinnedVertices != null)
            {
                var influences = skinnedVertices[i].Influences;
                var max = influences.Length;
                for (var j = 0; j < 8; j++)
                {
                    var boneID = j < max ? influences[j].Bone : (ushort) 0;
                    var weight = j < max ? influences[j].RawWeight : (ushort) 0;

                    Vertices[baseIndex + count++] = (boneID << 16) | weight;
                }
            }
        }

        Sections = new Section[lod.Sections.Length];
        for (var s = 0; s < Sections.Length; s++)
        {
            var section = lod.Sections[s];
            Sections[s] = new Section(section.MaterialIndex, section.NumFaces * 3, section.FirstIndex);
            if (section.IsValid && Materials.Length > 0 && section.MaterialIndex < Materials.Length)
                Sections[s].SetupMaterial(Materials[section.MaterialIndex]);
        }

        AddInstance(transform ?? Transform.Identity);
    }
}
