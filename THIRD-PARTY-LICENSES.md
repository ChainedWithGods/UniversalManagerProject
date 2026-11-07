# Third-Party Licenses

This project is licensed under the **GNU General Public License v3.0** — see the [LICENSE](LICENSE) file for details.

| Component | License | Location |
|---|---|---|
| Universal Manager | GPL-3.0 | `/LICENSE` |
| UE_Detector | GPL-3.0 | `/Tools/UE_Detector/LICENSE` |
| PakViewer | GPL-3.0 | `/Tools/PakViewer/LICENSE` |
| Avalonium | GPL-3.0 | `/Tools/Avalonium/LICENSE` |
| ModConflictDetector | GPL-3.0 | `/Tools/ModConflictDetector/LICENSE` |

All tools are free and open source.

## Third-Party Dependencies

Universal Manager Project uses the following open-source software and third-party assets. Full license texts are available in the respective `licenses/` folders within each tool directory.

### PakViewer Dependencies

| Library | License | Source |
|---|---|---|
| **CSCore** | MS-PL | [GitHub](https://github.com/filoe/cscore) |
| **CUE4Parse** | Apache-2.0 | [GitHub](https://github.com/FabianFG/CUE4Parse) |
| **CUE4Parse NOTICE** | Apache-2.0 NOTICE | [GitHub](https://github.com/FabianFG/CUE4Parse/blob/master/NOTICE) |
| **CommunityToolkit.Mvvm** | MIT | [GitHub](https://github.com/CommunityToolkit/dotnet) |
| **CommunityToolkit.WinUI.UI.Controls** | MIT | [GitHub](https://github.com/CommunityToolkit/Windows) |
| **Flaticon / magnific** | Attribution Required | [Flaticon](https://www.flaticon.com/authors/magnific) |
| **Microsoft.Web.WebView2** | BSD-3-Clause | [Microsoft](https://learn.microsoft.com/en-us/microsoft-edge/webview2/) |
| **Microsoft.Windows.SDK.BuildTools** | Microsoft License | [Microsoft](https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/) |
| **Microsoft.WindowsAppSDK** | MIT | [Microsoft](https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/) |
| **Newtonsoft.Json** | MIT | [GitHub](https://github.com/JamesNK/Newtonsoft.Json) |
| **OpenTK** | MIT | [GitHub](https://github.com/opentk/opentk) |
| **Serilog** | Apache-2.0 | [serilog.net](https://serilog.net) |
| **Serilog.Sinks.Console** | Apache-2.0 | [GitHub](https://github.com/serilog/serilog-sinks-console) |
| **Serilog.Sinks.File** | Apache-2.0 | [GitHub](https://github.com/serilog/serilog-sinks-file) |
| **SixLabors.ImageSharp** | Six Labors Split License 1.0 | [GitHub](https://github.com/SixLabors/ImageSharp) |
| **SkiaSharp** | MIT | [GitHub](https://github.com/mono/SkiaSharp) |
| **Svg.Skia** | MIT | [GitHub](https://github.com/wieslawsoltes/Svg.Skia) |
| **vgmstream** | ISC | [GitHub](https://github.com/vgmstream/vgmstream) |
| **Ww2Ogg.Core** | MIT | [NuGet](https://www.nuget.org/packages/Ww2Ogg.Core) |

#### Bundled Native Libraries (via vgmstream)

The following DLLs are bundled with vgmstream for audio decoding support:

- `avcodec-vgmstream-59.dll` — [FFmpeg](https://ffmpeg.org) codec library (LGPL-2.1)
- `avformat-vgmstream-59.dll` — [FFmpeg](https://ffmpeg.org) format library (LGPL-2.1)
- `avutil-vgmstream-57.dll` — [FFmpeg](https://ffmpeg.org) utility library (LGPL-2.1)
- `swresample-vgmstream-4.dll` — [FFmpeg](https://ffmpeg.org) resampling library (LGPL-2.1)
- `libatrac9.dll` — [LibAtrac9](https://github.com/Thealexbarney/LibAtrac9) decoder
- `libcelt-0061.dll` / `libcelt-0110.dll` — [CELT codec](https://www.celt-codec.org)
- `libg719_decode.dll` — G.719 decoder
- `libmpg123-0.dll` — [MPG123](https://www.mpg123.de) MP3 decoder (LGPL-2.1)
- `libspeex-1.dll` — [Speex](https://www.speex.org) codec (BSD-3-Clause)
- `libvorbis.dll` — [Vorbis](https://xiph.org/vorbis/) codec (BSD-3-Clause)

### Avalonium Runtime Helper Dependencies

| Library | License | Source |
|---|---|---|
| **Lua 5.4.6** | MIT | [License file](/Tools/Avalonium/Helpers/Licenses/lua-5.4.6-MIT.txt) |
| **Universal-Dear-ImGui-Hook** | MIT | [GitHub](https://github.com/Sh0ckFR/Universal-Dear-ImGui-Hook) |
| **MinHook** | BSD-2-Clause | [GitHub](https://github.com/TsudaKageyu/minhook) |

### Universal Manager (Qt/C++) Dependencies

| Library | License | Notes |
|---|---|---|
| **Qt 6** | LGPL-3.0 / GPL-3.0 / Commercial | [Qt](https://www.qt.io) — Core, Gui, Qml, Quick, QuickControls2, LabsPlatform, Concurrent, etc. |
| **Lucide Icons** | ISC | [Lucide](https://lucide.dev) — SVG icon set used throughout the UI |
| **Flaticon / magnific** | Attribution Required | [Flaticon](https://www.flaticon.com/authors/magnific) — Theme icons used in the UI |
| **UIverse.io** | Various / Creative Commons | [UIverse](https://uiverse.io) — UI design inspiration and component patterns |

### ModConflictDetector

| Library | License | Source |
|---|---|---|
| **CUE4Parse** | Apache-2.0 | [GitHub](https://github.com/FabianFG/CUE4Parse) |
| **Newtonsoft.Json** | MIT | [GitHub](https://github.com/JamesNK/Newtonsoft.Json) |
| **System.CommandLine** | MIT | [GitHub](https://github.com/dotnet/command-line-api) |
