using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.Web.WebView2.Core;
using Serilog;
using System;
using System.IO;
using System.Net;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using PakViewer.Helpers;
using PakViewer.Views;
using Windows.ApplicationModel.DataTransfer;
using Windows.System;

namespace PakViewer.Controls;

public sealed partial class TextPreviewControl : UserControl
{

    private const int NavigateToStringMaxUtf8Bytes = 1_800_000;
    private static readonly Regex AssetPathRegex = new(@"^(?:/?Game/|/?[A-Za-z0-9_]+/).+/.+", RegexOptions.Compiled | RegexOptions.IgnoreCase | RegexOptions.CultureInvariant);

    private readonly List<CodeViewerWindow> _viewerWindows = new();
    private string[] _lines = Array.Empty<string>();
    private string? _activeHtmlPreviewPath;
    private string _rawText = "";
    private string _extension = "";
    private bool _wordWrap;
    private bool _isWebViewReady;
    private bool _pendingSearchRefresh;
    private bool _showOpenInWindowButton = true;

    public static readonly DependencyProperty FileNameProperty =
        DependencyProperty.Register(nameof(FileName), typeof(string), typeof(TextPreviewControl), new PropertyMetadata(""));

    public static readonly DependencyProperty FilePathProperty =
        DependencyProperty.Register(nameof(FilePath), typeof(string), typeof(TextPreviewControl), new PropertyMetadata(""));

    public static readonly DependencyProperty FileSizeProperty =
        DependencyProperty.Register(nameof(FileSize), typeof(long), typeof(TextPreviewControl), new PropertyMetadata(0L));

    public static readonly DependencyProperty LineCountProperty =
        DependencyProperty.Register(nameof(LineCount), typeof(int), typeof(TextPreviewControl), new PropertyMetadata(0));

    public static readonly DependencyProperty EncodingNameProperty =
        DependencyProperty.Register(nameof(EncodingName), typeof(string), typeof(TextPreviewControl), new PropertyMetadata(""));

    public string FileName
    {
        get => (string)GetValue(FileNameProperty);
        set => SetValue(FileNameProperty, value);
    }

    public string FilePath
    {
        get => (string)GetValue(FilePathProperty);
        set => SetValue(FilePathProperty, value);
    }

    public long FileSize
    {
        get => (long)GetValue(FileSizeProperty);
        set => SetValue(FileSizeProperty, value);
    }

    public int LineCount
    {
        get => (int)GetValue(LineCountProperty);
        set => SetValue(LineCountProperty, value);
    }

    public string EncodingName
    {
        get => (string)GetValue(EncodingNameProperty);
        set => SetValue(EncodingNameProperty, value);
    }

    public bool ShowOpenInWindowButton
    {
        get => _showOpenInWindowButton;
        set
        {
            _showOpenInWindowButton = value;
            if (OpenInWindowButton != null)
            {
                OpenInWindowButton.Visibility = value ? Visibility.Visible : Visibility.Collapsed;
            }
        }
    }

    public bool IsWordWrapEnabled => _wordWrap;

    public event Action<string>? AssetPathClicked;

    public TextPreviewControl()
    {
        InitializeComponent();
        Loaded += TextPreviewControl_Loaded;
        WordWrapToggle.Click += WordWrapToggle_Click;
        SearchToggleButton.Click += SearchToggleButton_Click;
        FindPreviousButton.Click += FindPreviousButton_Click;
        FindNextButton.Click += FindNextButton_Click;
        MatchCaseToggle.Click += MatchCaseToggle_Click;
        CloseSearchButton.Click += CloseSearchButton_Click;
        CopyAllButton.Click += CopyAllButton_Click;
        OpenInWindowButton.Click += OpenInWindowButton_Click;
        SearchTextBox.KeyDown += SearchTextBox_KeyDown;
        SearchTextBox.TextChanged += SearchTextBox_TextChanged;
        ContentWebView.NavigationCompleted += ContentWebView_NavigationCompleted;
        ActualThemeChanged += TextPreviewControl_ActualThemeChanged;

        UpdateWordWrapVisualState();
        UpdateSearchStatusText("Enter text to find");
        ShowOpenInWindowButton = _showOpenInWindowButton;
    }

    public void SetWordWrap(bool enabled)
    {
        _wordWrap = enabled;
        UpdateWordWrapVisualState();
    }

