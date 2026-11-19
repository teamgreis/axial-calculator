using Microsoft.UI.Xaml;
using Axial.App.Modules.Calculator;

namespace Axial.App.Modules.Calculator;

/// <summary>Rechner-Modul: algebraische Eingabe + exakter Kern.</summary>
public sealed class CalculatorModule : IModule
{
    public string Id => "calculator";
    public string Title => "Rechner";
    public string IconGlyph => "\uE8EF";  // Taschenrechner-Symbol
    public string Description => "Algebraischer Rechner mit exaktem Mathe-Kern";

    public FrameworkElement CreateView() => new CalculatorPage();
}
