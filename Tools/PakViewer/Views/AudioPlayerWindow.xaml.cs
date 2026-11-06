using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Dispatching;
using Windows.Storage.Pickers;
using Windows.Storage.Streams;
using Windows.Media.Core;
using Windows.Media.Playback;
using PakViewer.Services;

namespace PakViewer.Views;

public sealed partial class AudioPlayerWindow : Window
{
    private MediaPlayer? _mediaPlayer;
    private List<AudioTrackInfo>? _tracks;
    private byte[]? _singleTrackData;
    private string _singleTrackFormat = "wav";
    private bool _isClosing;
    private bool _isDraggingSlider;
    private DispatcherQueueTimer? _positionTimer;

    public AudioPlayerWindow()
    {
        InitializeComponent();
        Title = "Audio Player";
        var appWindow = AppWindow;
        appWindow.Resize(new Windows.Graphics.SizeInt32(700, 500));
        var presenter = appWindow.Presenter as OverlappedPresenter;
        if (presenter != null)
        {
            presenter.IsAlwaysOnTop = true;
        }

        _mediaPlayer = new MediaPlayer();
        MediaPlayerElement.SetMediaPlayer(_mediaPlayer);
        _mediaPlayer.MediaEnded += OnMediaEnded;
        _mediaPlayer.MediaFailed += OnMediaFailed;
        _mediaPlayer.PlaybackSession.PlaybackStateChanged += OnPlaybackStateChanged;

        _positionTimer = DispatcherQueue.CreateTimer();
        _positionTimer.Interval = TimeSpan.FromMilliseconds(250);
        _positionTimer.Tick += (_, _) => UpdatePositionUI();

        Closed += (_, _) => DisposePlayer();
    }

    public void SetAudio(string title, string path, byte[] audioData, string format)
    {
        TitleText.Text = title;
        PathText.Text = path;
        _singleTrackData = audioData;
        _singleTrackFormat = format;

        SingleTrackBorder.Visibility = Visibility.Visible;
        TrackListBorder.Visibility = Visibility.Collapsed;
        SingleTrackName.Text = title;
        SingleTrackFormat.Text = format.ToUpperInvariant();

        _ = PlayAudioWithConversionAsync(audioData, format, title);
        StatusText.Text = IsSupportedFormat(format)
            ? $"Loading: {title}..."
            : $"Playback not supported for: {format.ToUpperInvariant()} (export it and convert externally)";
    }

    public void SetSoundBank(string title, string path, List<AudioTrackInfo> tracks)
    {
        TitleText.Text = title;
        PathText.Text = path;
        _tracks = tracks;

        TrackListBorder.Visibility = Visibility.Visible;
        SingleTrackBorder.Visibility = Visibility.Collapsed;

        TrackListView.ItemsSource = tracks;
        StatusText.Text = $"{tracks.Count} track(s) found. Select a track to play.";

        if (tracks.Count > 0)
        {
            TrackListView.SelectedIndex = 0;
        }
    }

    private async void TrackListView_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (TrackListView.SelectedItem is not AudioTrackInfo track)
        {
            return;
        }

        if (track.Data == null && track.DataLoader != null)
        {
            StatusText.Text = $"Loading: {track.Name}...";

            try
            {
                var loaded = await track.DataLoader();
                if (loaded != null)
                {
                    track.Data = loaded.Data;
                    track.Format = loaded.Format;
                    track.OriginalFormat = loaded.OriginalFormat;
                }
            }
            catch (Exception ex)
            {
                StatusText.Text = $"Failed to load track: {ex.Message}";
                return;
            }
        }

