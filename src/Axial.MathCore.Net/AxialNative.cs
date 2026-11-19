using System.Runtime.InteropServices;
using System.Text.Json;

namespace Axial.MathCore.Net;

/// <summary>Native result of an evaluation (mirrors the C-ABI JSON).</summary>
public sealed class AxialResult
{
    public bool Ok { get; init; }
    public bool Exact { get; init; }
    public string Text { get; init; } = "";
    public double Approx { get; init; }
    public string Kind { get; init; } = "real";
    public int ParensFixed { get; init; }
    public string? ErrorKind { get; init; }
    public int ErrorPos { get; init; } = -1;
    public string? ErrorToken { get; init; }
    public string? ErrorMessage { get; init; }
    public string? ErrorHint { get; init; }
}

public enum AxialAngleMode { Deg, Rad, Gon }

public static class AxialNative
{
    private const string DllName = "axial_mathcore_dll.dll";

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    private static extern IntPtr axial_eval(IntPtr text, IntPtr optionsJson);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern void axial_free(IntPtr p);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr axial_version();

    /// <summary>Core version string, e.g. "0.1.0".</summary>
    public static string Version()
    {
        IntPtr p = axial_version();
        return p == IntPtr.Zero ? "" : Marshal.PtrToStringUTF8(p) ?? "";
    }

    /// <summary>
    /// Evaluates an expression. Never throws on bad input: errors come back
    /// as a failed AxialResult with kind/pos/message/hint.
    /// </summary>
    public static AxialResult Eval(string text, AxialAngleMode angle,
                                   IReadOnlyDictionary<string, double>? variables = null)
    {
        string options = BuildOptions(angle, variables);
        IntPtr tp = Marshal.StringToCoTaskMemUTF8(text);
        IntPtr op = Marshal.StringToCoTaskMemUTF8(options);
        IntPtr res = IntPtr.Zero;
        try
        {
            res = axial_eval(tp, op);
            if (res == IntPtr.Zero)
            {
                return Fail("Internal", -1, "", "Kern antwortete nicht", "");
            }
            string json = Marshal.PtrToStringUTF8(res) ?? "";
            return Parse(json);
        }
        catch (DllNotFoundException)
        {
            return Fail("Internal", -1, "", "Mathe-Kern (axial_mathcore_dll.dll) nicht gefunden",
                        "Build der Kern-DLL prüfen.");
        }
        catch (EntryPointNotFoundException)
        {
            return Fail("Internal", -1, "", "Mathe-Kern-ABI passt nicht zur DLL", "");
        }
        finally
        {
            if (res != IntPtr.Zero) axial_free(res);
            Marshal.FreeCoTaskMem(tp);
            Marshal.FreeCoTaskMem(op);
        }
    }

    private static AxialResult Fail(string kind, int pos, string token, string msg, string hint)
        => new()
        {
            Ok = false, ErrorKind = kind, ErrorPos = pos, ErrorToken = token,
            ErrorMessage = msg, ErrorHint = hint
        };

    private static string BuildOptions(AxialAngleMode angle,
                                       IReadOnlyDictionary<string, double>? variables)
    {
        string mode = angle switch
        {
            AxialAngleMode.Deg => "deg",
            AxialAngleMode.Gon => "gon",
            _ => "rad"
        };
        if (variables is null || variables.Count == 0)
        {
            return "{\"angle\":\"" + mode + "\"}";
        }
        var sb = new System.Text.StringBuilder();
        sb.Append("{\"angle\":\"").Append(mode).Append("\",\"vars\":{");
        bool first = true;
        foreach (var kv in variables)
        {
            if (!first) sb.Append(',');
            first = false;
            sb.Append('"').Append(kv.Key).Append("\":").Append(kv.Value.ToString("R", System.Globalization.CultureInfo.InvariantCulture));
        }
        sb.Append("}}");
        return sb.ToString();
    }

    private static AxialResult Parse(string json)
    {
        try
        {
            using JsonDocument doc = JsonDocument.Parse(json);
            JsonElement root = doc.RootElement;
            bool ok = root.TryGetProperty("ok", out JsonElement okEl) && okEl.GetBoolean();
            if (!ok)
            {
                JsonElement e = root.TryGetProperty("error", out JsonElement err) ? err : default;
                return Fail(
                    e.TryGetProperty("kind", out JsonElement k) ? k.GetString() ?? "ReadableFailure" : "ReadableFailure",
                    e.TryGetProperty("pos", out JsonElement p) ? p.GetInt32() : -1,
                    e.TryGetProperty("token", out JsonElement t) ? t.GetString() ?? "" : "",
                    e.TryGetProperty("message", out JsonElement m) ? m.GetString() ?? "" : "",
                    e.TryGetProperty("hint", out JsonElement h) ? h.GetString() ?? "" : "");
            }
            return new AxialResult
            {
                Ok = true,
                Exact = root.TryGetProperty("exact", out JsonElement ex) && ex.GetBoolean(),
                Text = root.TryGetProperty("text", out JsonElement tx) ? tx.GetString() ?? "" : "",
                Approx = root.TryGetProperty("approx", out JsonElement ap) ? ap.GetDouble() : 0.0,
                Kind = root.TryGetProperty("kind", out JsonElement kd) ? kd.GetString() ?? "real" : "real",
                ParensFixed = root.TryGetProperty("parensFixed", out JsonElement pf) ? pf.GetInt32() : 0
            };
        }
        catch (JsonException)
        {
            return Fail("Internal", -1, "", "Kern-Antwort nicht lesbar: " + json, "");
        }
    }
}
