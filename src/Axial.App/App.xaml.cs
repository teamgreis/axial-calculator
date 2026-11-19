using Microsoft.UI.Xaml;

namespace Axial.App;

/// <summary>Axial application root.</summary>
public partial class App : Application
{
    public static Window? Window { get; private set; }

    public App()
    {
        InitializeComponent();
        UnhandledException += OnUnhandledException;
    }

    private void OnUnhandledException(object sender, Microsoft.UI.Xaml.UnhandledExceptionEventArgs e)
    {
        // Fehlerunanfälligkeit: kein stilles Verschwinden — Meldung zeigen,
        // Handled=true nur bei behobenen Kern-Zuständen.
        System.Diagnostics.Debug.WriteLine("Axial-Fehler: " + e.Exception);
    }

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        var window = new Shell.AxialWindow();
        Window = window;
        window.Activate();
    }
}
