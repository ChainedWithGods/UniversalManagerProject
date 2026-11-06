using System;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using PakViewer.Services;
using PakViewer.Settings;
using Serilog;
using Windows.System;

namespace PakViewer.Views;

public sealed partial class UpdatePrompt : ContentDialog
{
    private static bool _isDialogOpen = false;
    private readonly UpdateCheckResult _updateResult;
    private CancellationTokenSource? _downloadCts;
    private Uri? _fallbackUri;

    public UpdatePrompt(UpdateCheckResult updateResult)
    {
        this.InitializeComponent();
        _updateResult = updateResult;

        Title = $"Update Available (v{_updateResult.LatestVersion})";
        CurrentVersionTextBlock.Text = $"v{_updateResult.CurrentVersion}";
        LatestVersionTextBlock.Text = $"v{_updateResult.LatestVersion}";

        ChangelogTextBlock.Text = string.IsNullOrWhiteSpace(_updateResult.Manifest?.Changelog)
            ? "No release notes provided."
            : _updateResult.Manifest.Changelog;
    }

    public new static async Task ShowAsync(XamlRoot xamlRoot, UpdateCheckResult updateResult)
    {
        if (xamlRoot == null || updateResult.Manifest == null || _isDialogOpen)
            return;

        _isDialogOpen = true;

        try
        {
            var dialog = new UpdatePrompt(updateResult)
            {
                XamlRoot = xamlRoot
            };

            await ((ContentDialog)dialog).ShowAsync();
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to display update dialog");
        }
        finally
        {
            _isDialogOpen = false;
        }
    }

    private async void ContentDialog_PrimaryButtonClick(ContentDialog sender, ContentDialogButtonClickEventArgs args)
    {
        var deferral = args.GetDeferral();
        args.Cancel = true;

        IsPrimaryButtonEnabled = false;
        IsSecondaryButtonEnabled = false;
        PrimaryButtonText = string.Empty;
        SecondaryButtonText = string.Empty;
        CloseButtonText = string.Empty;

        ProgressBorder.Visibility = Visibility.Visible;
        ErrorBorder.Visibility = Visibility.Collapsed;
        ProgressStatusTextBlock.Text = "Connecting to server...";

        _downloadCts = new CancellationTokenSource();

        try
        {
            var updateService = new UpdateService();
            var progress = new Progress<UpdateDownloadProgress>(p =>
            {
                if (p.TotalBytes.HasValue && p.TotalBytes.Value > 0)
                {
                    DownloadProgressBar.IsIndeterminate = false;
                    DownloadProgressBar.Value = p.ProgressPercentage;
                    var mbRead = (p.BytesDownloaded / 1024.0 / 1024.0).ToString("F1");
                    var mbTotal = (p.TotalBytes.Value / 1024.0 / 1024.0).ToString("F1");
                    ProgressPercentTextBlock.Text = $"{p.ProgressPercentage:F0}%";
                    ProgressDetailTextBlock.Text = $"{mbRead} MB / {mbTotal} MB";
                    ProgressStatusTextBlock.Text = p.StatusMessage;
                }
                else
                {
                    DownloadProgressBar.IsIndeterminate = true;
                    var mbRead = (p.BytesDownloaded / 1024.0 / 1024.0).ToString("F1");
                    ProgressPercentTextBlock.Text = string.Empty;
                    ProgressDetailTextBlock.Text = $"{mbRead} MB downloaded";
                    ProgressStatusTextBlock.Text = p.StatusMessage;
                }
            });

            var zipPath = await updateService.DownloadUpdateAsync(
                _updateResult.Manifest!.DownloadUrl,
                progress,
                _downloadCts.Token);

            ProgressStatusTextBlock.Text = "Extracting update and restarting PakViewer...";
            ProgressPercentTextBlock.Text = "100%";
            await Task.Delay(600);

            UpdateService.ApplyUpdateAndRestart(zipPath, _updateResult.Manifest.Version);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Update download or extraction failed");
            ProgressBorder.Visibility = Visibility.Collapsed;
            ErrorBorder.Visibility = Visibility.Visible;
            ErrorMessageTextBlock.Text = $"Download failed: {ex.Message}";

            IsSecondaryButtonEnabled = true;
            SecondaryButtonText = "Close";

            try
            {
                var directUrl = UpdateService.NormalizeDownloadUrl(_updateResult.Manifest!.DownloadUrl, isJsonManifest: false);
                if (Uri.TryCreate(directUrl, UriKind.Absolute, out var uri))
                {
                    _fallbackUri = uri;
                    BrowserFallbackButton.Visibility = Visibility.Visible;
                }
            }
            catch { }
        }
        finally
        {
            deferral.Complete();
        }
    }

    private void ContentDialog_CloseButtonClick(ContentDialog sender, ContentDialogButtonClickEventArgs args)
    {
        if (!string.IsNullOrWhiteSpace(_updateResult.Manifest?.Version))
        {
            AppSettings.Instance.IgnoredUpdateVersion = _updateResult.Manifest.Version;
            AppSettings.Save();
            Log.Information("User chose to skip update version: {Version}", _updateResult.Manifest.Version);
        }
    }

    private async void BrowserFallbackButton_Click(object sender, RoutedEventArgs e)
    {
        if (_fallbackUri != null)
        {
            try
            {
                await Launcher.LaunchUriAsync(_fallbackUri);
            }
            catch (Exception ex)
            {
                Log.Error(ex, "Failed to launch browser fallback URL");
            }
        }
    }
}
