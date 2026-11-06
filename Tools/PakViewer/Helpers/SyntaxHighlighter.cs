using Microsoft.UI;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Documents;
using Microsoft.UI.Xaml.Media;
using System;
using System.Collections.Generic;
using System.Text;
using Windows.UI;

namespace PakViewer.Helpers;

public class HighlightSegment
{
    public string Text { get; set; } = "";
    public string ColorHex { get; set; } = "";
    public bool IsBold { get; set; }
    public bool IsLineBreak { get; set; }
}

public static class SyntaxHighlighter
{

    private const string L_Keyword = "#AF00DB";
    private const string L_String = "#A31515";
    private const string L_Number = "#098658";
    private const string L_Comment = "#6B7280";
    private const string L_Property = "#0451A5";
    private const string L_Value = "#795E26";
    private const string L_Bracket = "#1F2937";
    private const string L_Section = "#795E26";
    private const string L_Bool = "#0000FF";
    private const string L_Default = "#111827";
    private const string L_Punctuation = "#1F2937";
    private const string L_TypeName = "#267F99";

    private const string D_Keyword = "#C678DD";
    private const string D_String = "#98C379";
    private const string D_Number = "#D19A66";
    private const string D_Comment = "#5C6370";
    private const string D_Property = "#E06C75";
    private const string D_Value = "#D19A66";
    private const string D_Bracket = "#ABB2BF";
    private const string D_Section = "#E06C75";
    private const string D_Bool = "#56B6C2";
    private const string D_Default = "#ABB2BF";
    private const string D_Punctuation = "#C678DD";
    private const string D_TypeName = "#E5C07B";

    private static string GetKeywordHex(bool isDarkTheme) => isDarkTheme ? D_Keyword : L_Keyword;
    private static string GetStringHex(bool isDarkTheme) => isDarkTheme ? D_String : L_String;
    private static string GetNumberHex(bool isDarkTheme) => isDarkTheme ? D_Number : L_Number;
    private static string GetCommentHex(bool isDarkTheme) => isDarkTheme ? D_Comment : L_Comment;
    private static string GetPropertyHex(bool isDarkTheme) => isDarkTheme ? D_Property : L_Property;
    private static string GetValueHex(bool isDarkTheme) => isDarkTheme ? D_Value : L_Value;
    private static string GetBracketHex(bool isDarkTheme) => isDarkTheme ? D_Bracket : L_Bracket;
    private static string GetSectionHex(bool isDarkTheme) => isDarkTheme ? D_Section : L_Section;
    private static string GetBoolHex(bool isDarkTheme) => isDarkTheme ? D_Bool : L_Bool;
    private static string GetDefaultHex(bool isDarkTheme) => isDarkTheme ? D_Default : L_Default;
    private static string GetPunctuationHex(bool isDarkTheme) => isDarkTheme ? D_Punctuation : L_Punctuation;
    private static string GetTypeNameHex(bool isDarkTheme) => isDarkTheme ? D_TypeName : L_TypeName;

    public class ThemeColors
    {
        public string Keyword = "", String = "", Number = "", Comment = "",
                      Property = "", Value = "", Bracket = "", Section = "",
                      Bool = "", Default = "", Punctuation = "", TypeName = "";
    }

    public static ThemeColors CaptureThemeColors(bool? isDarkTheme = null)
    {
        var useDarkTheme = ResolveIsDarkTheme(isDarkTheme);
        return new ThemeColors
        {
            Keyword = GetKeywordHex(useDarkTheme),
            String = GetStringHex(useDarkTheme),
            Number = GetNumberHex(useDarkTheme),
            Comment = GetCommentHex(useDarkTheme),
            Property = GetPropertyHex(useDarkTheme),
            Value = GetValueHex(useDarkTheme),
            Bracket = GetBracketHex(useDarkTheme),
            Section = GetSectionHex(useDarkTheme),
            Bool = GetBoolHex(useDarkTheme),
            Default = GetDefaultHex(useDarkTheme),
            Punctuation = GetPunctuationHex(useDarkTheme),
            TypeName = GetTypeNameHex(useDarkTheme)
        };
    }

    private static bool ResolveIsDarkTheme(bool? isDarkTheme = null)
    {
        if (isDarkTheme.HasValue)
            return isDarkTheme.Value;

        try
        {
            if (Application.Current == null)
                return false;

            return Application.Current.RequestedTheme == ApplicationTheme.Dark;
        }
        catch
        {
            return false;
        }
    }

    public static void ClearCache()
    {
        lock (_brushCache)
        {
            _brushCache.Clear();
        }
    }

    private static readonly Dictionary<string, SolidColorBrush> _brushCache = new();

