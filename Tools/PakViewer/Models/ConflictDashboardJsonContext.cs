using System.Text.Json.Serialization;

namespace PakViewer.Models;

[JsonSourceGenerationOptions(PropertyNameCaseInsensitive = true)]
[JsonSerializable(typeof(ConflictDashboardData))]
internal partial class ConflictDashboardJsonContext : JsonSerializerContext
{
}
