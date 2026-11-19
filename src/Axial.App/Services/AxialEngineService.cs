using Axial.MathCore.Net;

namespace Axial.App.Services;

/// <summary>
/// Zustandsbehaftete Fassade des Mathe-Kerns: Winkelmodus, Variablen
/// (inkl. ans), Verlauf. Einzelschuss-Auswertung geht immer an denselben
/// kompilierten Pfad — der Dienst hält nur Kontext.
/// </summary>
public sealed class AxialEngineService
{
    private static readonly Lazy<AxialEngineService> Lazy =
        new(() => new AxialEngineService());
    public static AxialEngineService Instance => Lazy.Value;

    public AxialAngleMode Angle { get; set; } = AxialAngleMode.Deg;

    public double Ans { get; private set; }
    public bool HasAns { get; private set; }

    public ObservableCollection<string> History { get; } = new();

    /// <summary>Bekannte Variablen; "ans" wird ergänzt, wenn gesetzt.</summary>
    public Dictionary<string, double> Variables { get; } = new();

    public AxialResult Evaluate(string expression)
    {
        if (HasAns) Variables["ans"] = Ans;

        var vars = new Dictionary<string, double>(Variables);
        if (HasAns) vars["ans"] = Ans;

        AxialResult result = AxialNative.Eval(expression, Angle, vars);
        if (result.Ok)
        {
            Ans = result.Approx;
            HasAns = true;
            History.Insert(0, $"{expression} = {result.Text}");
            if (History.Count > 200) History.RemoveAt(History.Count - 1);
        }
        return result;
    }

    public void ClearHistory() => History.Clear();
}
