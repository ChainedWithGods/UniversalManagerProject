using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Media.Imaging;
using PakViewer.Services;
using Windows.UI;

namespace PakViewer.Views;

public sealed partial class MaterialViewerWindow : Window
{
    public MaterialViewerWindow()
    {
        this.InitializeComponent();
        Title = "Material Viewer";

        var appWindow = this.AppWindow;
        appWindow.Resize(new Windows.Graphics.SizeInt32(960, 720));

        var iconPath = System.IO.Path.Combine(AppContext.BaseDirectory, "Icons", "3Ds_Viewer.ico");
        if (System.IO.File.Exists(iconPath))
            appWindow.SetIcon(iconPath);

        var presenter = appWindow.Presenter as OverlappedPresenter;
        if (presenter != null)
        {
            presenter.IsAlwaysOnTop = true;
        }
    }

    public void SetMaterial(string title, string path, BitmapImage? texture, MaterialPreviewData? materialData)
    {
        TitleText.Text = title;
        PathText.Text = path;

        BuildTexturePreview(materialData);
        BuildMaterialInspector(materialData);
    }

    private void BuildTexturePreview(MaterialPreviewData? materialData)
    {
        TexturePreviewPanel.Children.Clear();

        var titleBlock = new TextBlock
        {
            Text = "Material Textures",
            FontSize = 16,
            FontWeight = Microsoft.UI.Text.FontWeights.SemiBold,
            Margin = new Thickness(0, 0, 0, 12)
        };
        TexturePreviewPanel.Children.Add(titleBlock);

        if (materialData == null || materialData.TextureParams.Count == 0)
        {
            var noTexturesBlock = new TextBlock
            {
                Text = "No textures found in this material",
                Opacity = 0.7,
                HorizontalAlignment = HorizontalAlignment.Center,
                Margin = new Thickness(0, 40, 0, 0)
            };
            TexturePreviewPanel.Children.Add(noTexturesBlock);
            return;
        }

        foreach (var tex in materialData.TextureParams)
        {
            var border = new Border
            {
                BorderBrush = new SolidColorBrush(Microsoft.UI.Colors.Gray) { Opacity = 0.3 },
                BorderThickness = new Thickness(1),
                CornerRadius = new CornerRadius(8),
                Padding = new Thickness(12),
                Margin = new Thickness(0, 0, 0, 12),
                Background = new SolidColorBrush(Microsoft.UI.Colors.White) { Opacity = 0.05 }
            };

            var stack = new StackPanel { Spacing = 8 };

            var nameBlock = new TextBlock
            {
                Text = tex.Name,
                FontSize = 14,
                FontWeight = Microsoft.UI.Text.FontWeights.SemiBold
            };
            stack.Children.Add(nameBlock);

            if (tex.Thumbnail != null)
            {
                var img = new Image
                {
                    Source = tex.Thumbnail,
                    Stretch = Stretch.Uniform,
                    MaxHeight = 400,
                    HorizontalAlignment = HorizontalAlignment.Center
                };
                stack.Children.Add(img);
            }
            else
            {
                var noPreviewBlock = new TextBlock
                {
                    Text = "No preview available",
                    Opacity = 0.5,
                    HorizontalAlignment = HorizontalAlignment.Center,
                    Margin = new Thickness(0, 20, 0, 20)
                };
                stack.Children.Add(noPreviewBlock);
            }

            var pathBlock = new TextBlock
            {
                Text = tex.Path,
                FontSize = 11,
                Opacity = 0.6,
                TextWrapping = TextWrapping.Wrap
            };
            stack.Children.Add(pathBlock);

            border.Child = stack;
            TexturePreviewPanel.Children.Add(border);
        }
    }

    private void Close_Click(object sender, RoutedEventArgs e)
    {
        Close();
    }