        if (track.Data != null)
        {
            await PlayAudioWithConversionAsync(track.Data, track.Format, track.Name);
        }
    }

    private async Task PlayAudioWithConversionAsync(byte[] audioData, string format, string? displayName = null)
    {
        try
        {
            var ext = format.ToLowerInvariant();
            if (!IsSupportedFormat(ext))
            {
                StatusText.Text = $"Playback not supported for: {ext.ToUpperInvariant()} (export it and convert externally)";
                return;
            }

            if (NeedsConversion(ext))
            {
                StatusText.Text = $"Converting {ext.ToUpperInvariant()}...";
                var result = await AudioConverterService.TryConvertAsync(audioData, ext);
                if (result == null)
                {
                    StatusText.Text = ext == "wem"
                        ? $"Cannot play this WEM file (Opus/XMA codec). Place vgmstream-cli.exe in the app folder for full WEM support."
                        : $"Failed to convert {ext.ToUpperInvariant()} for playback.";
                    return;
                }
                audioData = result.Data;
                format = result.Format;
            }

            PlayAudioDirect(audioData, format);

            if (displayName != null)
            {
                var original = ext == format ? "" : $" ({ext.ToUpperInvariant()} → {format.ToUpperInvariant()})";
                StatusText.Text = $"Playing: {displayName}{original}";
            }
            else
            {
                var original = ext == format ? "" : $" ({ext.ToUpperInvariant()} → {format.ToUpperInvariant()})";
                StatusText.Text = $"Playing{original}";
            }
        }
        catch (Exception ex)
        {
            StatusText.Text = $"Failed to play audio: {ex.Message}";
        }
    }

    private void PlayAudioDirect(byte[] audioData, string format)
    {
        try
        {
            _mediaPlayer?.Pause();

            var stream = new MemoryStream(audioData);
            var randomAccessStream = new RandomAccessStream(stream);

            var source = MediaSource.CreateFromStream(randomAccessStream, GetMimeType(format));
            _mediaPlayer!.Source = source;
            _mediaPlayer.Play();
        }
        catch (Exception ex)
        {
            StatusText.Text = $"Failed to play audio: {ex.Message}";
        }
    }

    private static bool IsSupportedFormat(string format) => IsDirectlyPlayableFormat(format) || NeedsConversion(format);

    private static bool IsDirectlyPlayableFormat(string format) => format.ToLowerInvariant() switch
    {
        "wav" => true,
        "ogg" => true,
        "flac" => true,
        "mp4" => true,
        "opus" => true,
        "mp3" => true,
        "mid" => true,
        _ => false
    };

    private static bool NeedsConversion(string format) => format.ToLowerInvariant() switch
    {
        "wem" or "hca" or "adx" => true,
        _ => false
    };

    private static string GetMimeType(string format) => format.ToLowerInvariant() switch
    {
        "wav" => "audio/wav",
        "ogg" => "audio/ogg",
        "flac" => "audio/flac",
        "mp4" => "video/mp4",
        "opus" => "audio/ogg",
        "mp3" => "audio/mpeg",
        "mid" => "audio/midi",
        "wem" => "audio/x-wem",
        "hca" => "audio/x-hca",
        "adx" => "audio/x-adx",
        "xvag" => "audio/x-xvag",
        "at9" => "audio/x-at9",
        _ => "application/octet-stream"
    };

    private void OnPlaybackStateChanged(MediaPlaybackSession sender, object args)
    {
        DispatcherQueue.TryEnqueue(() =>
        {
            var isPlaying = sender.PlaybackState == MediaPlaybackState.Playing;
            PlayPauseIcon.Glyph = isPlaying ? "\uE769" : "\uE768";
            if (isPlaying)
                _positionTimer?.Start();
            else
                _positionTimer?.Stop();
        });
    }

    private void UpdatePositionUI()
    {
        if (_mediaPlayer == null || _isDraggingSlider) return;

        var session = _mediaPlayer.PlaybackSession;
        if (session.NaturalDuration.TotalSeconds > 0)
        {
            ProgressSlider.Maximum = session.NaturalDuration.TotalSeconds;
            ProgressSlider.Value = session.Position.TotalSeconds;
            TimeText.Text = $"{session.Position:mm\\:ss} / {session.NaturalDuration:mm\\:ss}";
        }
        else
        {
            TimeText.Text = $"{session.Position:mm\\:ss} / 0:00";
        }
    }

    private void PlayPause_Click(object sender, RoutedEventArgs e)
    {
        if (_mediaPlayer == null) return;
        if (_mediaPlayer.PlaybackSession.PlaybackState == MediaPlaybackState.Playing)
            _mediaPlayer.Pause();
        else
            _mediaPlayer.Play();
    }

    private void ProgressSlider_PointerPressed(object sender, Microsoft.UI.Xaml.Input.PointerRoutedEventArgs e)
    {
        _isDraggingSlider = true;
    }

    private void ProgressSlider_PointerReleased(object sender, Microsoft.UI.Xaml.Input.PointerRoutedEventArgs e)
    {
        _isDraggingSlider = false;
        if (_mediaPlayer != null)
        {
            _mediaPlayer.PlaybackSession.Position = TimeSpan.FromSeconds(ProgressSlider.Value);
        }
    }

    private void OnMediaEnded(MediaPlayer sender, object args)
    {

        if (_tracks != null && TrackListView.SelectedIndex >= 0 && TrackListView.SelectedIndex < _tracks.Count - 1)
        {
            DispatcherQueue.TryEnqueue(() =>
            {
                TrackListView.SelectedIndex++;
            });
        }
    }

    private void OnMediaFailed(MediaPlayer sender, MediaPlayerFailedEventArgs args)
    {
        DispatcherQueue.TryEnqueue(() =>
        {
            StatusText.Text = $"Playback failed: {args.Error}";
        });
    }

    private void DisposePlayer()
    {
        if (_isClosing)
        {
            return;
        }

        _isClosing = true;
        try
        {
            _positionTimer?.Stop();
            if (_mediaPlayer != null)
            {
                _mediaPlayer.MediaEnded -= OnMediaEnded;
                _mediaPlayer.MediaFailed -= OnMediaFailed;
                _mediaPlayer.PlaybackSession.PlaybackStateChanged -= OnPlaybackStateChanged;
                _mediaPlayer.Pause();
            }

            MediaPlayerElement.SetMediaPlayer(null);

            _mediaPlayer?.Dispose();
            _mediaPlayer = null;
            _singleTrackData = null;
            _tracks = null;
        }
        catch
        {

        }
    }

    private async void Export_Click(object sender, RoutedEventArgs e)
    {
        try
        {
            byte[]? exportData = null;
            string exportFormat = "wav";
            string fileName = TitleText.Text;

            if (_tracks != null && TrackListView.SelectedItem is AudioTrackInfo track)
            {
                exportData = track.Data;
                exportFormat = track.Format;
                fileName = track.Name;
            }
            else if (_singleTrackData != null)
            {
                exportData = _singleTrackData;
                exportFormat = _singleTrackFormat;
            }

            if (exportData == null)
            {
                StatusText.Text = "No audio data to export.";
                return;
            }

            var picker = new FileSavePicker();
            picker.SuggestedStartLocation = PickerLocationId.MusicLibrary;
            picker.SuggestedFileName = fileName;
            picker.FileTypeChoices.Add($"{exportFormat.ToUpperInvariant()} Audio", [$".{exportFormat}"]);

            var hwnd = WinRT.Interop.WindowNative.GetWindowHandle(this);
            WinRT.Interop.InitializeWithWindow.Initialize(picker, hwnd);

            var file = await picker.PickSaveFileAsync();
            if (file != null)
            {
                await File.WriteAllBytesAsync(file.Path, exportData);
                StatusText.Text = $"Exported to: {file.Path}";
            }
        }
        catch (Exception ex)
        {
            StatusText.Text = $"Export failed: {ex.Message}";
        }
    }

    private void Close_Click(object sender, RoutedEventArgs e)
    {
        DisposePlayer();
        try
        {
            Close();
        }
        catch
        {

        }
    }
}

