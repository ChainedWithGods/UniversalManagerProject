import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Effects
import Qt5Compat.GraphicalEffects
import QtWebEngine

Window {
    id: dialogRoot

    property var appWindow: null

    property bool parkedInCorner: false
    readonly property bool parked: parkedInCorner || visibility === Window.Minimized

    onVisibilityChanged: function(visibility) {
        if (visibility === Window.Minimized) {
            parkedInCorner = true
            visible = false
        }
    }

    function minimizeToCorner() {
        if (!visible && !parkedInCorner)
            return
        parkedInCorner = true
        visible = false
    }

    function restoreFromMinimized() {
        if (!visible && !parkedInCorner)
            return
        parkedInCorner = false
        visible = true
        showMaximized()
        raise()
        requestActivate()
    }

    readonly property bool animationsPaused: (modManager.pauseAnimationsOnDefocus
        || modManager.pauseOnDefocus) && Qt.application.state !== Qt.ApplicationActive
    readonly property bool blurReduced: modManager.pauseOnDefocus
        && Qt.application.state !== Qt.ApplicationActive

    readonly property color surfaceColor: modManager.theme === "Glassmorphic"
        ? Qt.rgba(0.12, 0.12, 0.12, 0.98)
        : (appWindow ? appWindow.surfaceColor : "#FFFFFF")
    readonly property color textColor: appWindow ? appWindow.textColor : "#3E3E3E"
    readonly property color primaryColor: appWindow ? appWindow.primaryColor : "#BE9F7E"

    readonly property bool wallpaperTheme: modManager.theme === "Crimson"
        || modManager.theme === "Glassmorphic" || modManager.theme === "Wavey"

    readonly property bool glassTheme: modManager.theme === "Glassmorphic"

    readonly property color glassSurface: appWindow
        ? appWindow.surfaceColor
        : Qt.rgba(0.05, 0.05, 0.05, Math.min(1, modManager.glassOpacity + 0.1))

    readonly property int contentMaxWidth: 1000
    readonly property real descriptionCardWidth:
        Math.min(dialogRoot.width - 48, contentMaxWidth + 36)

    ThemeConfig {
        id: themeConfig
    }

    function panelColor(fallback) {
        if (glassTheme)
            return glassSurface
        if (modManager.theme === "Wavey")
            return themeConfig.wavey.cardColor1
        if (modManager.theme === "Crimson")
            return themeConfig.crimson.cardColor1
        return fallback
    }

    title: (modData && modData.name) ? modData.name : "Mod Details"
    width: 1280
    height: 860
    minimumWidth: 900
    minimumHeight: 600
    visible: false
    color: surfaceColor
    modality: Qt.NonModal
    transientParent: appWindow
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowMinimizeButtonHint |
        Qt.WindowCloseButtonHint | Qt.WindowMaximizeButtonHint | Qt.WindowSystemMenuHint

    Component.onCompleted: {
        titleBarController.attachToWindow(dialogRoot)
        rebuildDescriptionBlocks()
    }

    onClosing: {
        previewViewer.closeViewer()
        videoWindow.close()
        avifWindow.close()
        modManager.closeNexusModDetails()
    }

    property var modData: ({})

    property int currentTab: 0

    readonly property int activeFileCount: {
        var files = modManager.currentNexusModFiles || []
        var count = 0
        for (var i = 0; i < files.length; ++i) {
            if (files[i] && files[i].category !== "OLD_VERSION")
                ++count
        }
        return count
    }

    readonly property var previewImages: {
        var images = modManager.nexusModPreviewImages || []
        if (images.length > 0) return images
        var picture = (modManager.currentNexusModDetails && modManager.currentNexusModDetails.pictureUrl)
            || (dialogRoot.modData && dialogRoot.modData.pictureUrl) || ""
        if (picture.length > 0) {
            return [{ fullUrl: picture, thumbUrl: picture, caption: "" }]
        }
        return []
    }
    readonly property bool previewSkeletonVisible:
        (modManager.isLoadingNexusModPreviews || modManager.isLoadingNexusModDetails)
        && previewImages.length === 0
    readonly property bool blurAdultMedia:
        nexusOAuth.adultContentBlurred
        && ((modManager.currentNexusModDetails && modManager.currentNexusModDetails.containsAdult)
            || (dialogRoot.modData && dialogRoot.modData.adultContent))

    function colorToHex(c) {
        var toByte = function(v) {
            var s = Math.round(Math.max(0, Math.min(1, v)) * 255).toString(16)
            return s.length < 2 ? "0" + s : s
        }
        return "#" + toByte(c.r) + toByte(c.g) + toByte(c.b)
    }

    function rgbaString(c, alpha) {
        return "rgba(" + Math.round(c.r * 255) + "," + Math.round(c.g * 255) + ","
            + Math.round(c.b * 255) + "," + alpha + ")"
    }

    function relativeLuminance(c) {
        var r = c.r, g = c.g, b = c.b
        r = r <= 0.03928 ? r / 12.92 : Math.pow((r + 0.055) / 1.055, 2.4)
        g = g <= 0.03928 ? g / 12.92 : Math.pow((g + 0.055) / 1.055, 2.4)
        b = b <= 0.03928 ? b / 12.92 : Math.pow((b + 0.055) / 1.055, 2.4)
        return 0.2126 * r + 0.7152 * g + 0.0722 * b
    }

    function contrastRatio(a, b) {
        var la = relativeLuminance(a)
        var lb = relativeLuminance(b)
        if (la < lb) {
            var swap = la
            la = lb
            lb = swap
        }
        return (la + 0.05) / (lb + 0.05)
    }

    function readableColorHex(value, background) {
        var raw = ("" + value).trim()
        var c
        try {
            c = Qt.darker(raw, 1.0)
        } catch (e) {
            return raw
        }
        if (!c || isNaN(c.r) || c.a < 0.5)
            return raw

        var onDark = relativeLuminance(background) < 0.35
        var adjusted = c
        for (var i = 0; i < 14 && contrastRatio(adjusted, background) < 4.5; ++i)
            adjusted = onDark ? Qt.lighter(adjusted, 1.18) : Qt.darker(adjusted, 1.18)
        if (contrastRatio(adjusted, background) < 3.0)
            return dialogRoot.colorToHex(dialogRoot.textColor)
        return dialogRoot.colorToHex(adjusted)
    }

    function bbcodeToHtml(text, maxImgWidth) {
        if (!text || text.length === 0) return ""

        var h = String(text)

        h = h.replace(/[\ufeff\u200b]/g, "").replace(/\u00a0/g, " ")
        h = h.replace(/\r\n/g, "\n").replace(/\r/g, "\n")
        h = h.replace(/[ \t]*\n?[ \t]*<br\s*\/?>[ \t]*\n?[ \t]*/gi, "\n")

        var noparseSegments = []
        h = h.replace(/\[noparse\]([\s\S]*?)\[\/noparse\]/gi, function(match, content) {
            noparseSegments.push(content.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;"))
            return "\u0001NOPARSE" + (noparseSegments.length - 1) + "\u0002"
        })

        h = h.replace(/\[url=([^\]]+)\]/gi, function(match, url) {
            return '[url=' + url.replace(/&amp;/g, '&') + ']'
        })
        h = h.replace(/\[img\]([\s\S]*?)\[\/img\]/gi, function(match, url) {
            return '[img]' + url.trim().replace(/&amp;/g, '&') + '[/img]'
        })

        var parseList = function(match, param, content) {
            var isOl = false
            var olType = ""
            if (param) {
                var val = param.replace(/^=/, "").trim()
                if (val === "1" || val.toLowerCase() === "decimal") {
                    isOl = true
                } else if (val.toLowerCase() === "a" || val.toLowerCase() === "lower-alpha") {
                    isOl = true
                    olType = ' type="a"'
                } else if (val === "A" || val.toLowerCase() === "upper-alpha") {
                    isOl = true
                    olType = ' type="A"'
                } else if (val.toLowerCase() === "i" || val.toLowerCase() === "lower-roman") {
                    isOl = true
                    olType = ' type="i"'
                } else if (val === "I" || val.toLowerCase() === "upper-roman") {
                    isOl = true
                    olType = ' type="I"'
                }
            }

            content = content.replace(/\[\/\*\]/gi, "")
            var parts = content.split(/\[\*\]/i)
            var itemsHtml = []

            if (parts[0] && parts[0].trim().length > 0) {
                var prefix = parts[0].trim().replace(/^\n+|\n+$/g, "")
                if (prefix.length > 0) {
                    itemsHtml.push(prefix)
                }
            }

            for (var i = 1; i < parts.length; i++) {
                var item = parts[i].trim()
                item = item.replace(/^\n+|\n+$/g, "")
                item = item.replace(/\n+/g, "<br>")
                if (item.length > 0) {
                    itemsHtml.push("<li>" + item + "</li>")
                }
            }

            var inner = itemsHtml.join("")
            if (isOl) {
                return '<ol' + olType + ' style="margin: 6px 0; padding-left: 24px; line-height: 1.5;">' + inner + '</ol>'
            } else {
                return '<ul style="margin: 6px 0; padding-left: 20px; line-height: 1.5;">' + inner + '</ul>'
            }
        }

        h = h.replace(/\[olist(?:\s[^\]]*)?\]/gi, '[list=1]').replace(/\[\/olist\]/gi, '[/list]')
        h = h.replace(/\[ul(?:\s[^\]]*)?\]/gi, '[list]').replace(/\[\/ul\]/gi, '[/list]')
        h = h.replace(/\[ol(?:\s[^\]]*)?\]/gi, '[list=1]').replace(/\[\/ol\]/gi, '[/list]')
        h = h.replace(/\[li\]/gi, '[*]')

        var prevH = ""
        var loopCount = 0
        while (prevH !== h && /\[list/i.test(h) && loopCount < 10) {
            prevH = h
            loopCount++
            h = h.replace(/\[list(=[^\]]*)?\]((?:(?!\[list[\s=\]])[\s\S])*?)\[\/list\]/gi, parseList)
        }

        h = h.replace(/\[b\]/gi, '<b>').replace(/\[\/b\]/gi, '</b>')
        h = h.replace(/\[i\]/gi, '<i>').replace(/\[\/i\]/gi, '</i>')
        h = h.replace(/\[u\]/gi, '<u>').replace(/\[\/u\]/gi, '</u>')
        h = h.replace(/\[(?:s|strike|del)\]/gi, '<s>').replace(/\[\/(?:s|strike|del)\]/gi, '</s>')
        h = h.replace(/\[sub\]/gi, '<sub>').replace(/\[\/sub\]/gi, '</sub>')
        h = h.replace(/\[sup\]/gi, '<sup>').replace(/\[\/sup\]/gi, '</sup>')

        var sizeMap = {
            "1": "10px",
            "2": "12px",
            "3": "13px",
            "4": "16px",
            "5": "18px",
            "6": "22px",
            "7": "26px"
        }
        h = h.replace(/\[size=([^\]]+)\]/gi, function(match, p1) {
            var sz = p1.trim()
            var px = sizeMap[sz] || (sz.indexOf("px") !== -1 || sz.indexOf("pt") !== -1 ? sz : (sz.match(/^\d+$/) ? sz + "px" : "14px"))
            return '<span style="font-size: ' + px + '; font-weight: 600;">'
        })
        h = h.replace(/\[\/size\]/gi, '</span>')

        h = h.replace(/\[color=([^\]]+)\]/gi, function(match, value) {
            return '<font color="' + dialogRoot.readableColorHex(value, dialogRoot.surfaceColor) + '">'
        }).replace(/\[\/color\]/gi, '</font>')
        h = h.replace(/\[font=([^\]]+)\]/gi, '<font face="$1">').replace(/\[\/font\]/gi, '</font>')

        var headingColor = dialogRoot.colorToHex(dialogRoot.textColor)
        h = h.replace(/\[(?:heading|h1)\]([\s\S]*?)\[\/(?:heading|h1)\]/gi, '<h2 style="font-size: 18px; font-weight: bold; margin: 12px 0 6px 0; color: ' + headingColor + ';">$1</h2>')
        h = h.replace(/\[h2\]([\s\S]*?)\[\/h2\]/gi, '<h3 style="font-size: 16px; font-weight: bold; margin: 10px 0 4px 0; color: ' + headingColor + ';">$1</h3>')
        h = h.replace(/\[h3\]([\s\S]*?)\[\/h3\]/gi, '<h4 style="font-size: 14px; font-weight: bold; margin: 8px 0 4px 0; color: ' + headingColor + ';">$1</h4>')

        h = h.replace(/\[center\]/gi, '<div align="center">').replace(/\[\/center\]/gi, '</div>')
        h = h.replace(/\[left\]/gi, '<div align="left">').replace(/\[\/left\]/gi, '</div>')
        h = h.replace(/\[right\]/gi, '<div align="right">').replace(/\[\/right\]/gi, '</div>')
        h = h.replace(/\[justify\]/gi, '<div align="justify">').replace(/\[\/justify\]/gi, '</div>')

        var imageBlockHtml = function(rawUrl, sizeParam, href) {
            var trimmed = rawUrl.trim()

            if (/\.avif(?:[?#]|$)/i.test(trimmed)) {
                var avifIndex = dialogRoot.avifBlockDefs.length
                dialogRoot.avifBlockDefs.push({
                    url: trimmed.replace(/&amp;/g, "&"),
                    href: href
                })
                return '\u0006AV' + avifIndex + '\u0006'
            }

            var srcUrl = dialogRoot.badgePngUrl(trimmed)
            var maxW = (maxImgWidth && maxImgWidth > 100) ? Math.round(maxImgWidth) : 960
            var isBadge = trimmed.indexOf("shields.io") !== -1
                || trimmed.indexOf("badge") !== -1 || trimmed.indexOf("button") !== -1
            if (isBadge && href.length === 0)
                return '<img src="' + srcUrl + '">'
            if (isBadge)
                return '<div align="center" style="margin: 12px 0;">&nbsp;<a href="' + href
                    + '"><img src="' + srcUrl + '"></a></div>'
            var dim = (sizeParam || "").trim().match(/^(\d+)\s*(?:[x×]\s*(\d+))?$/i)
            var w
            var hgt = 0
            if (dim) {
                w = parseInt(dim[1], 10)
                hgt = dim[2] ? parseInt(dim[2], 10) : 0
                if (w > maxW) {
                    w = maxW
                    hgt = 0
                }
            } else {

                var naturalW = dialogRoot.imageWidths[trimmed] || 0
                w = (naturalW > 0 && naturalW < maxW) ? Math.round(naturalW) : maxW
            }
            var inner = '<img src="' + srcUrl + '" width="' + w + '"'
                + (hgt > 0 ? ' height="' + hgt + '"' : '') + '>'
            var link = href.length > 0 ? href : ("preview:" + trimmed)

            var spacer = (w < maxW - 8) ? "&nbsp;" : ""
            return '<div align="center" style="margin: 12px 0;">' + spacer + '<a href="' + link + '">' + inner + '</a></div>'
        }

        var linkedImages = []
        h = h.replace(/\[url=([^\]]+)\]\s*\[img(?:=([^\]]*))?\]([\s\S]*?)\[\/img\]\s*\[\/url\]/gi,
            function(match, linkUrl, sizeParam, url) {
                linkedImages.push(imageBlockHtml(url, sizeParam || "", linkUrl.trim()))
                return '\u0003LINKEDIMG' + (linkedImages.length - 1) + '\u0004'
            })

        h = h.replace(/\[img=([^\]]*)\]([\s\S]*?)\[\/img\]/gi, function(match, sizeParam, url) {
            return imageBlockHtml(url, sizeParam, "")
        })

        h = h.replace(/\[img\]([\s\S]*?)\[\/img\]/gi, function(match, url) {
            return imageBlockHtml(url, "", "")
        })

        h = h.replace(/\[url=([^\]]+)\]([\s\S]*?)\[\/url\]/gi, '<a href="$1">$2</a>')
        h = h.replace(/\[url\]([\s\S]*?)\[\/url\]/gi, '<a href="$1">$1</a>')
        h = h.replace(/\[email=([^\]]+)\]([\s\S]*?)\[\/email\]/gi, '<a href="mailto:$1">$2</a>')
        h = h.replace(/\[email\]([\s\S]*?)\[\/email\]/gi, '<a href="mailto:$1">$1</a>')

        h = h.replace(/\u0003LINKEDIMG(\d+)\u0004/g, function(match, idx) {
            return linkedImages[parseInt(idx, 10)] || ""
        })

        h = h.replace(/\[youtube\]([\s\S]*?)\[\/youtube\]/gi, function(match, raw) {
            var trimmed = raw.trim()
            var vId = trimmed
            var m = trimmed.match(/(?:v=|\/embed\/|\.be\/)([a-zA-Z0-9_\-]+)/)
            if (m) vId = m[1]
            if (!/^[a-zA-Z0-9_\-]+$/.test(vId)) return ""
            return '\u0006YT' + vId + '\u0006'
        })

        var tableCellAttrs = function(params) {
            var extra = ""
            var mc = params.match(/colspan\s*=\s*"?(\d+)/i)
            if (mc) extra += ' colspan="' + mc[1] + '"'
            var mr = params.match(/rowspan\s*=\s*"?(\d+)/i)
            if (mr) extra += ' rowspan="' + mr[1] + '"'
            var ma = params.match(/align\s*=\s*"?([a-z]+)/i)
            if (ma) extra += ' align="' + ma[1] + '"'
            var mw = params.match(/width\s*=\s*"?([0-9]+%?)/i)
            if (mw) extra += ' width="' + mw[1] + '"'
            return extra
        }

        var tableBorder = dialogRoot.rgbaString(dialogRoot.textColor, 0.15)
        var tableHeaderBg = dialogRoot.rgbaString(dialogRoot.textColor, 0.08)
        h = h.replace(/\[table([^\]]*)\]/gi, function(match, params) {
            var extra = tableCellAttrs(params)
            var bare = params.match(/=\s*([0-9]+%?)/)
            if (bare && !/width\s*=/i.test(params))
                extra += ' width="' + bare[1] + '"'
            return '<table' + extra + ' style="border-collapse: collapse; margin: 8px 0;'
                + (extra.indexOf("width=") === -1 ? ' width: 100%;' : '')
                + ' border: 1px solid ' + tableBorder + ';">'
        }).replace(/\[\/table\]/gi, '</table>')
        h = h.replace(/\[tr([^\]]*)\]/gi, function(match, params) {
            return '<tr' + tableCellAttrs(params) + '>'
        }).replace(/\[\/tr\]/gi, '</tr>')
        h = h.replace(/\[th([^\]]*)\]/gi, function(match, params) {
            return '<th' + tableCellAttrs(params) + ' style="padding: 6px 10px; background: ' + tableHeaderBg + '; border: 1px solid ' + tableBorder + '; font-weight: bold;">'
        }).replace(/\[\/th\]/gi, '</th>')
        h = h.replace(/\[td([^\]]*)\]/gi, function(match, params) {
            return '<td' + tableCellAttrs(params) + ' style="padding: 6px 10px; border: 1px solid ' + tableBorder + ';">'
        }).replace(/\[\/td\]/gi, '</td>')

        var codeStyle = 'background: ' + dialogRoot.rgbaString(dialogRoot.textColor, 0.06)
            + '; padding: 8px 12px; border-radius: 4px; font-family: monospace; border: 1px solid '
            + dialogRoot.rgbaString(dialogRoot.textColor, 0.15) + ';'
        h = h.replace(/\[code(?:=[^\]]*)?\]([\s\S]*?)\[\/code\]/gi, '<pre style="' + codeStyle + '">$1</pre>')
        h = h.replace(/\[pre\]([\s\S]*?)\[\/pre\]/gi, '<pre style="' + codeStyle + '">$1</pre>')
        h = h.replace(/\[indent\]/gi, '<div style="margin-left: 24px;">').replace(/\[\/indent\]/gi, '</div>')

        var accent = dialogRoot.readableColorHex("#c27ba0", dialogRoot.surfaceColor)
        h = h.replace(/\[quote(?:=([^\]]+))?\]/gi, function(match, author) {
            var header = author ? '<div style="font-size: 11px; font-weight: bold; color: ' + accent + '; margin-bottom: 4px;">Quote (' + author.trim() + '):</div>' : ''
            return '<blockquote style="border-left: 3px solid ' + accent + '; padding-left: 10px; margin: 8px 0; color: ' + dialogRoot.rgbaString(dialogRoot.textColor, 0.85) + ';">' + header
        })
        h = h.replace(/\[\/quote\]/gi, '</blockquote>')

        var spoilerStyle = 'background: ' + dialogRoot.rgbaString(dialogRoot.textColor, 0.04)
            + '; padding: 10px 14px; border-radius: 6px; margin: 8px 0; border: 1px solid '
            + dialogRoot.rgbaString(dialogRoot.textColor, 0.1) + ';'
        var spoilerLabel = function(title) {
            var t = title ? title.trim().replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;") : ""
            return t.length > 0 ? ('[SPOILER: ' + t + ']') : '[SPOILER]'
        }
        var spoilerSeq = 0
        var renderSpoilers = function(src) {
            var out = ""
            var openRe = /\[spoiler(?:=([^\]]*))?\]/gi
            var lastEnd = 0
            var m
            while ((m = openRe.exec(src)) !== null) {
                var index = spoilerSeq++
                var expanded = dialogRoot.isSpoilerExpanded(index)
                var depth = 1
                var scanRe = /\[spoiler(?:=[^\]]*)?\]|\[\/spoiler\]/gi
                scanRe.lastIndex = openRe.lastIndex
                var closeStart = -1
                var closeEnd = src.length
                var sm
                while ((sm = scanRe.exec(src)) !== null) {
                    if (sm[0].charAt(1) === '/') {
                        if (--depth === 0) {
                            closeStart = sm.index
                            closeEnd = scanRe.lastIndex
                            break
                        }
                    } else {
                        ++depth
                    }
                }
                var innerEnd = closeStart === -1 ? src.length : closeStart
                var inner = renderSpoilers(src.substring(openRe.lastIndex, innerEnd))
                out += src.substring(lastEnd, m.index)
                    + '<div style="' + spoilerStyle + '">'
                    + '<a href="spoiler:' + index + '" style="color: ' + accent
                    + '; font-weight: bold; font-size: 11px; text-decoration: none;">'
                    + (expanded ? '▾ ' : '▸ ') + spoilerLabel(m[1]) + '</a>'
                if (expanded)
                    out += '<br>' + inner
                out += '</div>'
                lastEnd = closeEnd
                openRe.lastIndex = closeEnd
            }
            return out + src.substring(lastEnd)
        }
        h = renderSpoilers(h)

        h = h.replace(/\[(?:hr|line)\]/gi, '<hr style="border: 0; border-top: 1px solid ' + dialogRoot.rgbaString(dialogRoot.textColor, 0.15) + '; margin: 12px 0;">')
        h = h.replace(/\[br\s*\/?\]/gi, '<br>')

        h = h.replace(/\n\s*(<br\s*\/?>)/gi, '$1')
        h = h.replace(/(<br\s*\/?>)\s*\n/gi, '$1')
        h = h.replace(/\n{3,}/g, '\n\n')
        h = h.replace(/\n/g, '<br>')

        h = h.replace(/(<(?:ul|ol|div|blockquote|pre|table|h2|h3|h4)[^>]*>)(?:\s*<br\s*\/?>)+/gi, '$1')
        h = h.replace(/(?:\s*<br\s*\/?>)+(<\/(?:ul|ol|div|blockquote|pre|table|h2|h3|h4)>)/gi, '$1')
        h = h.replace(/(<\/(?:ul|ol|div|blockquote|pre|table|h2|h3|h4)>)(?:\s*<br\s*\/?>)+/gi, '$1')
        h = h.replace(/(?:\s*<br\s*\/?>)+(<(?:ul|ol|div|blockquote|pre|table|h2|h3|h4)[^>]*>)/gi, '$1')

        h = h.replace(/<img\b[^>]*\bsrc\s*=\s*["'][^"']*\.avif(?:[?#][^"']*)?["'][^>]*>/gi, "")

        h = h.replace(/\u0001NOPARSE(\d+)\u0002/g, function(match, idx) {
            return noparseSegments[parseInt(idx, 10)] || ""
        })

        return h
    }

    function splitDescriptionHtml(html, maxChars) {
        if (!html || html.length <= maxChars)
            return [html]

        var knownTags = {
            "b": true, "i": true, "u": true, "s": true, "sub": true, "sup": true,
            "a": true, "span": true, "font": true, "em": true, "strong": true,
            "p": true, "div": true, "ul": true, "ol": true, "li": true,
            "blockquote": true, "h1": true, "h2": true, "h3": true, "h4": true,
            "pre": true, "code": true, "table": true, "tr": true, "td": true,
            "th": true, "center": true, "img": true, "br": true, "hr": true
        }
        var blockTags = {
            "p": true, "div": true, "ul": true, "ol": true, "blockquote": true,
            "h1": true, "h2": true, "h3": true, "h4": true, "pre": true,
            "table": true
        }
        var voidTags = { "br": true, "hr": true, "img": true }

        var chunks = []
        var start = 0
        var depth = 0
        var re = /<(\/?)([a-zA-Z][a-zA-Z0-9]*)(?:\s[^>]*)?>/g
        var m
        while ((m = re.exec(html)) !== null) {
            var closing = m[1] === "/"
            var name = m[2].toLowerCase()
            if (!knownTags[name])
                continue

            var isVoid = voidTags[name] === true
            var breaker = false
            if (isVoid) {

                breaker = (depth === 0)
            } else if (closing) {
                depth = Math.max(0, depth - 1)
                breaker = (depth === 0 && blockTags[name] === true)
            } else {
                depth++
            }

            if (breaker && re.lastIndex - start >= maxChars) {
                chunks.push(html.slice(start, re.lastIndex))
                start = re.lastIndex
            }
        }
        if (start < html.length)
            chunks.push(html.slice(start))
        return chunks.length > 0 ? chunks : [html]
    }

    property var imageWidths: ({})
    property var imageProbes: []
    property var probingUrls: ({})
    property bool imageWidthsDirty: false

    Component {
        id: imageProbeComponent

        Image {
            property string imageUrl: ""
            visible: false
            asynchronous: true
            cache: true

            onStatusChanged: {
                if (status === Image.Ready)
                    dialogRoot.noteImageSize(imageUrl, implicitWidth)
            }
        }
    }

    Timer {
        id: imageWidthDebounce
        interval: 250
        repeat: false
        onTriggered: {
            if (!dialogRoot.imageWidthsDirty)
                return
            dialogRoot.imageWidthsDirty = false
            dialogRoot.rebuildDescriptionBlocks(true)
        }
    }

    Timer {
        id: viewRebuildTimer
        interval: 180
        repeat: false
        onTriggered: dialogRoot.rebuildDescriptionBlocks(true)
    }

    function noteImageSize(url, width) {
        if (!width || width <= 0)
            return
        var roundedW = Math.round(width)
        if (imageWidths[url] === roundedW)
            return
        var nextWidths = {}
        for (var key in imageWidths)
            nextWidths[key] = imageWidths[key]
        nextWidths[url] = roundedW
        imageWidths = nextWidths
        imageWidthsDirty = true
        imageWidthDebounce.restart()
    }

    function probeDescriptionImages() {
        var d = (modManager.currentNexusModDetails && modManager.currentNexusModDetails.description) || ""
        if (d.length === 0)
            return
        var re = /\[img\]([\s\S]*?)\[\/img\]/gi
        var m
        while ((m = re.exec(d)) !== null) {
            var url = m[1].trim().replace(/&amp;/g, "&")
            if (url.length === 0 || /\.avif(?:[?#]|$)/i.test(url)
                || dialogRoot.probingUrls[url] || imageWidths[url] !== undefined)
                continue
            dialogRoot.probingUrls[url] = true
            var probe = imageProbeComponent.createObject(dialogRoot,
                { imageUrl: url, source: url })
            if (probe)
                imageProbes.push(probe)
        }
    }

    property var descriptionBlocks: []
    property string lastDescriptionSource: ""
    property var avifBlockDefs: []

    function rebuildDescriptionBlocks(force) {
        var d = (modManager.currentNexusModDetails && modManager.currentNexusModDetails.description) || ""
        if (!force && d === lastDescriptionSource && descriptionBlocks.length > 0)
            return
        lastDescriptionSource = d

        var linkColor = dialogRoot.readableColorHex(dialogRoot.primaryColor, dialogRoot.surfaceColor)
        var style = "<style> a { color: " + linkColor
            + "; text-decoration: underline; } p { margin: 8px 0; line-height: 1.4; }</style>"
        if (d.length === 0) {
            descriptionBlocks = [{ kind: "text", html: style + "<em>Loading full mod documentation and description from Nexus...</em>" }]
            return
        }

        var availW = Math.max(200, Math.min(dialogRoot.contentMaxWidth,
            dialogRoot.descriptionCardWidth - 36))
        dialogRoot.avifBlockDefs = []
        dialogRoot.prepareAvifImages(d)
        var html = dialogRoot.bbcodeToHtml(d, availW)
        var blocks = []
        var lastIndex = 0
        var markerRe = /\u0006(?:YT([A-Za-z0-9_-]+)|AV(\d+))\u0006/g
        var m
        while ((m = markerRe.exec(html)) !== null) {
            dialogRoot.appendTextBlocks(blocks, html.substring(lastIndex, m.index), style)
            if (m[1] !== undefined) {
                blocks.push({ kind: "video", videoId: m[1] })
            } else {
                var def = dialogRoot.avifBlockDefs[parseInt(m[2], 10)] || {}
                var info = animatedImageStore.infoFor(def.url || "")
                blocks.push({
                    kind: "avif",
                    url: def.url || "",
                    href: def.href || "",
                    width: info.width || 1000,
                    height: info.height || 562
                })
            }
            lastIndex = markerRe.lastIndex
        }
        dialogRoot.appendTextBlocks(blocks, html.substring(lastIndex), style)
        descriptionBlocks = blocks

        dialogRoot.probeDescriptionImages()
    }

    function appendTextBlocks(blocks, html, style) {
        if (html.length === 0)
            return
        var chunks = dialogRoot.splitDescriptionHtml(html, 1500)
        for (var i = 0; i < chunks.length; ++i) {
            if (chunks[i].length === 0)
                continue
            blocks.push({ kind: "text", html: style + chunks[i] })
        }
    }

    function prepareAvifImages(description) {
        var re = /\[img(?:=[^\]]*)?\]([\s\S]*?)\[\/img\]/gi
        var m
        while ((m = re.exec(description)) !== null) {
            var url = m[1].trim().replace(/&amp;/g, "&")
            if (/\.avif(?:[?#]|$)/i.test(url))
                animatedImageStore.request(url)
        }
    }

    Connections {
        target: animatedImageStore
        function onConverted(url) {
            viewRebuildTimer.restart()
        }
    }

    function latestFileVersion() {
        var files = modManager.currentNexusModFiles
        var bestVersion = ""
        var bestTimestamp = -1
        for (var i = 0; i < files.length; ++i) {
            var file = files[i]
            var version = (file.version || "").toString().trim()
            if (version.length === 0)
                continue
            var timestamp = Number(file.uploadedTimestamp) || 0
            if (timestamp >= bestTimestamp) {
                bestTimestamp = timestamp
                bestVersion = version
            }
        }
        return bestVersion
    }

    function formatStatCount(value) {
        var n = Number(value)
        if (!isFinite(n) || n < 0)
            return "0"
        return Math.round(n).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",")
    }

    function badgePngUrl(url) {
        if (url.indexOf("img.shields.io") === -1)
            return url
        if (/\.(?:png|svg|json)(?:[?#]|$)/i.test(url))
            return url
        var q = url.indexOf("?")
        return q === -1 ? url + ".png" : url.substring(0, q) + ".png" + url.substring(q)
    }

    function openVideo(videoId) {
        if (!videoId)
            return
        videoWindow.openVideo(videoId)
    }

    function openAvif(url) {
        if (!url)
            return
        avifWindow.openAvif(url)
    }

    property var spoilerExpanded: ({})
    function isSpoilerExpanded(index) {
        return dialogRoot.spoilerExpanded[index] === true
    }
    function toggleSpoiler(index) {
        if (isNaN(index))
            return
        var next = {}
        for (var key in dialogRoot.spoilerExpanded)
            next[key] = dialogRoot.spoilerExpanded[key]
        next[index] = !next[index]
        dialogRoot.spoilerExpanded = next

        Qt.callLater(dialogRoot.rebuildDescriptionBlocks, true)
    }

    function handleDescriptionLink(link) {
        if (!link)
            return
        if (link.indexOf("preview:") === 0) {
            dialogRoot.openDescriptionImage(link.substring(8))
            return
        }
        if (link.indexOf("spoiler:") === 0) {
            dialogRoot.toggleSpoiler(parseInt(link.substring(8), 10))
            return
        }

        if (/^(https?:\/\/|mailto:)/i.test(link))
            Qt.openUrlExternally(link)
    }

    function descriptionImageUrls() {
        var d = (modManager.currentNexusModDetails && modManager.currentNexusModDetails.description) || ""
        d = d.replace(/\[url=[^\]]+\]\s*\[img(?:=[^\]]*)?\][\s\S]*?\[\/img\]\s*\[\/url\]/gi, "")
        var urls = []
        var re = /\[img(?:=[^\]]*)?\]([\s\S]*?)\[\/img\]/gi
        var m
        while ((m = re.exec(d)) !== null) {
            var url = m[1].trim().replace(/&amp;/g, "&")
            if (url.length === 0 || /\.avif(?:[?#]|$)/i.test(url))
                continue
            if (url.indexOf("shields.io") !== -1 || url.indexOf("badge") !== -1
                || url.indexOf("button") !== -1)
                continue
            if (urls.indexOf(url) === -1)
                urls.push(url)
        }
        return urls
    }

    function openDescriptionImage(url) {
        var target = (url || "").trim().replace(/&amp;/g, "&")
        var urls = descriptionImageUrls()
        var index = urls.indexOf(target)
        if (index < 0)
            return
        var list = []
        for (var i = 0; i < urls.length; ++i)
            list.push({ fullUrl: urls[i], thumbUrl: urls[i], caption: "" })
        previewViewer.openImages(list, index)
    }

    Connections {
        target: modManager
        function onCurrentNexusModDetailsChanged() { dialogRoot.rebuildDescriptionBlocks() }
    }

    function open(data) {
        if (!data || !data.modId) return
        previewViewer.closeViewer()
        videoWindow.close()
        avifWindow.close()

        for (var i = 0; i < imageProbes.length; ++i)
            imageProbes[i].destroy()
        imageProbes = []
        probingUrls = ({})
        spoilerExpanded = ({})
        modsUsingTab.resetFilter()

        modData = data
        currentTab = 0
        modManager.openNexusModDetails(data.modId, data)
        descriptionTab.resetScroll()
        parkedInCorner = false
        visible = true
        showMaximized()
        raise()
        requestActivate()
    }

    function closeDetails() {
        previewViewer.closeViewer()
        videoWindow.close()
        avifWindow.close()
        parkedInCorner = false
        visible = false
        modManager.closeNexusModDetails()
    }

    ThemedBackground {
        anchors.fill: parent
        cozyColor: dialogRoot.surfaceColor
        hostWindow: dialogRoot
    }

    Rectangle {
        id: fullPageContainer
        anchors.fill: parent
        color: (modManager.theme === "Crimson" || modManager.theme === "Glassmorphic"
            || modManager.theme === "Wavey") ? "transparent" : dialogRoot.surfaceColor

        MouseArea {
            anchors.fill: parent
            onClicked: {}
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 50
                color: dialogRoot.panelColor(Qt.rgba(dialogRoot.surfaceColor.r, dialogRoot.surfaceColor.g, dialogRoot.surfaceColor.b,
                    dialogRoot.wallpaperTheme ? 0.18 : 0.85))
                border.width: 1
                border.color: Qt.rgba(dialogRoot.textColor.r, dialogRoot.textColor.g, dialogRoot.textColor.b, 0.08)

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    spacing: 14

                    Rectangle {
                        height: 34
                        width: backBtnRow.implicitWidth + 24
                        radius: 8
                        color: backMouse.containsMouse
                            ? Qt.rgba(dialogRoot.primaryColor.r, dialogRoot.primaryColor.g, dialogRoot.primaryColor.b, 0.22)
                            : Qt.rgba(dialogRoot.textColor.r, dialogRoot.textColor.g, dialogRoot.textColor.b, 0.06)
                        border.width: 1
                        border.color: backMouse.containsMouse
                            ? dialogRoot.primaryColor
                            : Qt.rgba(dialogRoot.textColor.r, dialogRoot.textColor.g, dialogRoot.textColor.b, 0.15)

                        Behavior on color { ColorAnimation { duration: 120 } }

                        RowLayout {
                            id: backBtnRow
                            anchors.centerIn: parent
                            spacing: 8
                            Text {
                                text: "←"
                                font.pixelSize: 14
                                font.weight: Font.Bold
                                color: dialogRoot.primaryColor
                            }
                            Text {
                                text: "Back to Browse"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                color: dialogRoot.textColor
                            }
                        }

                        MouseArea {
                            id: backMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: dialogRoot.closeDetails()
                        }
                    }

                    Text {
                        text: {
                            var domain = (dialogRoot.modData.gameDomain || modManager.nexusGameDomain || "STELLAR BLADE").toUpperCase()
                            var cat = (modManager.currentNexusModDetails && modManager.currentNexusModDetails.category) || dialogRoot.modData.category || "General"
                            return "GAMES / " + domain + " / MODS / " + cat.toUpperCase()
                        }
                        font.pixelSize: 11
                        font.weight: Font.Bold
                        color: Qt.rgba(dialogRoot.textColor.r, dialogRoot.textColor.g, dialogRoot.textColor.b, 0.5)
                    }

                    Item { Layout.fillWidth: true }

                    Rectangle {
                        height: 32
                        width: openWebRow.implicitWidth + 22
                        radius: 6
                        color: openWebMouse.containsMouse ? Qt.rgba(dialogRoot.primaryColor.r, dialogRoot.primaryColor.g, dialogRoot.primaryColor.b, 0.2) : Qt.rgba(dialogRoot.textColor.r, dialogRoot.textColor.g, dialogRoot.textColor.b, 0.06)
                        border.width: 1
                        border.color: Qt.rgba(dialogRoot.textColor.r, dialogRoot.textColor.g, dialogRoot.textColor.b, 0.15)

                        RowLayout {
                            id: openWebRow
                            anchors.centerIn: parent
                            spacing: 6
                            Text {
                                text: "Nexus Mods ↗"
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                                color: dialogRoot.textColor
                            }
                        }

                        MouseArea {
                            id: openWebMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                var domain = dialogRoot.modData.gameDomain || modManager.nexusGameDomain || "stellarblade"
                                var url = "https://www.nexusmods.com/" + domain + "/mods/" + dialogRoot.modData.modId
                                modManager.openNexusUrl(url)
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: headerCol.implicitHeight + 48
                color: dialogRoot.panelColor(Qt.rgba(dialogRoot.surfaceColor.r, dialogRoot.surfaceColor.g, dialogRoot.surfaceColor.b,
                    dialogRoot.wallpaperTheme ? 0.15 : 0.7))
                border.width: 1
                border.color: Qt.rgba(dialogRoot.textColor.r, dialogRoot.textColor.g, dialogRoot.textColor.b, 0.08)

                ColumnLayout {
                    id: headerCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 24
                    spacing: 14

                    Text {
                        Layout.fillWidth: true
                        text: (modManager.currentNexusModDetails && modManager.currentNexusModDetails.name)
                            ? modManager.currentNexusModDetails.name
                            : (dialogRoot.modData.name || "Mod Details")
                        font.pixelSize: 24
                        font.weight: Font.Bold
                        color: dialogRoot.textColor
                        elide: Text.ElideRight
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Rectangle {
                            Layout.preferredHeight: 24
                            Layout.preferredWidth: verText.implicitWidth + 14
                            radius: 4
                            color: Qt.rgba(dialogRoot.textColor.r, dialogRoot.textColor.g, dialogRoot.textColor.b, 0.07)
                            border.width: 1
                            border.color: Qt.rgba(dialogRoot.textColor.r, dialogRoot.textColor.g, dialogRoot.textColor.b, 0.15)

                            Text {
                                id: verText
                                anchors.centerIn: parent
                                text: {
                                    var v = dialogRoot.latestFileVersion()
                                        || (modManager.currentNexusModDetails && modManager.currentNexusModDetails.version)
                                        || dialogRoot.modData.version || "1.0"
                                    v = String(v).trim()
                                    return (v.startsWith("v") || v.startsWith("V")) ? v : ("v" + v)
                                }
                                font.pixelSize: 11
                                font.weight: Font.Bold
                                color: dialogRoot.textColor
                            }
                        }

                        Rectangle {
                            Layout.preferredHeight: 24
                            Layout.preferredWidth: endRow.implicitWidth + 14
                            radius: 4
                            color: Qt.rgba(dialogRoot.textColor.r, dialogRoot.textColor.g, dialogRoot.textColor.b, 0.07)
                            border.width: 1
                            border.color: Qt.rgba(dialogRoot.textColor.r, dialogRoot.textColor.g, dialogRoot.textColor.b, 0.15)

                            Row {
                                id: endRow
                                anchors.centerIn: parent
                                spacing: 4
                                Text {
                                    text: "Endorsements:"
                                    font.pixelSize: 11
                                    color: Qt.rgba(dialogRoot.textColor.r, dialogRoot.textColor.g, dialogRoot.textColor.b, 0.55)
                                }
                                Text {
                                    text: {
                                        var count = (modManager.currentNexusModDetails && modManager.currentNexusModDetails.endorsements !== undefined)
                                            ? modManager.currentNexusModDetails.endorsements
                                            : (dialogRoot.modData.endorsements || 0)
                                        return dialogRoot.formatStatCount(count)
                                    }
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                    color: dialogRoot.textColor
                                }
                            }
                        }

                        Rectangle {
                            Layout.preferredHeight: 24
                            Layout.preferredWidth: dlRow.implicitWidth + 14
                            radius: 4
                            color: Qt.rgba(dialogRoot.textColor.r, dialogRoot.textColor.g, dialogRoot.textColor.b, 0.07)
                            border.width: 1
                            border.color: Qt.rgba(dialogRoot.textColor.r, dialogRoot.textColor.g, dialogRoot.textColor.b, 0.15)

                            Row {
                                id: dlRow
                                anchors.centerIn: parent
                                spacing: 4
                                Text {
                                    text: "Downloads:"
                                    font.pixelSize: 11
                                    color: Qt.rgba(dialogRoot.textColor.r, dialogRoot.textColor.g, dialogRoot.textColor.b, 0.55)
                                }
                                Text {
                                    text: {
                                        var count = (modManager.currentNexusModDetails && modManager.currentNexusModDetails.downloads !== undefined)
                                            ? modManager.currentNexusModDetails.downloads
                                            : (dialogRoot.modData.downloads || 0)
                                        return dialogRoot.formatStatCount(count)
                                    }
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                    color: dialogRoot.textColor
                                }
                            }
                        }

                        Rectangle {
                            visible: !!((modManager.currentNexusModDetails && modManager.currentNexusModDetails.containsAdult) || dialogRoot.modData.adultContent)
                            Layout.preferredHeight: 24
                            Layout.preferredWidth: 64
                            radius: 4
                            color: Qt.rgba(1, 0.2, 0.2, 0.15)
                            border.width: 1
                            border.color: Qt.rgba(1, 0.2, 0.2, 0.45)

                            Text {
                                anchors.centerIn: parent
                                text: "18+ Adult"
                                font.pixelSize: 10
                                font.weight: Font.Bold
                                color: "#E5484D"
                            }
                        }

                        Item { Layout.fillWidth: true }
                    }

                    Item {
                        id: previewStrip
                        Layout.fillWidth: true
                        Layout.preferredHeight: 112
                        visible: dialogRoot.previewImages.length > 0 || dialogRoot.previewSkeletonVisible
                        clip: true

                        property bool canScroll: previewList.contentWidth > previewList.width + 1

                        function scrollBy(direction) {
                            var maxX = Math.max(0, previewList.contentWidth - previewList.width)
                            var target = Math.max(0, Math.min(maxX, previewList.contentX + direction * previewList.width * 0.85))
                            scrollAnim.stop()
                            scrollAnim.from = previewList.contentX
                            scrollAnim.to = target
                            scrollAnim.start()
                        }

                        ListView {
                            id: previewList
                            anchors.fill: parent
                            visible: dialogRoot.previewImages.length > 0
                            orientation: ListView.Horizontal
                            spacing: 10
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            model: dialogRoot.previewImages

                            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOff }

                            delegate: Rectangle {
                                required property var modelData
                                required property int index

                                width: 198
                                height: 112
                                radius: 8
                                color: Qt.rgba(dialogRoot.textColor.r, dialogRoot.textColor.g, dialogRoot.textColor.b, 0.06)
                                border.width: 1
                                border.color: previewHover.hovered
                                    ? dialogRoot.primaryColor
                                    : Qt.rgba(dialogRoot.textColor.r, dialogRoot.textColor.g, dialogRoot.textColor.b, 0.12)
                                clip: true

                                Behavior on border.color { ColorAnimation { duration: 110 } }

                                Image {
                                    id: previewThumb
                                    anchors.fill: parent
                                    source: modelData.thumbUrl || ""
                                    asynchronous: true
                                    cache: true
                                    fillMode: Image.PreserveAspectCrop
                                    sourceSize.width: 396
                                    sourceSize.height: 224
                                    opacity: status === Image.Ready ? 1 : 0

                                    layer.enabled: dialogRoot.blurAdultMedia
                                    layer.effect: MultiEffect {
                                        blurEnabled: true
                                        blur: 0.6
                                        blurMax: 48
                                    }

                                    Behavior on opacity { NumberAnimation { duration: 120 } }
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    color: "#000000"
                                    opacity: previewHover.hovered ? 0.22 : 0
                                    Behavior on opacity { NumberAnimation { duration: 100 } }
                                }

                                Rectangle {
                                    width: 26
                                    height: 26
                                    radius: 13
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: 8
                                    color: Qt.rgba(0, 0, 0, 0.55)
                                    opacity: previewHover.hovered ? 1 : 0

                                    Behavior on opacity { NumberAnimation { duration: 100 } }

                                    Text {
                                        anchors.centerIn: parent
                                        text: "⤢"
                                        font.pixelSize: 13
                                        color: "#FFFFFF"
                                    }
                                }

                                MouseArea {
                                    id: previewHover
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: previewViewer.openAt(index)
                                }
                            }
                        }

                        Row {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            spacing: 10
                            visible: dialogRoot.previewSkeletonVisible

                            Repeater {
                                model: 7

                                Rectangle {
                                    width: 198
                                    height: 112
                                    radius: 8
                                    color: Qt.rgba(dialogRoot.textColor.r, dialogRoot.textColor.g, dialogRoot.textColor.b, 0.06)

                                    SequentialAnimation on opacity {
                                        loops: Animation.Infinite
                                        NumberAnimation { to: 0.35; duration: 750; easing.type: Easing.InOutQuad }
                                        NumberAnimation { to: 0.85; duration: 750; easing.type: Easing.InOutQuad }
                                    }
                                }
                            }
                        }

                        Rectangle {
                            visible: previewStrip.canScroll && previewList.contentX > 1
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            width: 30
                            height: 56
                            radius: 8
                            color: leftScrollMouse.containsMouse ? Qt.rgba(0, 0, 0, 0.78) : Qt.rgba(0, 0, 0, 0.55)
                            z: 2

                            Text {
                                anchors.centerIn: parent
                                text: "‹"
                                font.pixelSize: 28
                                color: "#FFFFFF"
                            }

                            MouseArea {
                                id: leftScrollMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: previewStrip.scrollBy(-1)
                            }
                        }

                        Rectangle {
                            visible: previewStrip.canScroll && previewList.contentX < previewList.contentWidth - previewList.width - 1
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            width: 30
                            height: 56
                            radius: 8
                            color: rightScrollMouse.containsMouse ? Qt.rgba(0, 0, 0, 0.78) : Qt.rgba(0, 0, 0, 0.55)
                            z: 2

                            Text {
                                anchors.centerIn: parent
                                text: "›"
                                font.pixelSize: 28
                                color: "#FFFFFF"
                            }

                            MouseArea {
                                id: rightScrollMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: previewStrip.scrollBy(1)
                            }
                        }

                        NumberAnimation {
                            id: scrollAnim
                            target: previewList
                            property: "contentX"
                            duration: 180
                            easing.type: Easing.OutCubic
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                color: dialogRoot.panelColor(Qt.rgba(dialogRoot.surfaceColor.r, dialogRoot.surfaceColor.g, dialogRoot.surfaceColor.b,
                    dialogRoot.wallpaperTheme ? 0.15 : 0.7))
                border.width: 1
                border.color: Qt.rgba(dialogRoot.textColor.r, dialogRoot.textColor.g, dialogRoot.textColor.b, 0.08)

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 28
                    anchors.rightMargin: 28
                    spacing: 8

                    Repeater {
                        model: [
                            { title: "Description", index: 0 },
                            { title: "Requirements", index: 1 },
                            { title: "Mods using this mod", index: 2 },
                            { title: "Files (" + dialogRoot.activeFileCount + ")", index: 3 }
                        ]

                        delegate: Rectangle {
                            required property var modelData
                            Layout.preferredHeight: 34
                            Layout.preferredWidth: tabText.implicitWidth + 28
                            radius: 6
                            color: dialogRoot.currentTab === modelData.index
                                ? Qt.rgba(dialogRoot.primaryColor.r, dialogRoot.primaryColor.g, dialogRoot.primaryColor.b, 0.18)
                                : (tabMouse.containsMouse ? Qt.rgba(dialogRoot.textColor.r, dialogRoot.textColor.g, dialogRoot.textColor.b, 0.06) : "transparent")
                            border.width: 1
                            border.color: dialogRoot.currentTab === modelData.index
                                ? dialogRoot.primaryColor
                                : "transparent"

                            Behavior on color { ColorAnimation { duration: 120 } }

                            Text {
                                id: tabText
                                anchors.centerIn: parent
                                text: modelData.title
                                font.pixelSize: 12
                                font.weight: dialogRoot.currentTab === modelData.index ? Font.DemiBold : Font.Normal
                                color: dialogRoot.currentTab === modelData.index ? dialogRoot.primaryColor : dialogRoot.textColor
                            }

                            MouseArea {
                                id: tabMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: dialogRoot.currentTab = modelData.index
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    RowLayout {
                        spacing: 6
                        visible: modManager.isLoadingNexusModDetails || modManager.isLoadingNexusModFiles

                        BusyIndicator {
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20
                            running: true
                        }
                        Text {
                            text: "Syncing Nexus..."
                            font.pixelSize: 11
                            color: Qt.rgba(dialogRoot.textColor.r, dialogRoot.textColor.g, dialogRoot.textColor.b, 0.5)
                        }
                    }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: dialogRoot.currentTab

                NexusDescriptionTab {
                    id: descriptionTab
                }

                NexusRequirementsTab {
                }

                NexusModsUsingTab {
                    id: modsUsingTab
                }

                NexusFilesTab {
                    id: filesTab
                }
            }
        }
    }

    NexusPreviewViewer {
        id: previewViewer
    }

    NexusVideoWindow {
        id: videoWindow
        dialog: dialogRoot
    }

    NexusAnimatedViewer {
        id: avifWindow
        dialog: dialogRoot
    }
}
