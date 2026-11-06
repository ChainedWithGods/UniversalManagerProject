using CUE4Parse_Conversion.Writers.ActorX.Structs.Animations;
using CUE4Parse.Utils;

namespace PakViewer.Snooper.Animations;

public class Sequence
{
    public readonly string Name;
    public readonly float TimePerFrame;
    public readonly float RateScale;
    public readonly float StartTime;
    public readonly float Duration;
    public readonly float EndTime;
    public readonly int EndFrame;
    public readonly int LoopingCount;
    public readonly bool IsAdditive;

    public Sequence(CAnimSequence sequence)
    {
        Name = sequence.Name;
        TimePerFrame = 1.0f / sequence.FramesPerSecond;
        RateScale = sequence.OriginalSequence?.RateScale ?? 1.0f;
        StartTime = sequence.StartPos;
        Duration = sequence.AnimEndTime;
        EndTime = StartTime + Duration;
        EndFrame = (Duration / TimePerFrame).FloorToInt() - 1;
        LoopingCount = sequence.LoopingCount;
        IsAdditive = sequence.IsAdditive;
    }
}
