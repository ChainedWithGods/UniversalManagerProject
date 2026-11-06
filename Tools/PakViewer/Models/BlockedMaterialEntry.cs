namespace PakViewer.Models;

public class BlockedMaterialEntry
{
    public string Name { get; set; } = "";
    public bool IsShown { get; set; }
    public bool IsAutoDetected { get; set; }
    public string Reason { get; set; } = "";
}