internal sealed class RandomAccessStream : IRandomAccessStream
{
    private readonly MemoryStream _stream;

    public RandomAccessStream(MemoryStream stream)
    {
        _stream = stream;
        _stream.Position = 0;
    }

    public RandomAccessStream(byte[] data)
    {
        _stream = new MemoryStream(data);
        _stream.Position = 0;
    }

    public bool CanRead => true;
    public bool CanWrite => false;
    public ulong Position => (ulong)_stream.Position;
    public ulong Size
    {
        get => (ulong)_stream.Length;
        set => _stream.SetLength((long)value);
    }

    public IRandomAccessStream CloneStream()
    {
        var data = _stream.ToArray();
        return new RandomAccessStream(data);
    }

    public void Seek(ulong position)
    {
        _stream.Position = (long)position;
    }

    public IInputStream GetInputStreamAt(ulong position)
    {
        _stream.Position = (long)position;
        return _stream.AsInputStream();
    }

    public IOutputStream GetOutputStreamAt(ulong position)
    {
        throw new NotSupportedException();
    }

    public Windows.Foundation.IAsyncOperationWithProgress<IBuffer, uint> ReadAsync(IBuffer buffer, uint count, InputStreamOptions options)
    {
        return _stream.AsInputStream().ReadAsync(buffer, count, options);
    }

    public Windows.Foundation.IAsyncOperationWithProgress<uint, uint> WriteAsync(IBuffer buffer)
    {
        throw new NotSupportedException();
    }

    public Windows.Foundation.IAsyncOperation<bool> FlushAsync()
    {
        throw new NotSupportedException();
    }

    public void Dispose()
    {
        _stream.Dispose();
    }
}
