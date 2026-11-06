using OpenTK.Windowing.GraphicsLibraryFramework;

namespace PakViewer.Snooper;

public interface IRendererWindow
{
    KeyboardState KeyboardState { get; }
    void WindowShouldClose(bool close, bool viaEscape);
}
