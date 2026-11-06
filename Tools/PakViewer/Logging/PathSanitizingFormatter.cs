using System;
using System.IO;
using System.Text.RegularExpressions;
using Serilog.Events;
using Serilog.Formatting;
using Serilog.Formatting.Display;

namespace PakViewer.Logging;

public class PathSanitizingFormatter : ITextFormatter
{
    private readonly MessageTemplateTextFormatter _innerFormatter;
    private static readonly Regex PathRegex = new Regex(
        @"in [A-Z]:\\[^\r\n:]+:line \d+",
        RegexOptions.Compiled | RegexOptions.IgnoreCase);

    public PathSanitizingFormatter(string outputTemplate)
    {
        _innerFormatter = new MessageTemplateTextFormatter(outputTemplate);
    }

    public void Format(LogEvent logEvent, TextWriter output)
    {

        if (logEvent.Exception != null)
        {
            var sanitizedException = SanitizeException(logEvent.Exception);
            var sanitizedLogEvent = new LogEvent(
                logEvent.Timestamp,
                logEvent.Level,
                sanitizedException,
                logEvent.MessageTemplate,
                logEvent.Properties.Select(kvp => new LogEventProperty(kvp.Key, kvp.Value)));

            _innerFormatter.Format(sanitizedLogEvent, output);
        }
        else
        {
            _innerFormatter.Format(logEvent, output);
        }
    }

    private Exception SanitizeException(Exception exception)
    {
        if (exception == null) return null;

        var sanitizedInner = exception.InnerException != null
            ? SanitizeException(exception.InnerException)
            : null;

        var stackTrace = exception.StackTrace;
        if (!string.IsNullOrEmpty(stackTrace))
        {

            stackTrace = PathRegex.Replace(stackTrace, match =>
            {

                var pathPart = match.Value.Substring(3);
                var colonIndex = pathPart.LastIndexOf(':');
                if (colonIndex > 0)
                {
                    var filePath = pathPart.Substring(0, colonIndex);
                    var fileName = Path.GetFileName(filePath);
                    var lineNumber = pathPart.Substring(colonIndex);
                    return $"in {fileName}{lineNumber}";
                }
                return match.Value;
            });

            var sanitizedException = new SanitizedException(exception.Message, sanitizedInner)
            {
                SanitizedStackTrace = stackTrace,
                OriginalType = exception.GetType().FullName
            };

            return sanitizedException;
        }

        if (sanitizedInner != null)
        {
            return new SanitizedException(exception.Message, sanitizedInner)
            {
                OriginalType = exception.GetType().FullName
            };
        }

        return exception;
    }

    private class SanitizedException : Exception
    {
        public string SanitizedStackTrace { get; set; }
        public string OriginalType { get; set; }

        public SanitizedException(string message, Exception innerException)
            : base(message, innerException)
        {
        }

        public override string StackTrace => SanitizedStackTrace ?? base.StackTrace;

        public override string ToString()
        {
            var result = $"{OriginalType}: {Message}";
            if (!string.IsNullOrEmpty(SanitizedStackTrace))
            {
                result += Environment.NewLine + SanitizedStackTrace;
            }
            if (InnerException != null)
            {
                result += Environment.NewLine + " ---> " + InnerException.ToString();
            }
            return result;
        }
    }
}