    private static SolidColorBrush GetBrush(string hex)
    {
        lock (_brushCache)
        {
            if (_brushCache.TryGetValue(hex, out var brush))
                return brush;
            brush = CreateBrush(hex);
            _brushCache[hex] = brush;
            return brush;
        }
    }

    private static SolidColorBrush CreateBrush(string hex)
    {
        hex = hex.TrimStart('#');
        return new SolidColorBrush(Color.FromArgb(
            255,
            Convert.ToByte(hex.Substring(0, 2), 16),
            Convert.ToByte(hex.Substring(2, 2), 16),
            Convert.ToByte(hex.Substring(4, 2), 16)
        ));
    }

    #region Thread-safe segment methods (can run on background threads)

    public static List<HighlightSegment> GetJsonSegments(string json, ThemeColors colors)
    {
        var segments = new List<HighlightSegment>();
        if (string.IsNullOrEmpty(json))
            return segments;

        var normalized = json.Replace("\r\n", "\n").Replace('\r', '\n');
        int pos = 0;
        int len = normalized.Length;

        while (pos < len)
        {
            char c = normalized[pos];

            if (c == ' ' || c == '\t')
            {
                int start = pos;
                while (pos < len && (normalized[pos] == ' ' || normalized[pos] == '\t'))
                    pos++;
                segments.Add(new HighlightSegment { Text = normalized.Substring(start, pos - start) });
                continue;
            }

            if (c == '\n')
            {
                segments.Add(new HighlightSegment { IsLineBreak = true });
                pos++;
                continue;
            }

            if (c == '"')
            {
                int start = pos;
                pos++;

                while (pos < len)
                {
                    if (normalized[pos] == '\\')
                    {
                        pos += 2;
                        continue;
                    }
                    if (normalized[pos] == '"')
                    {
                        pos++;
                        break;
                    }
                    pos++;
                }

                string strToken = normalized.Substring(start, pos - start);

                int lookAhead = pos;
                while (lookAhead < len && normalized[lookAhead] == ' ')
                    lookAhead++;

                if (lookAhead < len && normalized[lookAhead] == ':')
                {

                    segments.Add(new HighlightSegment { Text = strToken, ColorHex = colors.Property });
                }
                else
                {

                    segments.Add(new HighlightSegment { Text = strToken, ColorHex = colors.String });
                }
                continue;
            }

            if (c == '{' || c == '}' || c == '[' || c == ']')
            {
                segments.Add(new HighlightSegment { Text = c.ToString(), ColorHex = colors.Punctuation });
                pos++;
                continue;
            }
            if (c == ':')
            {
                segments.Add(new HighlightSegment { Text = c.ToString(), ColorHex = colors.Punctuation });
                pos++;
                continue;
            }
            if (c == ',')
            {
                segments.Add(new HighlightSegment { Text = c.ToString(), ColorHex = colors.Punctuation });
                pos++;
                continue;
            }

            if (c == 't' && pos + 3 < len && normalized.Substring(pos, 4) == "true")
            {
                segments.Add(new HighlightSegment { Text = "true", ColorHex = colors.Bool });
                pos += 4;
                continue;
            }
            if (c == 'f' && pos + 4 < len && normalized.Substring(pos, 5) == "false")
            {
                segments.Add(new HighlightSegment { Text = "false", ColorHex = colors.Bool });
                pos += 5;
                continue;
            }
            if (c == 'n' && pos + 3 < len && normalized.Substring(pos, 4) == "null")
            {
                segments.Add(new HighlightSegment { Text = "null", ColorHex = colors.Keyword });
                pos += 4;
                continue;
            }

            if (c == '-' || (c >= '0' && c <= '9'))
            {
                int start = pos;
                if (c == '-') pos++;

                while (pos < len && normalized[pos] >= '0' && normalized[pos] <= '9')
                    pos++;

                if (pos < len && normalized[pos] == '.')
                {
                    pos++;
                    while (pos < len && normalized[pos] >= '0' && normalized[pos] <= '9')
                        pos++;
                }

                if (pos < len && (normalized[pos] == 'e' || normalized[pos] == 'E'))
                {
                    pos++;
                    if (pos < len && (normalized[pos] == '+' || normalized[pos] == '-'))
                        pos++;
                    while (pos < len && normalized[pos] >= '0' && normalized[pos] <= '9')
                        pos++;
                }
                segments.Add(new HighlightSegment { Text = normalized.Substring(start, pos - start), ColorHex = colors.Number });
                continue;
            }

            segments.Add(new HighlightSegment { Text = c.ToString(), ColorHex = colors.Default });
            pos++;
        }

        return segments;
    }