    private void BuildMaterialInspector(MaterialPreviewData? materialData)
    {
        MaterialInspectorContent.Children.Clear();

        var titleBlock = new TextBlock
        {
            Text = "Material Inspector",
            FontSize = 16,
            FontWeight = Microsoft.UI.Text.FontWeights.SemiBold,
            Margin = new Thickness(0, 0, 0, 16)
        };
        MaterialInspectorContent.Children.Add(titleBlock);

        if (materialData == null)
        {
            var noDataBlock = new TextBlock
            {
                Text = "No material data available",
                Opacity = 0.7,
                Margin = new Thickness(0, 8, 0, 0)
            };
            MaterialInspectorContent.Children.Add(noDataBlock);
            return;
        }

        if (materialData.TextureParams.Count > 0)
        {
            var texHeader = new TextBlock
            {
                Text = "Textures",
                FontSize = 14,
                FontWeight = Microsoft.UI.Text.FontWeights.SemiBold,
                Opacity = 0.8,
                Margin = new Thickness(0, 8, 0, 8)
            };
            MaterialInspectorContent.Children.Add(texHeader);

            foreach (var tex in materialData.TextureParams)
            {
                var border = new Border
                {
                    BorderBrush = new SolidColorBrush(Microsoft.UI.Colors.Gray) { Opacity = 0.3 },
                    BorderThickness = new Thickness(1),
                    CornerRadius = new CornerRadius(6),
                    Padding = new Thickness(8),
                    Margin = new Thickness(0, 4, 0, 4),
                    Background = new SolidColorBrush(Microsoft.UI.Colors.White) { Opacity = 0.05 }
                };

                var stack = new StackPanel();

                var nameBlock = new TextBlock
                {
                    Text = tex.Name,
                    FontSize = 12,
                    FontWeight = Microsoft.UI.Text.FontWeights.Medium,
                    Margin = new Thickness(0, 0, 0, 4)
                };
                stack.Children.Add(nameBlock);

                if (tex.Thumbnail != null)
                {
                    var img = new Image
                    {
                        Source = tex.Thumbnail,
                        MaxWidth = 220,
                        MaxHeight = 140,
                        Stretch = Stretch.Uniform,
                        Margin = new Thickness(0, 4, 0, 4)
                    };
                    stack.Children.Add(img);
                }

                var pathBlock = new TextBlock
                {
                    Text = tex.Path,
                    FontSize = 10,
                    Opacity = 0.7,
                    TextWrapping = TextWrapping.Wrap,
                    Margin = new Thickness(0, 4, 0, 0)
                };
                stack.Children.Add(pathBlock);

                border.Child = stack;
                MaterialInspectorContent.Children.Add(border);
            }
        }

        if (materialData.ScalarParams.Count > 0)
        {
            var scalarHeader = new TextBlock
            {
                Text = "Scalars",
                FontSize = 14,
                FontWeight = Microsoft.UI.Text.FontWeights.SemiBold,
                Opacity = 0.8,
                Margin = new Thickness(0, 16, 0, 8)
            };
            MaterialInspectorContent.Children.Add(scalarHeader);

            foreach (var scalar in materialData.ScalarParams)
            {
                var grid = new Grid
                {
                    Margin = new Thickness(0, 4, 0, 4)
                };
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
                grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

                var nameBlock = new TextBlock
                {
                    Text = scalar.Name,
                    FontSize = 12,
                    VerticalAlignment = VerticalAlignment.Center
                };
                Grid.SetColumn(nameBlock, 0);

                var valueBlock = new TextBlock
                {
                    Text = scalar.Value.ToString("F4"),
                    FontSize = 12,
                    FontFamily = new FontFamily("Consolas"),
                    Opacity = 0.7,
                    VerticalAlignment = VerticalAlignment.Center
                };
                Grid.SetColumn(valueBlock, 1);

                grid.Children.Add(nameBlock);
                grid.Children.Add(valueBlock);
                MaterialInspectorContent.Children.Add(grid);
            }
        }

        if (materialData.VectorParams.Count > 0)
        {
            var vectorHeader = new TextBlock
            {
                Text = "Colors",
                FontSize = 14,
                FontWeight = Microsoft.UI.Text.FontWeights.SemiBold,
                Opacity = 0.8,
                Margin = new Thickness(0, 16, 0, 8)
            };
            MaterialInspectorContent.Children.Add(vectorHeader);

            foreach (var vec in materialData.VectorParams)
            {
                var border = new Border
                {
                    BorderBrush = new SolidColorBrush(Microsoft.UI.Colors.Gray) { Opacity = 0.3 },
                    BorderThickness = new Thickness(1),
                    CornerRadius = new CornerRadius(6),
                    Padding = new Thickness(8),
                    Margin = new Thickness(0, 4, 0, 4),
                    Background = new SolidColorBrush(Microsoft.UI.Colors.White) { Opacity = 0.05 }
                };

                var stack = new StackPanel();

                var nameBlock = new TextBlock
                {
                    Text = vec.Name,
                    FontSize = 12,
                    FontWeight = Microsoft.UI.Text.FontWeights.Medium,
                    Margin = new Thickness(0, 0, 0, 4)
                };
                stack.Children.Add(nameBlock);

                var colorGrid = new Grid
                {
                    Height = 32,
                    Margin = new Thickness(0, 4, 0, 4)
                };
                colorGrid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(56) });
                colorGrid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });

                var r = (byte)Math.Clamp(vec.R * 255, 0, 255);
                var g = (byte)Math.Clamp(vec.G * 255, 0, 255);
                var b = (byte)Math.Clamp(vec.B * 255, 0, 255);

                var colorRect = new Border
                {
                    Background = new SolidColorBrush(Color.FromArgb(255, r, g, b)),
                    BorderBrush = new SolidColorBrush(Microsoft.UI.Colors.Gray) { Opacity = 0.3 },
                    BorderThickness = new Thickness(1),
                    CornerRadius = new CornerRadius(4),
                    Margin = new Thickness(0, 0, 8, 0)
                };
                Grid.SetColumn(colorRect, 0);

                var rgbaText = new TextBlock
                {
                    Text = $"R: {vec.R:0.00}   G: {vec.G:0.00}   B: {vec.B:0.00}   A: {vec.A:0.00}",
                    FontSize = 11,
                    Opacity = 0.7,
                    VerticalAlignment = VerticalAlignment.Center
                };
                Grid.SetColumn(rgbaText, 1);

                colorGrid.Children.Add(colorRect);
                colorGrid.Children.Add(rgbaText);
                stack.Children.Add(colorGrid);

                border.Child = stack;
                MaterialInspectorContent.Children.Add(border);
            }
        }
    }
}