    public async Task OpenInWindowAsync()
    {
        if (string.IsNullOrEmpty(_rawText))
        {
            return;
        }

        try
        {
            var window = new CodeViewerWindow();
            window.Closed += (_, _) => _viewerWindows.Remove(window);
            _viewerWindows.Add(window);

            await window.LoadContentAsync(
                _rawText,
                _extension,
                FileName,
                FilePath,
                FileSize,
                string.IsNullOrWhiteSpace(EncodingName) ? null : EncodingName,
                _wordWrap);

            window.Activate();
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to open standalone code viewer for {FilePath}", FilePath);
        }
    }

    private async void TextPreviewControl_Loaded(object sender, RoutedEventArgs e)
    {
        await EnsureWebViewReadyAsync();
    }

    private async Task EnsureWebViewReadyAsync()
    {
        if (_isWebViewReady)
            return;

        try
        {

            var userDataFolder = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "UniversalManager", "PakViewer", "WebView2");
            Directory.CreateDirectory(userDataFolder);

            var environment = await CoreWebView2Environment.CreateWithOptionsAsync(
                browserExecutableFolder: null,
                userDataFolder: userDataFolder,
                options: null);
            await ContentWebView.EnsureCoreWebView2Async(environment);

            if (ContentWebView.CoreWebView2 != null)
            {
                var settings = ContentWebView.CoreWebView2.Settings;
                settings.AreDefaultContextMenusEnabled = false;
                settings.AreDevToolsEnabled = false;
                settings.AreBrowserAcceleratorKeysEnabled = false;
                settings.IsStatusBarEnabled = false;
                settings.IsZoomControlEnabled = false;
                ContentWebView.CoreWebView2.WebMessageReceived += CoreWebView2_WebMessageReceived;
                _isWebViewReady = true;
            }
        }
        catch (Exception ex)
        {
            Log.Warning(ex, "Failed to initialize WebView2 for text preview");
        }
    }

    private async void TextPreviewControl_ActualThemeChanged(FrameworkElement sender, object args)
    {
        if (!string.IsNullOrEmpty(_rawText))
            await RenderContentAsync();
    }

    private async void ContentWebView_NavigationCompleted(WebView2 sender, CoreWebView2NavigationCompletedEventArgs args)
    {
        if (!_pendingSearchRefresh || SearchBar.Visibility != Visibility.Visible || string.IsNullOrWhiteSpace(SearchTextBox.Text))
        {
            return;
        }

        _pendingSearchRefresh = false;
        await RefreshSearchAsync(resetToBoundary: true, forward: true);
    }

    private void CoreWebView2_WebMessageReceived(CoreWebView2 sender, CoreWebView2WebMessageReceivedEventArgs args)
    {
        try
        {
            using var message = JsonDocument.Parse(args.WebMessageAsJson);
            if (!message.RootElement.TryGetProperty("type", out var typeElement) || typeElement.GetString() != "assetPathClick")
            {
                return;
            }

            if (!message.RootElement.TryGetProperty("path", out var pathElement))
            {
                return;
            }

            var path = pathElement.GetString();
            if (string.IsNullOrWhiteSpace(path))
            {
                return;
            }

            AssetPathClicked?.Invoke(path);
        }
        catch
        {

        }
    }

    private async void WordWrapToggle_Click(object sender, RoutedEventArgs e)
    {
        _wordWrap = WordWrapToggle.IsChecked == true;
        UpdateWordWrapVisualState();

        if (!string.IsNullOrEmpty(_rawText))
            await RenderContentAsync();
    }

    private async void SearchToggleButton_Click(object sender, RoutedEventArgs e)
    {
        var isVisible = SearchToggleButton.IsChecked == true;
        SearchBar.Visibility = isVisible ? Visibility.Visible : Visibility.Collapsed;

        if (!isVisible)
        {
            await ClearSearchHighlightsAsync(updateMessage: false);
            UpdateSearchStatusText(string.IsNullOrWhiteSpace(SearchTextBox.Text) ? "Enter text to find" : "Press Enter to search");
            return;
        }

        SearchTextBox.Focus(FocusState.Programmatic);
        SearchTextBox.SelectAll();
        UpdateSearchStatusText(string.IsNullOrWhiteSpace(SearchTextBox.Text) ? "Enter text to find" : "Press Enter to search");
    }

    private async void CloseSearchButton_Click(object sender, RoutedEventArgs e)
    {
        SearchToggleButton.IsChecked = false;
        SearchBar.Visibility = Visibility.Collapsed;
        await ClearSearchHighlightsAsync(updateMessage: false);
        UpdateSearchStatusText(string.IsNullOrWhiteSpace(SearchTextBox.Text) ? "Enter text to find" : "Press Enter to search");
    }

    private async void FindPreviousButton_Click(object sender, RoutedEventArgs e)
    {
        await RefreshSearchAsync(resetToBoundary: false, forward: false);
    }

    private async void FindNextButton_Click(object sender, RoutedEventArgs e)
    {
        await RefreshSearchAsync(resetToBoundary: false, forward: true);
    }

    private async void MatchCaseToggle_Click(object sender, RoutedEventArgs e)
    {
        if (string.IsNullOrWhiteSpace(SearchTextBox.Text))
        {
            UpdateSearchStatusText("Enter text to find");
            return;
        }

        await RefreshSearchAsync(resetToBoundary: true, forward: true);
    }

    private async void SearchTextBox_KeyDown(object sender, KeyRoutedEventArgs e)
    {
        if (e.Key != VirtualKey.Enter)
        {
            return;
        }

        e.Handled = true;
        await RefreshSearchAsync(resetToBoundary: false, forward: true);
    }

    private async void SearchTextBox_TextChanged(object sender, TextChangedEventArgs e)
    {
        if (SearchBar.Visibility != Visibility.Visible)
        {
            return;
        }

        await ClearSearchHighlightsAsync(updateMessage: false);
        UpdateSearchStatusText(string.IsNullOrWhiteSpace(SearchTextBox.Text) ? "Enter text to find" : "Press Enter to search");
    }

    private void CopyAllButton_Click(object sender, RoutedEventArgs e)
    {
        if (string.IsNullOrEmpty(_rawText))
        {
            return;
        }

        try
        {
            var package = new DataPackage();
            package.SetText(_rawText);
            Clipboard.SetContent(package);
            UpdateSearchStatusText("Copied all text");
        }
        catch (Exception ex)
        {
            Log.Warning(ex, "Failed to copy preview text to clipboard");
        }
    }

    private async void OpenInWindowButton_Click(object sender, RoutedEventArgs e)
    {
        await OpenInWindowAsync();
    }

    public async Task LoadContentAsync(string text, string extension, string fileName, string filePath, long fileSize, string? encoding = null)
    {
        ShowLoading(true);

        try
        {
            _extension = extension;
            _rawText = NormalizeLineEndings(text);
            FileName = fileName;
            FilePath = filePath;
            FileSize = fileSize;
            EncodingName = encoding ?? "";

            _lines = await Task.Run(() => _rawText.Split('\n'));
            LineCount = _lines.Length;

            UpdateHeader(fileName, extension, fileSize, LineCount, encoding);

            await RenderContentAsync();
        }
        finally
        {
            ShowLoading(false);
        }
    }

    private void UpdateHeader(string fileName, string extension, long fileSize, int lineCount, string? encoding)
    {
        HeaderBar.Visibility = Visibility.Visible;
        HeaderFileName.Text = System.IO.Path.GetFileName(fileName);
        HeaderExtension.Text = string.IsNullOrWhiteSpace(extension) ? "TEXT" : extension.TrimStart('.').ToUpperInvariant();
        HeaderSize.Text = FormatFileSize(fileSize);
        HeaderLineCount.Text = lineCount == 1 ? "1 line" : $"{lineCount:N0} lines";

        if (!string.IsNullOrEmpty(encoding))
        {
            HeaderEncoding.Text = encoding;
            HeaderEncoding.Visibility = Visibility.Visible;
        }
        else
        {
            HeaderEncoding.Visibility = Visibility.Collapsed;
        }
    }

    private async Task RenderContentAsync()
    {
        await EnsureWebViewReadyAsync();
        if (!_isWebViewReady || ContentWebView.CoreWebView2 == null)
            return;

        if (_lines.Length == 0)
        {
            await NavigateHtmlAsync(BuildPlainHtmlDocument(Array.Empty<string>(), _wordWrap, GetHtmlTheme()));
            return;
        }

        var isDarkTheme = IsDarkThemeActive();
        var theme = GetHtmlTheme(isDarkTheme);
        SyntaxHighlighter.ThemeColors? syntaxColors = null;
        if (ShouldUseFullHighlight())
            syntaxColors = SyntaxHighlighter.CaptureThemeColors(isDarkTheme);

        var text = _rawText;
        var lines = _lines;
        var extension = _extension;
        var wrap = _wordWrap;

        var html = await Task.Run(() =>
        {
            if (syntaxColors != null)
            {
                var segments = GetHighlightSegments(text, extension, syntaxColors);
                if (segments != null)
                    return BuildHighlightedHtmlDocument(segments, wrap, theme);
            }

            return BuildPlainHtmlDocument(lines, wrap, theme);
        });

        _pendingSearchRefresh = SearchBar.Visibility == Visibility.Visible && !string.IsNullOrWhiteSpace(SearchTextBox.Text);
        await NavigateHtmlAsync(html);
    }

    private async Task NavigateHtmlAsync(string html)
    {
        var previousPreviewPath = _activeHtmlPreviewPath;
        _activeHtmlPreviewPath = null;

        if (ContentWebView.CoreWebView2 == null)
        {
            DeleteTempHtmlPreview(previousPreviewPath);
            return;
        }

        try
        {
            if (Encoding.UTF8.GetByteCount(html) <= NavigateToStringMaxUtf8Bytes)
            {
                try
                {
                    ContentWebView.NavigateToString(html);
                    DeleteTempHtmlPreview(previousPreviewPath);
                    return;
                }
                catch (ArgumentException ex)
                {
                    Log.Warning(ex, "NavigateToString rejected generated HTML for {FilePath}; retrying via temp file", FilePath);
                }
                catch (InvalidOperationException ex) when (ContentWebView.CoreWebView2 == null)
                {
                    Log.Warning(ex, "NavigateToString failed because CoreWebView2 is not present for {FilePath}; retrying via temp file", FilePath);
                }
                catch (Exception ex)
                {
                    Log.Warning(ex, "NavigateToString failed for {FilePath}; retrying via temp file", FilePath);
                }
            }
            else
            {
                Log.Information("Preview HTML exceeded NavigateToString limit, using temp file navigation for {FilePath}", FilePath);
            }
        }
        catch (Exception ex)
        {
            Log.Warning(ex, "Failed to measure preview HTML for {FilePath}; falling back to temp file", FilePath);
        }

        var previewDirectory = Path.Combine(Path.GetTempPath(), "PakViewer", "PreviewCache");
        Directory.CreateDirectory(previewDirectory);

        var previewFilePath = Path.Combine(previewDirectory, $"preview-{Guid.NewGuid():N}.html");
        await File.WriteAllTextAsync(previewFilePath, html, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));

        _activeHtmlPreviewPath = previewFilePath;
        try
        {
            ContentWebView.Source = new Uri(previewFilePath);
        }
        catch (Exception ex)
        {
            Log.Warning(ex, "Failed to assign temp file URI to WebView2 for {FilePath}", FilePath);
        }
        DeleteTempHtmlPreview(previousPreviewPath);
    }

    private void CleanupActiveHtmlPreview()
    {
        var previewPath = _activeHtmlPreviewPath;
        _activeHtmlPreviewPath = null;
        DeleteTempHtmlPreview(previewPath);
    }

    private static void DeleteTempHtmlPreview(string? previewPath)
    {
        if (string.IsNullOrWhiteSpace(previewPath))
        {
            return;
        }

        try
        {
            if (File.Exists(previewPath))
            {
                File.Delete(previewPath);
            }
        }
        catch
        {

        }
    }

    private async Task RefreshSearchAsync(bool resetToBoundary, bool forward)
    {
        var searchText = SearchTextBox.Text?.Trim() ?? string.Empty;
        if (string.IsNullOrEmpty(searchText))
        {
            await ClearSearchHighlightsAsync(updateMessage: false);
            UpdateSearchStatusText("Enter text to find");
            return;
        }

        var result = await ExecuteSearchAsync(searchText, MatchCaseToggle.IsChecked == true, forward, resetToBoundary);
        UpdateSearchStatusText(result.Count == 0 ? "No matches" : $"{result.Current} of {result.Count}");
    }

    private async Task<SearchNavigationState> ExecuteSearchAsync(string searchText, bool caseSensitive, bool forward, bool resetToBoundary)
    {
        await EnsureWebViewReadyAsync();
        if (!_isWebViewReady || ContentWebView.CoreWebView2 == null)
        {
            return default;
        }

        var script = $"globalThis.codeViewerSearch ? globalThis.codeViewerSearch.find({JsonSerializer.Serialize(searchText)}, {JsonSerializer.Serialize(caseSensitive)}, {JsonSerializer.Serialize(forward)}, {JsonSerializer.Serialize(resetToBoundary)}) : {{ count: 0, current: 0 }};";
        var result = await ContentWebView.CoreWebView2.ExecuteScriptAsync(script);
        return ParseSearchResult(result);
    }

    private async Task ClearSearchHighlightsAsync(bool updateMessage = true)
    {
        if (_isWebViewReady && ContentWebView.CoreWebView2 != null)
        {
            await ContentWebView.CoreWebView2.ExecuteScriptAsync("globalThis.codeViewerSearch ? globalThis.codeViewerSearch.clear() : null;");
        }

        if (updateMessage)
        {
            UpdateSearchStatusText("Enter text to find");
        }
    }

    private static SearchNavigationState ParseSearchResult(string json)
    {
        if (string.IsNullOrWhiteSpace(json))
        {
            return default;
        }

        try
        {
            using var document = JsonDocument.Parse(json);
            var root = document.RootElement;
            if (root.ValueKind != JsonValueKind.Object)
            {
                return default;
            }

            int count = root.TryGetProperty("count", out var countElement) ? countElement.GetInt32() : 0;
            int current = root.TryGetProperty("current", out var currentElement) ? currentElement.GetInt32() : 0;
            return new SearchNavigationState(count, current);
        }
        catch
        {
            return default;
        }
    }

    private void UpdateSearchStatusText(string message)
    {
        SearchResultText.Text = message;
    }

    private void UpdateWordWrapVisualState()
    {
        WordWrapToggle.IsChecked = _wordWrap;

        if (_wordWrap)
        {
            WrapIcon.Foreground = (Brush)Application.Current.Resources["AccentFillColorDefaultBrush"];
        }
        else
        {
            WrapIcon.ClearValue(IconElement.ForegroundProperty);
        }
    }

    private bool ShouldUseFullHighlight()
    {
        return true;
    }

    private static List<HighlightSegment>? GetHighlightSegments(string rawText, string extension, SyntaxHighlighter.ThemeColors colors)
    {
        var ext = extension.ToLowerInvariant().TrimStart('.');
        var trimmedStart = rawText.TrimStart();
        bool startsWithJson = trimmedStart.StartsWith("{") || trimmedStart.StartsWith("[");
        bool isUnrealAsset = ext is "uasset" or "umap" or "uexp" or "ubulk" or "utexture";
        bool looksLikeJson = startsWithJson || isUnrealAsset || ext == "json";
        bool looksLikeIni = !looksLikeJson &&
                           (ext is "ini" or "cfg" or "conf" or "txt" or "log" or "xml" or "csv" or "yaml" or "yml" ||
                            (rawText.Contains('=') && rawText.Contains('\n')));

        if (looksLikeIni)
            return SyntaxHighlighter.GetIniSegments(rawText, colors);

        if (looksLikeJson)
            return SyntaxHighlighter.GetJsonSegments(rawText, colors);

        return null;
    }

    private bool IsDarkThemeActive()
    {
        return ActualTheme == ElementTheme.Dark;
    }

    private HtmlTheme GetHtmlTheme()
    {
        return GetHtmlTheme(IsDarkThemeActive());
    }

    private static HtmlTheme GetHtmlTheme(bool isDarkTheme)
    {
        return isDarkTheme
            ? new HtmlTheme("#0F172A", "#111827", "#1E293B", "#E5E7EB", "#64748B", "#111827", "#475569", "#64748B", "dark")
            : new HtmlTheme("#FFFFFF", "#F8FAFC", "#D7DFEA", "#111827", "#64748B", "#F1F5F9", "#CBD5E1", "#94A3B8", "light");
    }

    private static string BuildPlainHtmlDocument(string[] lines, bool wrap, HtmlTheme theme)
    {
        var html = new StringBuilder(4096 + (lines.Length * 64));
        AppendHtmlHeader(html, wrap, theme);

        for (int i = 0; i < lines.Length; i++)
        {
            AppendLineStart(html, i + 1);
            AppendPlainCode(html, lines[i]);
            AppendLineEnd(html);
        }

        if (lines.Length == 0)
        {
            AppendLineStart(html, 1);
            AppendPlainCode(html, string.Empty);
            AppendLineEnd(html);
        }

        AppendHtmlFooter(html);
        return html.ToString();
    }

    private static string BuildHighlightedHtmlDocument(IReadOnlyList<HighlightSegment> segments, bool wrap, HtmlTheme theme)
    {
        var html = new StringBuilder(Math.Max(4096, segments.Count * 40));
        AppendHtmlHeader(html, wrap, theme);

        int lineNumber = 1;
        bool hasContent = false;
        AppendLineStart(html, lineNumber);

        foreach (var segment in segments)
        {
            if (segment.IsLineBreak)
            {
                if (!hasContent)
                    html.Append("&nbsp;");

                AppendLineEnd(html);
                lineNumber++;
                AppendLineStart(html, lineNumber);
                hasContent = false;
                continue;
            }

            hasContent |= AppendHighlightedSegment(html, segment);
        }

        if (!hasContent)
            html.Append("&nbsp;");

        AppendLineEnd(html);
        AppendHtmlFooter(html);
        return html.ToString();
    }

    private static void AppendHtmlHeader(StringBuilder html, bool wrap, HtmlTheme theme)
    {
        var whiteSpace = wrap ? "pre-wrap" : "pre";
        var overflowWrap = wrap ? "anywhere" : "normal";

        html.Append("<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='color-scheme' content='").Append(theme.ColorScheme).Append("'>");
        html.Append("<style>");
        html.Append("html{color-scheme:").Append(theme.ColorScheme).Append(";}");
        html.Append("html,body{margin:0;padding:0;background:").Append(theme.Background).Append(";color:").Append(theme.Foreground).Append(";font-family:Consolas,'Cascadia Code','Courier New',monospace;font-size:12px;line-height:1.45;scrollbar-color:").Append(theme.ScrollbarThumb).Append(' ').Append(theme.ScrollbarTrack).Append(";}");
        html.Append("body{overflow:auto;}");
        html.Append("::-webkit-scrollbar{width:12px;height:12px;}");
        html.Append("::-webkit-scrollbar-track{background:").Append(theme.ScrollbarTrack).Append(";}");
        html.Append("::-webkit-scrollbar-thumb{background:").Append(theme.ScrollbarThumb).Append(";border-radius:999px;border:3px solid ").Append(theme.ScrollbarTrack).Append(";}");
        html.Append("::-webkit-scrollbar-thumb:hover{background:").Append(theme.ScrollbarThumbHover).Append(";}");
        html.Append("::-webkit-scrollbar-corner{background:").Append(theme.ScrollbarTrack).Append(";}");
        html.Append(".editor{width:100%;box-sizing:border-box;}");
        html.Append(".line{display:grid;grid-template-columns:56px minmax(0,1fr);}");
        html.Append(".ln{padding:2px 10px 2px 0;text-align:right;background:").Append(theme.GutterBackground).Append(";color:").Append(theme.LineNumber).Append(";border-right:1px solid ").Append(theme.Border).Append(";user-select:none;}");
        html.Append(".code{padding:2px 12px;white-space:").Append(whiteSpace).Append(";overflow-wrap:").Append(overflowWrap).Append(";tab-size:4;}");
        html.Append(".asset-link{cursor:pointer;text-decoration:underline;text-decoration-thickness:1px;text-underline-offset:2px;}");
        html.Append(".asset-link:hover{opacity:0.92;filter:saturate(1.1);}");
        html.Append("mark.search-hit{background:#FDE68A;color:inherit;padding:0;border-radius:2px;}");
        html.Append("mark.search-hit.current{background:#F59E0B;box-shadow:0 0 0 1px rgba(0,0,0,0.18);}");
        html.Append("</style>");
        html.Append("""
<script>
window.codeViewerSearch = (function () {
    let matches = [];
    let currentIndex = -1;
    let lastTerm = "";
    let lastCaseSensitive = false;

    function unwrapMatch(mark) {
        const parent = mark.parentNode;
        if (!parent) {
            return;
        }

        parent.replaceChild(document.createTextNode(mark.textContent || ""), mark);
        parent.normalize();
    }

    function clear() {
        document.querySelectorAll("mark.search-hit").forEach(unwrapMatch);
        matches = [];
        currentIndex = -1;
        lastTerm = "";
        return { count: 0, current: 0 };
    }

    function collectMatches(term, caseSensitive) {
        matches = [];
        currentIndex = -1;
        if (!term) {
            return;
        }

        const normalizedTerm = caseSensitive ? term : term.toLowerCase();
        document.querySelectorAll(".code").forEach(codeBlock => {
            const walker = document.createTreeWalker(codeBlock, NodeFilter.SHOW_TEXT);
            const textNodes = [];

            while (walker.nextNode()) {
                if (walker.currentNode.nodeValue && walker.currentNode.nodeValue.length > 0) {
                    textNodes.push(walker.currentNode);
                }
            }

            textNodes.forEach(node => {
                const source = node.nodeValue || "";
                const searchable = caseSensitive ? source : source.toLowerCase();
                let start = 0;
                let index = searchable.indexOf(normalizedTerm, start);
                if (index < 0) {
                    return;
                }

                const fragment = document.createDocumentFragment();
                while (index >= 0) {
                    if (index > start) {
                        fragment.appendChild(document.createTextNode(source.slice(start, index)));
                    }

                    const mark = document.createElement("mark");
                    mark.className = "search-hit";
                    mark.textContent = source.slice(index, index + term.length);
                    fragment.appendChild(mark);
                    matches.push(mark);

                    start = index + term.length;
                    index = searchable.indexOf(normalizedTerm, start);
                }

                if (start < source.length) {
                    fragment.appendChild(document.createTextNode(source.slice(start)));
                }

                if (node.parentNode) {
                    node.parentNode.replaceChild(fragment, node);
                }
            });
        });

        lastTerm = term;
        lastCaseSensitive = caseSensitive;
    }

    function selectMatch(index) {
        if (!matches.length) {
            currentIndex = -1;
            return { count: 0, current: 0 };
        }

        if (currentIndex >= 0 && currentIndex < matches.length) {
            matches[currentIndex].classList.remove("current");
        }

        currentIndex = ((index % matches.length) + matches.length) % matches.length;
        const current = matches[currentIndex];
        current.classList.add("current");
        current.scrollIntoView({ block: "center", inline: "nearest", behavior: "auto" });
        return { count: matches.length, current: currentIndex + 1 };
    }

    function find(term, caseSensitive, forward, reset) {
        if (!term) {
            return clear();
        }

        const needsRebuild = reset || term !== lastTerm || caseSensitive !== lastCaseSensitive || !matches.length;
        if (needsRebuild) {
            clear();
            collectMatches(term, caseSensitive);
            if (!matches.length) {
                return { count: 0, current: 0 };
            }

            return selectMatch(forward === false ? matches.length - 1 : 0);
        }

        return selectMatch(currentIndex + (forward === false ? -1 : 1));
    }

    return { clear, find };
})();

(function () {
    document.addEventListener("click", function (event) {
        var link = event.target.closest(".asset-link[data-asset-path]");
        if (!link) {
            return;
        }

        event.preventDefault();
        if (window.chrome && window.chrome.webview) {
            window.chrome.webview.postMessage({
                type: "assetPathClick",
                path: link.getAttribute("data-asset-path")
            });
        }
    });
})();
</script>
""");
        html.Append("</head><body><div class='editor'>");
    }

    private static void AppendHtmlFooter(StringBuilder html)
    {
        html.Append("</div></body></html>");
    }

    private static void AppendLineStart(StringBuilder html, int lineNumber)
    {
        html.Append("<div class='line'><div class='ln'>")
            .Append(lineNumber)
            .Append("</div><div class='code'>");
    }

    private static void AppendLineEnd(StringBuilder html)
    {
        html.Append("</div></div>");
    }

    private static void AppendPlainCode(StringBuilder html, string text)
    {
        if (string.IsNullOrEmpty(text))
        {
            html.Append("&nbsp;");
            return;
        }

        html.Append(WebUtility.HtmlEncode(text));
    }

    private static bool AppendHighlightedSegment(StringBuilder html, HighlightSegment segment)
    {
        if (string.IsNullOrEmpty(segment.Text))
            return false;

        var hasPath = TryGetNavigableAssetPathFromSegment(segment, out var navigablePath);
        var tagName = hasPath ? "a" : "span";

        html.Append('<').Append(tagName);
        if (hasPath)
        {
            html.Append(" class='asset-link' href='#' data-asset-path='")
                .Append(EncodeHtmlAttribute(navigablePath!))
                .Append("'");
        }

        if (!string.IsNullOrEmpty(segment.ColorHex) || segment.IsBold)
        {
            html.Append(" style='");
            if (!string.IsNullOrEmpty(segment.ColorHex))
                html.Append("color:").Append(segment.ColorHex).Append(';');
            if (segment.IsBold)
                html.Append("font-weight:600;");
            html.Append("'");
        }

        html.Append('>')
            .Append(WebUtility.HtmlEncode(segment.Text))
            .Append("</")
            .Append(tagName)
            .Append('>');

        return true;
    }

    private static bool TryGetNavigableAssetPathFromSegment(HighlightSegment segment, out string? path)
    {
        path = null;
        if (string.IsNullOrWhiteSpace(segment.Text) || string.IsNullOrEmpty(segment.ColorHex))
            return false;

        if (!TryDecodeJsonStringLiteral(segment.Text, out var decodedValue))
            return false;

        if (!TryExtractAssetPathCandidate(decodedValue, out var extractedPath))
            return false;

        path = NormalizeAssetPathForNavigation(extractedPath);
        return !string.IsNullOrWhiteSpace(path);
    }

    private static bool TryDecodeJsonStringLiteral(string text, out string decodedValue)
    {
        decodedValue = string.Empty;
        if (string.IsNullOrWhiteSpace(text) || text.Length < 2 || text[0] != '"' || text[^1] != '"')
            return false;

        try
        {
            var parsed = JsonSerializer.Deserialize<string>(text);
            if (string.IsNullOrWhiteSpace(parsed))
                return false;

            decodedValue = parsed;
            return true;
        }
        catch
        {
            return false;
        }
    }

    private static bool TryExtractAssetPathCandidate(string value, out string path)
    {
        path = string.Empty;
        if (string.IsNullOrWhiteSpace(value))
            return false;

        var trimmed = value.Trim();

        var firstQuote = trimmed.IndexOf('\'');
        var lastQuote = trimmed.LastIndexOf('\'');
        if (firstQuote >= 0 && lastQuote > firstQuote)
        {
            var quoted = trimmed.Substring(firstQuote + 1, lastQuote - firstQuote - 1).Trim();
            if (LooksLikeAssetPath(quoted))
            {
                path = quoted;
                return true;
            }
        }

        if ((trimmed.StartsWith("\"") && trimmed.EndsWith("\"")) ||
            (trimmed.StartsWith("'") && trimmed.EndsWith("'")))
        {
            var inner = trimmed.Substring(1, trimmed.Length - 2).Trim();
            if (LooksLikeAssetPath(inner))
            {
                path = inner;
                return true;
            }
        }

        if (LooksLikeAssetPath(trimmed))
        {
            path = trimmed;
            return true;
        }

        return false;
    }

    private static bool LooksLikeAssetPath(string value)
    {
        if (string.IsNullOrWhiteSpace(value))
            return false;

        var trimmed = value.Trim();
        if (trimmed.StartsWith("http://", StringComparison.OrdinalIgnoreCase) ||
            trimmed.StartsWith("https://", StringComparison.OrdinalIgnoreCase) ||
            trimmed.StartsWith("/Script/", StringComparison.OrdinalIgnoreCase) ||
            trimmed.StartsWith("Script/", StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }

        if (!trimmed.Contains('/'))
            return false;

        var normalized = trimmed.Replace('\\', '/');
        var colonIndex = normalized.IndexOf(':');
        if (colonIndex >= 0)
            normalized = normalized.Substring(0, colonIndex);

        return AssetPathRegex.IsMatch(normalized);
    }

    private static string NormalizeAssetPathForNavigation(string value)
    {
        var normalized = value.Trim().Replace('\\', '/').Trim('"', '\'');
        var colonIndex = normalized.IndexOf(':');
        if (colonIndex >= 0)
            normalized = normalized.Substring(0, colonIndex);

        return normalized;
    }

    private static string EncodeHtmlAttribute(string value)
    {
        return WebUtility.HtmlEncode(value).Replace("'", "&#39;");
    }

    public void Clear()
    {
        HeaderBar.Visibility = Visibility.Collapsed;
        SearchBar.Visibility = Visibility.Collapsed;
        SearchToggleButton.IsChecked = false;
        SearchTextBox.Text = "";
        UpdateSearchStatusText("Enter text to find");
        _lines = Array.Empty<string>();
        _rawText = "";
        LineCount = 0;
        FileName = "";
        FilePath = "";
        FileSize = 0;
        EncodingName = "";
        LoadingOverlay.Visibility = Visibility.Collapsed;
        CleanupActiveHtmlPreview();

        if (ContentWebView.CoreWebView2 == null)
        {
            return;
        }

        try
        {
            ContentWebView.NavigateToString(BuildPlainHtmlDocument(Array.Empty<string>(), _wordWrap, GetHtmlTheme(IsDarkThemeActive())));
        }
        catch (Exception ex)
        {
            Log.Warning(ex, "Failed to clear WebView2 preview content for {FilePath}", FilePath);
        }
    }

    private void ShowLoading(bool show)
    {
        LoadingOverlay.Visibility = show ? Visibility.Visible : Visibility.Collapsed;
        LoadingOverlay.IsHitTestVisible = show;
    }

    private static string NormalizeLineEndings(string text)
    {
        return text.Replace("\r\n", "\n").Replace('\r', '\n');
    }

    private static string FormatFileSize(long bytes)
    {
        string[] sizes = { "B", "KB", "MB", "GB", "TB" };
        double len = bytes;
        int order = 0;
        while (len >= 1024 && order < sizes.Length - 1)
        {
            order++;
            len /= 1024;
        }
        return $"{len:0.##} {sizes[order]}";
    }

    private readonly record struct SearchNavigationState(int Count, int Current);
    private sealed record HtmlTheme(
        string Background,
        string GutterBackground,
        string Border,
        string Foreground,
        string LineNumber,
        string ScrollbarTrack,
        string ScrollbarThumb,
        string ScrollbarThumbHover,
        string ColorScheme);
}
