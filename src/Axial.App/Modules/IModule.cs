using Microsoft.UI.Xaml.Controls;

namespace Axial.App.Modules;

/// <summary>
/// Ein Modul hängt sich 1:1 in die Registry — die Shell kennt nur diese
/// Schnittstelle. Neue Module = Datensatz + Titel + Glyphe + Ansicht.
/// </summary>
public interface IModule
{
    string Id { get; }
    string Title { get; }
    string IconGlyph { get; }
    string Description { get; }

    /// <summary>Baut eine frische Ansichtsinstanz für das Modul.</summary>
    FrameworkElement CreateView();
}

public static class ModuleRegistry
{
    private static readonly List<IModule> Modules = new();
    private static IModule? active;

    /// <summary>Registriert ein Modul. Idempotent je Id.</summary>
    public static void Register(IModule module)
    {
        if (Modules.Any(m => m.Id == module.Id)) return;
        Modules.Add(module);
    }

    public static IReadOnlyList<IModule> All => Modules;

    public static IModule? Find(string id) => Modules.FirstOrDefault(m => m.Id == id);

    public static IModule? Active => active;

    /// <summary>Wählt ein Modul; gibt true zurück, wenn es existiert.</summary>
    public static bool Activate(string id)
    {
        var m = Find(id);
        if (m is null) return false;
        active = m;
        return true;
    }
}
