using System;
using Microsoft.UI.Input;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Windows.System;
using Windows.UI.Core;

namespace PakViewer.Views;

public sealed partial class ImageViewerWindow : Window
{
    private readonly ContentDialog _helpDialog;

    public ImageViewerWindow()
    {
        this.InitializeComponent();
        Title = "Image Viewer";
        var appWindow = this.AppWindow;
        appWindow.Resize(new Windows.Graphics.SizeInt32(1200, 800));
        var iconPath = System.IO.Path.Combine(AppContext.BaseDirectory, "Icons", "PakViewer.ico");
        if (System.IO.File.Exists(iconPath))
            appWindow.SetIcon(iconPath);
        var presenter = appWindow.Presenter as OverlappedPresenter;
        if (presenter != null)
        {
            presenter.IsAlwaysOnTop = true;
        }
        _helpDialog = BuildHelpDialog();
    }

    public void SetImage(string title, string path, ImageSource? image, string? statusText)
    {
        TitleText.Text = title;
        PathText.Text = path;

        ImageDisplay.Source = image;
        StatusText.Text = statusText ?? string.Empty;
        StatusText.Visibility = string.IsNullOrWhiteSpace(StatusText.Text) ? Visibility.Collapsed : Visibility.Visible;
    }

    private void Close_Click(object sender, RoutedEventArgs e)
    {
        Cleanup();
        Close();
    }

    private void Cleanup()
    {

        if (ImageDisplay != null)
        {
            ImageDisplay.Source = null;
        }
    }

    private void ImageScrollViewer_PointerWheelChanged(object sender, PointerRoutedEventArgs e)
    {
        var keyboardState = InputKeyboardSource.GetKeyStateForCurrentThread(VirtualKey.Control);
        if (!keyboardState.HasFlag(CoreVirtualKeyStates.Down))
        {
            return;
        }

        if (sender is not ScrollViewer scrollViewer)
        {
            return;
        }

        var delta = e.GetCurrentPoint(scrollViewer).Properties.MouseWheelDelta;
        var zoomStep = delta > 0 ? 0.1 : -0.1;
        var targetZoom = Math.Clamp(scrollViewer.ZoomFactor + zoomStep, scrollViewer.MinZoomFactor, scrollViewer.MaxZoomFactor);
        scrollViewer.ChangeView(null, null, (float)targetZoom);
        e.Handled = true;
    }

    private async void Help_Click(object sender, RoutedEventArgs e)
    {
        _helpDialog.XamlRoot = Content.XamlRoot;
        await _helpDialog.ShowAsync();
    }

    private ContentDialog BuildHelpDialog()
    {
        var dialog = new ContentDialog
        {
            Title = "Viewer Controls",
            PrimaryButtonText = "Got it"
        };

        var panel = new StackPanel
        {
            Spacing = 8
        };

        panel.Children.Add(new TextBlock
        {
            Text = "Ctrl + mouse wheel: Zoom in/out",
            TextWrapping = TextWrapping.Wrap
        });

        dialog.Content = panel;
        return dialog;
    }
}
