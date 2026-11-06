using System;
using System.Numerics;
using CUE4Parse.UE4.Objects.Core.Math;
using PakViewer.Snooper;
using OpenTK.Windowing.GraphicsLibraryFramework;

namespace PakViewer.Snooper;

public class Camera
{
    public enum WorldMode
    {
        FlyCam,
        Arcball
    }

    public Vector3 Position;
    public Vector3 Direction;
    public WorldMode Mode;
    public Vector3 PositionArc => Position - Direction;
    public Vector3 DirectionArc => Direction - Position;
    public Vector3 Up => Vector3.UnitY;

    public float Zoom = 60f;
    public float Speed = 1f;
    public float Far = 100f;
    public float Near => 0.01f;
    public float AspectRatio = 16f / 9f;

    public Camera()
    {
        Position = new Vector3(0, 1, 1);
        Direction = Vector3.Zero;
        Mode = UserSettings.Default.CameraMode;
    }

    public void Setup(FBox box) => Teleport(FVector.ZeroVector, box, true);
    public void Teleport(Vector3 instancePos, FBox box, bool updateAll = false)
    {
        box.GetCenterAndExtents(out var center, out var extents);
        center += new FVector(instancePos.X, instancePos.Z, instancePos.Y);
        var distance = extents.AbsMax();

        Position = new Vector3(instancePos.X, center.Z, instancePos.Z + distance * 2);
        Direction = new Vector3(center.X, center.Z, center.Y);
        if (updateAll)
        {
            Far = Math.Max(Far, box.Max.AbsMax() * 50f);
            Speed = Math.Max(Speed, distance);
        }
    }

    public void Modify(Vector2 mouseDelta)
    {
        var lookSensitivity = Mode switch
        {
            WorldMode.FlyCam => 0.002f,
            WorldMode.Arcball => 0.003f,
            _ => throw new ArgumentOutOfRangeException()
        };
        mouseDelta *= lookSensitivity;

        const float tolerance = 0.001f;
        var rotationX = Matrix4x4.CreateFromAxisAngle(-Up, mouseDelta.X);
        switch (Mode)
        {
            case WorldMode.FlyCam:
            {
                Direction = Vector3.Transform(DirectionArc, rotationX) + Position;

                var right = Vector3.Normalize(Vector3.Cross(Up, DirectionArc));

                var currentPitch = MathF.Acos(Vector3.Dot(DirectionArc, Up) / (DirectionArc.Length() * Up.Length()));
                var newPitch = currentPitch + mouseDelta.Y;
                var clampedPitch = Math.Clamp(newPitch, tolerance, MathF.PI - tolerance);
                var pitchDelta = clampedPitch - currentPitch;

                var rotationY = Matrix4x4.CreateFromAxisAngle(right, pitchDelta);
                Direction = Vector3.Transform(DirectionArc, rotationY) + Position;
                break;
            }
            case WorldMode.Arcball:
            {
                Position = Vector3.Transform(PositionArc, rotationX) + Direction;

                var right = Vector3.Normalize(Vector3.Cross(-Up, PositionArc));

                var currentPitch = MathF.Acos(Vector3.Dot(PositionArc, -Up) / (PositionArc.Length() * Up.Length()));
                var newPitch = currentPitch + mouseDelta.Y;
                var clampedPitch = Math.Clamp(newPitch, tolerance, MathF.PI - tolerance);
                var pitchDelta = clampedPitch - currentPitch;

                var rotationY = Matrix4x4.CreateFromAxisAngle(right, pitchDelta);
                Position = Vector3.Transform(PositionArc, rotationY) + Direction;
                break;
            }
            default:
                throw new ArgumentOutOfRangeException();
        }
    }

    public void Modify(KeyboardState keyboard, float time)
    {
        if (!keyboard.IsAnyKeyDown) return;
        var multiplier = keyboard.IsKeyDown(Keys.LeftShift) ? 2f : 1f;
        var moveSpeed = Speed * multiplier * time;
        var moveAxis = Vector3.Normalize(-PositionArc);
        var panAxis = Vector3.Normalize(Vector3.Cross(moveAxis, Up));

        switch (Mode)
        {
            case WorldMode.FlyCam:
            {
                if (keyboard.IsKeyDown(Keys.W))
                {
                    var d = moveSpeed * moveAxis;
                    Position += d;
                    Direction += d;
                }
                if (keyboard.IsKeyDown(Keys.S))
                {
                    var d = moveSpeed * moveAxis;
                    Position -= d;
                    Direction -= d;
                }
                break;
            }
            case WorldMode.Arcball:
            {
                if (keyboard.IsKeyDown(Keys.W))
                    Position += moveSpeed * moveAxis;
                if (keyboard.IsKeyDown(Keys.S))
                    Position -= moveSpeed * moveAxis;
                break;
            }
            default:
                throw new ArgumentOutOfRangeException();
        }

        if (keyboard.IsKeyDown(Keys.A))
        {
            var d = panAxis * moveSpeed;
            Position -= d;
            Direction -= d;
        }
        if (keyboard.IsKeyDown(Keys.D))
        {
            var d = panAxis * moveSpeed;
            Position += d;
            Direction += d;
        }
        if (keyboard.IsKeyDown(Keys.E))
        {
            var d = moveSpeed * Up;
            Position += d;
            Direction += d;
        }
        if (keyboard.IsKeyDown(Keys.Q))
        {
            var d = moveSpeed * Up;
            Position -= d;
            Direction -= d;
        }

        if (keyboard.IsKeyDown(Keys.C))
            ModifyZoom(+.5f);
        if (keyboard.IsKeyDown(Keys.X))
            ModifyZoom(-.5f);
    }

    private void ModifyZoom(float zoomAmount)
    {

        Zoom = Math.Clamp(Zoom - zoomAmount, 1.0f, 89f);
    }

    public void SetPresetView(int presetIndex, FBox modelBox)
    {
        modelBox.GetCenterAndExtents(out var center, out var extents);
        var distance = extents.AbsMax() * 2.5f;
        var centerVec = new Vector3(center.X, center.Z, center.Y);

        switch (presetIndex)
        {
            case 1:
                Position = centerVec + new Vector3(0, 0, distance);
                Direction = centerVec;
                break;
            case 2:
                Position = centerVec + new Vector3(0, 0, -distance);
                Direction = centerVec;
                break;
            case 3:
                Position = centerVec + new Vector3(-distance, 0, 0);
                Direction = centerVec;
                break;
            case 4:
                Position = centerVec + new Vector3(distance, 0, 0);
                Direction = centerVec;
                break;
            case 5:
                Position = centerVec + new Vector3(0, distance, 0.01f);
                Direction = centerVec;
                break;
            case 6:
                Position = centerVec + new Vector3(0, -distance, -0.01f);
                Direction = centerVec;
                break;
        }
    }

    public Matrix4x4 GetViewMatrix() => Matrix4x4.CreateLookAt(Position, Direction, Up);
    public Matrix4x4 GetProjectionMatrix()
        => Matrix4x4.CreatePerspectiveFieldOfView(DegreesToRadians(Zoom), AspectRatio, Near, Far);

    private static float DegreesToRadians(float degrees) => degrees * (MathF.PI / 180.0f);
}
