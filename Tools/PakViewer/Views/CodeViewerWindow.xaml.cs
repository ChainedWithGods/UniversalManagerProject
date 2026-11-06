using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using PakViewer.Settings;

namespace PakViewer.Views;

public sealed partial class CodeViewerWindow : Window
{
    public CodeViewerWindow()
    {
        InitializeComponent();
        Title = "Code Viewer";
        App.ApplyThemeToWindow(this, AppSettings.Instance.Theme);

        var appWindow = AppWindow;
        appWindow.Resize(new Windows.Graphics.SizeInt32(1280, 860));

        if (appWindow.Presenter is OverlappedPresenter presenter)
        {
            presenter.IsResizable = true;
            presenter.IsMaximizable = true;
            presenter.IsMinimizable = true;
        }

        CodePreview.ShowOpenInWindowButton = false;
    }

    public async Task LoadContentAsync(string text, string extension, string fileName, string filePath, long fileSize, string? encoding = null, bool wordWrap = false)
    {
        Title = string.IsNullOrWhiteSpace(fileName) ? "Code Viewer" : $"{fileName} - Code Viewer";
        CodePreview.SetWordWrap(wordWrap);
        await CodePreview.LoadContentAsync(text, extension, fileName, filePath, fileSize, encoding);
    }
}
