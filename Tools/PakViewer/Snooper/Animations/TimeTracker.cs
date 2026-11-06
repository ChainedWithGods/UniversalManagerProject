using System;

namespace PakViewer.Snooper.Animations;

public class TimeTracker : IDisposable
{
    public bool IsPaused;
    public bool IsActive;
    public float ElapsedTime;
    public float MaxElapsedTime;
    public float TimeMultiplier;

    public TimeTracker()
    {
        Reset();
    }

    public void Update(float deltaSeconds)
    {
        ElapsedTime += deltaSeconds * TimeMultiplier;
        if (ElapsedTime >= MaxElapsedTime) Reset(false);
    }

    public void SafeSetElapsedTime(float elapsedTime)
    {
        ElapsedTime = Math.Clamp(elapsedTime, 0.0f, MaxElapsedTime);
    }

    public void SafeSetMaxElapsedTime(float maxElapsedTime)
    {
        MaxElapsedTime = MathF.Max(maxElapsedTime, MaxElapsedTime);
    }

    public void Reset(bool doMet = true)
    {
        IsPaused = false;
        ElapsedTime = 0.0f;
        if (doMet)
        {
            MaxElapsedTime = 0.01f;
            TimeMultiplier = 1f;
        }
    }

    public void Dispose()
    {
        Reset();
    }
}