    public static List<HighlightSegment> GetIniSegments(string ini, ThemeColors colors)
    {
        var segments = new List<HighlightSegment>();
        if (string.IsNullOrEmpty(ini))
            return segments;

        var normalized = ini.Replace("\r\n", "\n").Replace('\r', '\n');
        var lines = normalized.Split('\n');

        for (int i = 0; i < lines.Length; i++)
        {
            BuildIniLineSegments(segments, lines[i], colors);
            if (i < lines.Length - 1)
                segments.Add(new HighlightSegment { IsLineBreak = true });
        }

        return segments;
    }

    private static void BuildIniLineSegments(List<HighlightSegment> segments, string line, ThemeColors colors)
    {
        if (string.IsNullOrEmpty(line))
            return;

        var trimmedLine = line.TrimStart();
        var indent = line.Length - trimmedLine.Length;
        if (indent > 0)
        {
            segments.Add(new HighlightSegment { Text = new string(' ', indent) });
        }

        var trimmed = trimmedLine.TrimEnd();

        if (trimmed.StartsWith(";") || trimmed.StartsWith("#"))
        {
            segments.Add(new HighlightSegment { Text = trimmedLine, ColorHex = colors.Comment });
            return;
        }

        if (trimmed.StartsWith("[") && trimmed.Contains("]"))
        {
            segments.Add(new HighlightSegment { Text = trimmedLine, ColorHex = colors.Section, IsBold = true });
            return;
        }

        var equalsIndex = trimmedLine.IndexOf('=');
        if (equalsIndex > 0)
        {
            var key = trimmedLine.Substring(0, equalsIndex);
            var equals = "=";
            var value = trimmedLine.Substring(equalsIndex + 1);

            segments.Add(new HighlightSegment { Text = key, ColorHex = colors.Property });
            segments.Add(new HighlightSegment { Text = equals, ColorHex = colors.Punctuation });

            var trimmedValue = value.Trim();
            if (trimmedValue.Equals("true", StringComparison.OrdinalIgnoreCase) ||
                trimmedValue.Equals("false", StringComparison.OrdinalIgnoreCase))
            {
                segments.Add(new HighlightSegment { Text = value, ColorHex = colors.Bool });
            }
            else if (double.TryParse(trimmedValue, out _))
            {
                segments.Add(new HighlightSegment { Text = value, ColorHex = colors.Number });
            }
            else if (trimmedValue.StartsWith("\"") && trimmedValue.EndsWith("\""))
            {
                segments.Add(new HighlightSegment { Text = value, ColorHex = colors.String });
            }
            else
            {
                segments.Add(new HighlightSegment { Text = value, ColorHex = colors.Value });
            }
            return;
        }

        segments.Add(new HighlightSegment { Text = trimmedLine });
    }

    #endregion

    #region Paragraph-building methods (must run on UI thread)

    public static Paragraph BuildParagraphFromSegments(List<HighlightSegment> segments)
    {
        var paragraph = new Paragraph();
        foreach (var seg in segments)
        {
            if (seg.IsLineBreak)
            {
                paragraph.Inlines.Add(new LineBreak());
            }
            else
            {
                var run = new Run { Text = seg.Text };
                if (!string.IsNullOrEmpty(seg.ColorHex))
                    run.Foreground = GetBrush(seg.ColorHex);
                if (seg.IsBold)
                    run.FontWeight = Microsoft.UI.Text.FontWeights.SemiBold;
                paragraph.Inlines.Add(run);
            }
        }
        return paragraph;
    }

    public static async Task<Paragraph> BuildParagraphFromSegmentsAsync(IReadOnlyList<HighlightSegment> segments, int yieldEvery = 600)
    {
        var paragraph = new Paragraph();
        for (int i = 0; i < segments.Count; i++)
        {
            var seg = segments[i];
            if (seg.IsLineBreak)
            {
                paragraph.Inlines.Add(new LineBreak());
            }
            else
            {
                var run = new Run { Text = seg.Text };
                if (!string.IsNullOrEmpty(seg.ColorHex))
                    run.Foreground = GetBrush(seg.ColorHex);
                if (seg.IsBold)
                    run.FontWeight = Microsoft.UI.Text.FontWeights.SemiBold;
                paragraph.Inlines.Add(run);
            }

            if (yieldEvery > 0 && (i + 1) % yieldEvery == 0)
                await Task.Yield();
        }

        return paragraph;
    }

    public static Paragraph HighlightJson(string json)
    {
        var colors = CaptureThemeColors();
        var segments = GetJsonSegments(json, colors);
        return BuildParagraphFromSegments(segments);
    }

    public static Paragraph HighlightIni(string ini)
    {
        var colors = CaptureThemeColors();
        var segments = GetIniSegments(ini, colors);
        return BuildParagraphFromSegments(segments);
    }

    #endregion
}
