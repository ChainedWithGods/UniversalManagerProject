using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using PakViewer.Models;
using PakViewer.Settings;
using PakViewer.Services;
using System.IO;
using Windows.Storage.Pickers;
using WinRT.Interop;
using Serilog;

namespace PakViewer.Views;

public sealed partial class ViewerSettingsWindow : Window
{

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);

    private static readonly IntPtr HWND_TOPMOST = new IntPtr(-1);
    private static readonly IntPtr HWND_NOTOPMOST = new IntPtr(-2);
    private const uint SWP_NOMOVE = 0x0002;
    private const uint SWP_NOSIZE = 0x0001;
    private const uint SWP_SHOWWINDOW = 0x0040;

    public string ScreenshotPath { get; private set; }
    private MaterialPreviewData? _materialData;
    private bool _materialInspectorLoaded = false;
    private bool _dependencyGraphLoaded = false;
    private List<DependencyNode>? _dependencyNodes;
    private List<string> _loadedMaterialNames = new();
    private readonly Dictionary<string, BlockedMaterialEntry> _blockedMaterials = new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, int> _textureChannelModes = new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, bool> _materialVisibility = new(StringComparer.OrdinalIgnoreCase);

    public event Action<string, int, string?>? TextureClicked;
    public event Action? ResetOriginalTexturesClicked;
    public event Action<string, bool>? BlockedMaterialVisibilityChanged;
    public event Action<string>? BlockedMaterialRemoved;
    public event Action<string, string>? ExportTextureClicked;
    public event Action<string, string>? LoadCustomTextureClicked;
    public event Action<string, bool>? MaterialVisibilityToggled;

    public ViewerSettingsWindow()
    {
        this.InitializeComponent();
        ScreenshotPath = "";
    }

    public ViewerSettingsWindow(
        string screenshotPath,
        List<MeshSectionInfo> meshSections,
        MaterialPreviewData? materialData = null,
        List<DependencyNode>? dependencyNodes = null,
        List<string>? loadedMaterialNames = null,
        List<BlockedMaterialEntry>? blockedMaterials = null) : this()
    {
        ScreenshotPath = screenshotPath;
        ScreenshotPathTextBox.Text = screenshotPath;
        _materialData = materialData;
        _dependencyNodes = dependencyNodes;
        _loadedMaterialNames = loadedMaterialNames ?? new List<string>();
        SetBlockedMaterials(blockedMaterials);

        Title = materialData != null ? "Material Viewer - Controls & Settings" : "3D Model Viewer - Controls & Settings";

        var appWindow = this.AppWindow;
        appWindow.Resize(new Windows.Graphics.SizeInt32(720, 600));

        BackfaceCullingToggle.IsOn = PakViewer.Snooper.UserSettings.Default.DisableBackfaceCulling;
        AlwaysOnTopToggle.IsOn = true;

        MeshSectionsList.ItemsSource = meshSections;

        BuildBlockedMaterials();

    }

    private async void BrowseButton_Click(object sender, RoutedEventArgs e)
    {
        var folderPicker = new FolderPicker();
        folderPicker.SuggestedStartLocation = PickerLocationId.PicturesLibrary;
        folderPicker.FileTypeFilter.Add("*");

        var hWnd = WindowNative.GetWindowHandle(this);
        InitializeWithWindow.Initialize(folderPicker, hWnd);

        var folder = await folderPicker.PickSingleFolderAsync();
        if (folder != null)
        {
             ScreenshotPath = folder.Path;
             ScreenshotPathTextBox.Text = folder.Path;
        }
    }

    private void BackfaceCullingToggle_Changed(object sender, RoutedEventArgs e)
    {
        PakViewer.Snooper.UserSettings.Default.DisableBackfaceCulling = BackfaceCullingToggle.IsOn;
    }

    private void AlwaysOnTopToggle_Changed(object sender, RoutedEventArgs e)
    {
        var hwnd = WindowNative.GetWindowHandle(this);

        if (AlwaysOnTopToggle.IsOn)
        {

            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        }
        else
        {

            SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        }
    }

    private void ToggleSection_Click(object sender, RoutedEventArgs e)
    {
        if (sender is FrameworkElement element && element.DataContext is MeshSectionInfo info)
        {
            info.Toggle();
        }
    }

    public void UpdateMeshSections(List<MeshSectionInfo> meshSections)
    {
        MeshSectionsList.ItemsSource = null;
        MeshSectionsList.ItemsSource = meshSections;
    }

    public void UpdateDependencyGraph(List<DependencyNode> nodes)
    {
        _dependencyNodes = nodes;
        if (_dependencyGraphLoaded)
        {
            BuildDependencyGraph(nodes);
        }
    }

    public void UpdateBlockedMaterials(List<BlockedMaterialEntry> blockedMaterials)
    {
        SetBlockedMaterials(blockedMaterials);
        BuildBlockedMaterials();
    }

    private void BuildDependencyGraph(List<DependencyNode> nodes)
    {
        DependencyTreeView.ItemsSource = null;
        DependencyTreeView.ItemsSource = nodes;
    }

    private void DependencyTreeView_ItemInvoked(TreeView sender, TreeViewItemInvokedEventArgs args)
    {
        if (args.InvokedItem is DependencyNode node && node.Type == DependencyNodeType.Texture)
        {

            if (!_textureChannelModes.ContainsKey(node.Path))
            {
                _textureChannelModes[node.Path] = 0;
            }
            var channelMode = _textureChannelModes[node.Path];
            TextureClicked?.Invoke(node.Path, channelMode, null);
        }
    }

    private void CloseButton_Click(object sender, RoutedEventArgs e)
    {
        Close();
    }

    private void Pivot_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {

        if (MainPivot.SelectedIndex == 4 && !_materialInspectorLoaded && _materialData != null)
        {
            _materialInspectorLoaded = true;
            BuildMaterialInspector(_materialData);
        }

        if (MainPivot.SelectedIndex == 6 && !_dependencyGraphLoaded && _dependencyNodes != null)
        {
            _dependencyGraphLoaded = true;
            BuildDependencyGraph(_dependencyNodes);
        }
    }

    private void BuildMaterialInspector(MaterialPreviewData materialData)
    {
        MaterialInspectorContent.Children.Clear();

        var resetButton = new Button
        {
            Content = "Reset to Original Textures",
            FontSize = 14,
            FontWeight = Microsoft.UI.Text.FontWeights.SemiBold,
            Margin = new Thickness(0, 0, 0, 24),
            Padding = new Thickness(12, 8, 12, 8),
            Background = new SolidColorBrush(Microsoft.UI.Colors.Blue) { Opacity = 0.7 },
            Foreground = new SolidColorBrush(Microsoft.UI.Colors.White),
            HorizontalAlignment = HorizontalAlignment.Stretch
        };
        resetButton.Click += (s, e) => ResetOriginalTexturesClicked?.Invoke();
        MaterialInspectorContent.Children.Add(resetButton);

        if (materialData.Materials.Count > 0)
        {
            var materialHeader = new TextBlock
            {
                Text = "Materials",
                FontSize = 18,
                FontWeight = Microsoft.UI.Text.FontWeights.SemiBold,
                Margin = new Thickness(0, 0, 0, 12)
            };
            MaterialInspectorContent.Children.Add(materialHeader);

            foreach (var materialGroup in materialData.Materials)
            {
                BuildMaterialGroupExpander(materialGroup);
            }
        }
    }

    private void BuildMaterialGroupExpander(MaterialGroup materialGroup)
    {

        var expander = new Expander
        {
            IsExpanded = materialGroup.IsExpanded,
            Margin = new Thickness(0, 0, 0, 12),
            HorizontalAlignment = HorizontalAlignment.Stretch,
            HorizontalContentAlignment = HorizontalAlignment.Stretch
        };

        var headerGrid = new Grid();
        headerGrid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        headerGrid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

        var headerStack = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 8
        };

        var materialNameBlock = new TextBlock
        {
            Text = $"Material: {materialGroup.Name}",
            FontSize = 16,
            FontWeight = Microsoft.UI.Text.FontWeights.SemiBold
        };
        headerStack.Children.Add(materialNameBlock);

        Grid.SetColumn(headerStack, 0);
        headerGrid.Children.Add(headerStack);

        var statsText = new TextBlock
        {
            Text = $"({materialGroup.TextureCount} textures, {materialGroup.ScalarCount} scalars, {materialGroup.ColorCount} colors)",
            FontSize = 12,
            Opacity = 0.6,
            VerticalAlignment = VerticalAlignment.Center,
            Margin = new Thickness(8, 0, 0, 0)
        };
        Grid.SetColumn(statsText, 1);
        headerGrid.Children.Add(statsText);

        expander.Header = headerGrid;

        var contentStack = new StackPanel { Spacing = 8, Margin = new Thickness(0, 8, 0, 0) };

        if (materialGroup.Textures.Count > 0)
        {
            foreach (var tex in materialGroup.Textures)
            {
                var textureCard = CreateTextureCard(tex, materialGroup.Name);
                contentStack.Children.Add(textureCard);
            }
        }

        if (materialGroup.Scalars.Count > 0)
        {
            var scalarHeader = new TextBlock
            {
                Text = "Scalars",
                FontSize = 14,
                FontWeight = Microsoft.UI.Text.FontWeights.SemiBold,
                Opacity = 0.8,
                Margin = new Thickness(0, 12, 0, 8)
            };
            contentStack.Children.Add(scalarHeader);

            var scalarBorder = new Border
            {
                BorderBrush = new SolidColorBrush(Microsoft.UI.Colors.Gray) { Opacity = 0.2 },
                BorderThickness = new Thickness(1),
                CornerRadius = new CornerRadius(8),
                Padding = new Thickness(16),
                Background = new SolidColorBrush(Microsoft.UI.Colors.White) { Opacity = 0.03 }
            };

            var scalarStack = new StackPanel { Spacing = 12 };

            foreach (var scalar in materialGroup.Scalars)
            {
                var grid = new Grid();
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

                var nameBlock = new TextBlock
                {
                    Text = scalar.Name,
                    FontSize = 13,
                    VerticalAlignment = VerticalAlignment.Center
                };
                Grid.SetColumn(nameBlock, 0);

                var valueBlock = new TextBlock
                {
                    Text = scalar.Value.ToString("F4"),
                    FontSize = 13,
                    FontFamily = new FontFamily("Consolas"),
                    Opacity = 0.8,
                    VerticalAlignment = VerticalAlignment.Center,
                    FontWeight = Microsoft.UI.Text.FontWeights.SemiBold
                };
                Grid.SetColumn(valueBlock, 1);

                grid.Children.Add(nameBlock);
                grid.Children.Add(valueBlock);
                scalarStack.Children.Add(grid);
            }

            scalarBorder.Child = scalarStack;
            contentStack.Children.Add(scalarBorder);
        }

        if (materialGroup.Colors.Count > 0)
        {
            var colorHeader = new TextBlock
            {
                Text = "Colors",
                FontSize = 14,
                FontWeight = Microsoft.UI.Text.FontWeights.SemiBold,
                Opacity = 0.8,
                Margin = new Thickness(0, 12, 0, 8)
            };
            contentStack.Children.Add(colorHeader);

            foreach (var vec in materialGroup.Colors)
            {
                var border = new Border
                {
                    BorderBrush = new SolidColorBrush(Microsoft.UI.Colors.Gray) { Opacity = 0.2 },
                    BorderThickness = new Thickness(1),
                    CornerRadius = new CornerRadius(8),
                    Padding = new Thickness(12),
                    Margin = new Thickness(0, 0, 0, 8),
                    Background = new SolidColorBrush(Microsoft.UI.Colors.White) { Opacity = 0.03 }
                };

                var grid = new Grid();
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

                var r = (byte)Math.Clamp(vec.R * 255, 0, 255);
                var g = (byte)Math.Clamp(vec.G * 255, 0, 255);
                var b = (byte)Math.Clamp(vec.B * 255, 0, 255);

                var colorRect = new Border
                {
                    Width = 40,
                    Height = 40,
                    Background = new SolidColorBrush(Windows.UI.Color.FromArgb(255, r, g, b)),
                    BorderBrush = new SolidColorBrush(Microsoft.UI.Colors.Gray) { Opacity = 0.3 },
                    BorderThickness = new Thickness(1),
                    CornerRadius = new CornerRadius(6),
                    Margin = new Thickness(0, 0, 16, 0)
                };
                grid.Children.Add(colorRect);
                Grid.SetColumn(colorRect, 0);

                var nameBlock = new TextBlock
                {
                    Text = vec.Name,
                    FontSize = 14,
                    FontWeight = Microsoft.UI.Text.FontWeights.SemiBold,
                    VerticalAlignment = VerticalAlignment.Center
                };
                grid.Children.Add(nameBlock);
                Grid.SetColumn(nameBlock, 1);

                var rgbaText = new TextBlock
                {
                    Text = $"R:{vec.R:0.##} G:{vec.G:0.##} B:{vec.B:0.##} A:{vec.A:0.##}",
                    FontSize = 11,
                    FontFamily = new FontFamily("Consolas"),
                    Opacity = 0.6,
                    VerticalAlignment = VerticalAlignment.Center,
                    Margin = new Thickness(16, 0, 0, 0)
                };
                grid.Children.Add(rgbaText);
                Grid.SetColumn(rgbaText, 2);

                border.Child = grid;
                contentStack.Children.Add(border);
            }
        }

        expander.Content = contentStack;

        expander.Expanding += (s, e) => materialGroup.IsExpanded = true;
        expander.Collapsed += (s, e) => materialGroup.IsExpanded = false;

        MaterialInspectorContent.Children.Add(expander);
    }

    private Border CreateTextureCard(MaterialTextureParam tex, string? materialName = null)
    {
        var border = new Border
        {
            BorderBrush = new SolidColorBrush(Microsoft.UI.Colors.Gray) { Opacity = 0.2 },
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(8),
            Padding = new Thickness(12),
            Margin = new Thickness(0, 0, 0, 8),
            Background = new SolidColorBrush(Microsoft.UI.Colors.White) { Opacity = 0.03 }
        };

        var mainGrid = new Grid();
        mainGrid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        mainGrid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        mainGrid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });

        var topGrid = new Grid();
        topGrid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        topGrid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });

        if (tex.Thumbnail != null)
        {
            var img = new Image
            {
                Source = tex.Thumbnail,
                Width = 64,
                Height = 64,
                Stretch = Stretch.UniformToFill,
                Margin = new Thickness(0, 0, 16, 0)
            };
            var clipRect = new RectangleGeometry { Rect = new Windows.Foundation.Rect(0, 0, 64, 64) };
            img.Clip = clipRect;

            topGrid.Children.Add(img);
            Grid.SetColumn(img, 0);
        }

        var stack = new StackPanel { VerticalAlignment = VerticalAlignment.Center };

        var nameBlock = new TextBlock
        {
            Text = tex.Name,
            FontSize = 14,
            FontWeight = Microsoft.UI.Text.FontWeights.SemiBold,
            Margin = new Thickness(0, 0, 0, 4)
        };
        stack.Children.Add(nameBlock);

        var pathBlock = new TextBlock
        {
            Text = tex.Path,
            FontSize = 11,
            Opacity = 0.5,
            TextWrapping = TextWrapping.Wrap,
            MaxLines = 2,
            TextTrimming = TextTrimming.CharacterEllipsis
        };
        stack.Children.Add(pathBlock);

        topGrid.Children.Add(stack);
        Grid.SetColumn(stack, 1);

        Grid.SetRow(topGrid, 0);
        mainGrid.Children.Add(topGrid);

        var capturedPath = tex.Path;

        if (!_textureChannelModes.ContainsKey(capturedPath))
        {
            _textureChannelModes[capturedPath] = 0;
        }

        var channelCombo = new ComboBox
        {
            HorizontalAlignment = HorizontalAlignment.Stretch,
            Margin = new Thickness(0, 8, 0, 0),
            SelectedIndex = _textureChannelModes[capturedPath]
        };

        channelCombo.Items.Add("RGB (Normal View)");
        channelCombo.Items.Add("Red Channel Only");
        channelCombo.Items.Add("Green Channel Only");
        channelCombo.Items.Add("Blue Channel Only");
        channelCombo.Items.Add("Alpha Channel Only");
        channelCombo.Items.Add("Grayscale");

        channelCombo.SelectionChanged += (s, e) =>
        {
            var newChannelMode = channelCombo.SelectedIndex;
            _textureChannelModes[capturedPath] = newChannelMode;

            TextureClicked?.Invoke(capturedPath, newChannelMode, materialName);
        };

        Grid.SetRow(channelCombo, 1);
        mainGrid.Children.Add(channelCombo);

        var bottomGrid = new Grid
        {
            Margin = new Thickness(0, 8, 0, 0)
        };
        bottomGrid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        bottomGrid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

        var buttonStack = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 8,
            HorizontalAlignment = HorizontalAlignment.Left
        };

        var capturedMaterialNames = tex.MaterialNames;

        var previewBtn = new Button
        {
            Content = new StackPanel
            {
                Orientation = Orientation.Horizontal,
                Spacing = 6,
                Children =
                {
                    new FontIcon { Glyph = "\uE8B3", FontSize = 14 },
                    new TextBlock { Text = "Preview", FontSize = 12 }
                }
            },
            Padding = new Thickness(10, 6, 10, 6),
            Background = new SolidColorBrush(Microsoft.UI.Colors.Blue) { Opacity = 0.2 }
        };
        previewBtn.Click += (s, e) =>
        {
            var currentChannelMode = _textureChannelModes.ContainsKey(capturedPath) ? _textureChannelModes[capturedPath] : 0;
            TextureClicked?.Invoke(capturedPath, currentChannelMode, materialName);
        };
        buttonStack.Children.Add(previewBtn);

        var loadBtn = new Button
        {
            Content = new StackPanel
            {
                Orientation = Orientation.Horizontal,
                Spacing = 6,
                Children =
                {
                    new FontIcon { Glyph = "\uE8E5", FontSize = 14 },
                    new TextBlock { Text = "Load Custom", FontSize = 12 }
                }
            },
            Padding = new Thickness(10, 6, 10, 6)
        };
        loadBtn.Click += async (s, e) =>
        {
            var openPicker = new FileOpenPicker();
            openPicker.FileTypeFilter.Add(".png");
            openPicker.FileTypeFilter.Add(".jpg");
            openPicker.FileTypeFilter.Add(".jpeg");
            openPicker.FileTypeFilter.Add(".tga");
            openPicker.FileTypeFilter.Add(".bmp");
            openPicker.SuggestedStartLocation = PickerLocationId.PicturesLibrary;

            var hwnd = WindowNative.GetWindowHandle(this);
            InitializeWithWindow.Initialize(openPicker, hwnd);

            var file = await openPicker.PickSingleFileAsync();
            if (file != null)
            {
                LoadCustomTextureClicked?.Invoke(capturedPath, file.Path);
            }
        };
        buttonStack.Children.Add(loadBtn);

        var exportBtn = new Button
        {
            Content = new StackPanel
            {
                Orientation = Orientation.Horizontal,
                Spacing = 6,
                Children =
                {
                    new FontIcon { Glyph = "\uE74E", FontSize = 14 },
                    new TextBlock { Text = "Export PNG", FontSize = 12 }
                }
            },
            Padding = new Thickness(10, 6, 10, 6)
        };
        exportBtn.Click += async (s, e) =>
        {
            var savePicker = new FileSavePicker();
            savePicker.SuggestedFileName = System.IO.Path.GetFileNameWithoutExtension(capturedPath);
            savePicker.FileTypeChoices.Add("PNG Image", new[] { ".png" });
            savePicker.SuggestedStartLocation = PickerLocationId.PicturesLibrary;

            var hwnd = WindowNative.GetWindowHandle(this);
            InitializeWithWindow.Initialize(savePicker, hwnd);

            var file = await savePicker.PickSaveFileAsync();
            if (file != null)
            {
                ExportTextureClicked?.Invoke(capturedPath, file.Path);
            }
        };
        buttonStack.Children.Add(exportBtn);

        Grid.SetColumn(buttonStack, 0);
        bottomGrid.Children.Add(buttonStack);

        if (capturedMaterialNames != null && capturedMaterialNames.Count > 0)
        {
            var initiallyVisible = true;
            foreach (var matName in capturedMaterialNames)
            {
                if (_materialVisibility.TryGetValue(matName, out var visible))
                {
                    if (!visible)
                    {
                        initiallyVisible = false;
                        break;
                    }
                }
                else
                {
                    _materialVisibility[matName] = true;
                }
            }

            var visibilityButton = new Button
            {
                Content = new StackPanel
                {
                    Orientation = Orientation.Horizontal,
                    Spacing = 6,
                    Children =
                    {
                        new FontIcon { Glyph = initiallyVisible ? "\uE890" : "\uE8F4", FontSize = 14 },
                        new TextBlock { Text = initiallyVisible ? "Hide" : "Show", FontSize = 12 }
                    }
                },
                Padding = new Thickness(10, 6, 10, 6),
                Background = new SolidColorBrush(initiallyVisible ? Microsoft.UI.Colors.Orange : Microsoft.UI.Colors.Green) { Opacity = 0.2 }
            };

            visibilityButton.Click += (s, e) =>
            {
                var currentlyVisible = true;
                foreach (var matName in capturedMaterialNames)
                {
                    if (_materialVisibility.TryGetValue(matName, out var visible))
                    {
                        if (!visible)
                        {
                            currentlyVisible = false;
                            break;
                        }
                    }
                }

                var newVisibility = !currentlyVisible;

                foreach (var matName in capturedMaterialNames)
                {
                    _materialVisibility[matName] = newVisibility;
                    MaterialVisibilityToggled?.Invoke(matName, newVisibility);
                }

                var iconGlyph = newVisibility ? "\uE890" : "\uE8F4";
                var buttonText = newVisibility ? "Hide" : "Show";
                var buttonColor = newVisibility ? Microsoft.UI.Colors.Orange : Microsoft.UI.Colors.Green;

                if (visibilityButton.Content is StackPanel sp)
                {
                    if (sp.Children[0] is FontIcon icon)
                        icon.Glyph = iconGlyph;
                    if (sp.Children[1] is TextBlock tb)
                        tb.Text = buttonText;
                }
                visibilityButton.Background = new SolidColorBrush(buttonColor) { Opacity = 0.2 };

                Log.Information("Toggled visibility for texture {TextureName} → {State} (affects {Count} material(s): {Materials})",
                    tex.Name, newVisibility ? "visible" : "hidden", capturedMaterialNames.Count, string.Join(", ", capturedMaterialNames));
            };

            Grid.SetColumn(visibilityButton, 1);
            bottomGrid.Children.Add(visibilityButton);
        }

        Grid.SetRow(bottomGrid, 2);
        mainGrid.Children.Add(bottomGrid);

        border.Child = mainGrid;
        return border;
    }

    private void BuildBlockedMaterials()
    {
        BlockedMaterialsContent.Children.Clear();

        var infoBorder = new Border
        {
            Background = new SolidColorBrush(Microsoft.UI.Colors.DeepSkyBlue) { Opacity = 0.12 },
            BorderBrush = new SolidColorBrush(Microsoft.UI.Colors.DeepSkyBlue) { Opacity = 0.4 },
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(8),
            Padding = new Thickness(16),
            Margin = new Thickness(0, 0, 0, 16)
        };
        var infoStack = new StackPanel();
        infoStack.Children.Add(new TextBlock
        {
            Text = "If the mesh looks weird, you can block some materials to fix it.",
            FontSize = 14,
            FontWeight = Microsoft.UI.Text.FontWeights.SemiBold,
            TextWrapping = TextWrapping.Wrap,
            Margin = new Thickness(0, 0, 0, 6)
        });
        infoStack.Children.Add(new TextBlock
        {
            Text = "Why does it look weird? Developers use invisible materials for physics collisions, cloth simulation, and hitboxes. These aren't meant to be visible but still get loaded. Blocking them restores the intended look.",
            FontSize = 12,
            Opacity = 0.7,
            TextWrapping = TextWrapping.Wrap
        });
        infoBorder.Child = infoStack;
        BlockedMaterialsContent.Children.Add(infoBorder);

        var headerRow = new Grid
        {
            Margin = new Thickness(0, 0, 0, 8)
        };
        headerRow.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        headerRow.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

        var headerText = new TextBlock
        {
            Text = "Blocked Materials",
            FontSize = 20,
            FontWeight = Microsoft.UI.Text.FontWeights.SemiBold,
            VerticalAlignment = VerticalAlignment.Center
        };
        Grid.SetColumn(headerText, 0);

        var addBtn = new Button
        {
            Content = new StackPanel
            {
                Orientation = Orientation.Horizontal,
                Spacing = 6,
                Children =
                {
                    new FontIcon { Glyph = "\uE710", FontSize = 14 },
                    new TextBlock { Text = "Add Material", FontSize = 13 }
                }
            },
            Padding = new Thickness(12, 6, 12, 6),
            VerticalAlignment = VerticalAlignment.Center
        };
        Grid.SetColumn(addBtn, 1);

        var flyout = new MenuFlyout();
        if (_loadedMaterialNames.Count == 0)
        {
            flyout.Items.Add(new MenuFlyoutItem { Text = "(No materials loaded)", IsEnabled = false });
        }
        else
        {
            foreach (var matName in _loadedMaterialNames)
            {
                var captured = matName;
                var item = new MenuFlyoutItem { Text = matName };
                item.Click += (s, e) => AddCustomBlockedMaterial(captured);
                flyout.Items.Add(item);
            }
        }
        addBtn.Flyout = flyout;

        headerRow.Children.Add(headerText);
        headerRow.Children.Add(addBtn);
        BlockedMaterialsContent.Children.Add(headerRow);

        foreach (var entry in _blockedMaterials.Values
                     .OrderByDescending(x => x.IsAutoDetected)
                     .ThenBy(x => x.Name, StringComparer.OrdinalIgnoreCase))
        {
            BlockedMaterialsContent.Children.Add(CreateBlockedMaterialRow(entry));
        }

        if (_blockedMaterials.Count > 0)
            return;

        var emptyHint = new TextBlock
        {
            Text = "No materials blocked yet. Click \"Add Material\" to block one.",
            FontSize = 13,
            Opacity = 0.4,
            TextWrapping = TextWrapping.Wrap,
            Margin = new Thickness(0, 8, 0, 0)
        };
        BlockedMaterialsContent.Children.Add(emptyHint);
    }

    private Border CreateBlockedMaterialRow(BlockedMaterialEntry entry)
    {
        var border = new Border
        {
            Background = new SolidColorBrush(Microsoft.UI.Colors.White) { Opacity = 0.03 },
            BorderBrush = new SolidColorBrush(Microsoft.UI.Colors.Gray) { Opacity = 0.2 },
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(8),
            Padding = new Thickness(12),
            Margin = new Thickness(0, 0, 0, 4),
            Tag = entry.Name
        };

        var grid = new Grid();
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

        var capturedName = entry.Name;
        var textStack = new StackPanel
        {
            Spacing = 2,
            VerticalAlignment = VerticalAlignment.Center
        };
        textStack.Children.Add(new TextBlock
        {
            Text = capturedName,
            FontWeight = Microsoft.UI.Text.FontWeights.SemiBold
        });
        if (!string.IsNullOrWhiteSpace(entry.Reason))
        {
            textStack.Children.Add(new TextBlock
            {
                Text = entry.IsAutoDetected ? $"Auto-detected: {entry.Reason}" : entry.Reason,
                FontSize = 12,
                Opacity = 0.65,
                TextWrapping = TextWrapping.Wrap
            });
        }
        Grid.SetColumn(textStack, 0);

        var toggle = new ToggleSwitch
        {
            OffContent = "Hidden",
            OnContent = "Shown",
            IsOn = entry.IsShown,
            VerticalAlignment = VerticalAlignment.Center
        };
        toggle.Toggled += (s, e) =>
        {
            if (_blockedMaterials.TryGetValue(capturedName, out var blockedMaterial))
                blockedMaterial.IsShown = toggle.IsOn;
            BlockedMaterialVisibilityChanged?.Invoke(capturedName, toggle.IsOn);
        };
        Grid.SetColumn(toggle, 1);

        var removeBtn = new Button
        {
            Content = new FontIcon { Glyph = "\uE711", FontSize = 12 },
            Padding = new Thickness(4),
            MinWidth = 0,
            MinHeight = 0,
            VerticalAlignment = VerticalAlignment.Center,
            Margin = new Thickness(8, 0, 0, 0),
            Visibility = entry.IsAutoDetected ? Visibility.Collapsed : Visibility.Visible
        };
        removeBtn.Click += (s, e) =>
        {
            _blockedMaterials.Remove(capturedName);
            BlockedMaterialVisibilityChanged?.Invoke(capturedName, true);
            BlockedMaterialRemoved?.Invoke(capturedName);
            BuildBlockedMaterials();
        };
        Grid.SetColumn(removeBtn, 2);

        grid.Children.Add(textStack);
        grid.Children.Add(toggle);
        grid.Children.Add(removeBtn);

        border.Child = grid;
        return border;
    }

    private void AddCustomBlockedMaterial(string materialName)
    {
        if (_blockedMaterials.ContainsKey(materialName))
            return;

        _blockedMaterials[materialName] = new BlockedMaterialEntry
        {
            Name = materialName,
            IsShown = false,
            IsAutoDetected = false
        };

        BuildBlockedMaterials();
        BlockedMaterialVisibilityChanged?.Invoke(materialName, false);
    }

    private void SetBlockedMaterials(IEnumerable<BlockedMaterialEntry>? blockedMaterials)
    {
        _blockedMaterials.Clear();
        if (blockedMaterials == null)
            return;

        foreach (var entry in blockedMaterials)
        {
            if (string.IsNullOrWhiteSpace(entry.Name))
                continue;

            _blockedMaterials[entry.Name] = new BlockedMaterialEntry
            {
                Name = entry.Name,
                IsShown = entry.IsShown,
                IsAutoDetected = entry.IsAutoDetected,
                Reason = entry.Reason
            };
        }
    }
}

public class MeshSectionInfo : System.ComponentModel.INotifyPropertyChanged
{
    public string Shortcut { get; set; } = "";
    public string Name { get; set; } = "";

    private string _status = "";
    public string Status
    {
        get => _status;
        set
        {
            if (_status != value)
            {
                _status = value;
                OnPropertyChanged(nameof(Status));
            }
        }
    }

    public int SectionIndex { get; set; }
    public Action? ToggleAction { get; set; }

    public void Toggle()
    {
        ToggleAction?.Invoke();
    }

    public event System.ComponentModel.PropertyChangedEventHandler? PropertyChanged;

    protected void OnPropertyChanged(string propertyName)
    {
        PropertyChanged?.Invoke(this, new System.ComponentModel.PropertyChangedEventArgs(propertyName));
    }
}
