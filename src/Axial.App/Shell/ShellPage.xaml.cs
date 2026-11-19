using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using Axial.App.Modules;
using Axial.App.Services;
using Axial.App.Modules.Calculator;

namespace Axial.App.Shell;

public sealed partial class ShellPage : Page
{
    private IModule? currentModule;

    public ShellPage()
    {
        InitializeComponent();

        // Titelleiste verschiebbar
        AppTitleBar.ManipulationMode = ManipulationModes.None;

        // Module registrieren
        ModuleRegistry.Register(new CalculatorModule());

        // Status: Kern-Version
        StatusChip.Text = "Kern " + AxialNativeVersion();

        // Startmodul
        if (ModuleRegistry.Activate("calculator"))
        {
            ShowActiveModule();
        }

        // Modul-Aktionen: Winkelmodus (erster kontextabhängiger Block)
        BuildModuleActions();
    }

    private static string AxialNativeVersion()
    {
        try { return AxialNativeVersionSafe(); }
        catch { return "?"; }
    }

    private static string AxialNativeVersionSafe() => MathCore.Net.AxialNative.Version();

    private void BuildModuleActions()
    {
        ModuleActions.Children.Clear();

        var angle = new TextBlock
        {
            Text = "DEG",
            FontSize = 13,
            VerticalAlignment = VerticalAlignment.Center,
            Foreground = (Microsoft.UI.Xaml.Media.SolidColorBrush)Application.Current.Resources["AxialOnSurfaceVariantBrush"]
        };

        var toggle = new Button
        {
            Content = "Winkelmodus: DEG",
            Padding = new Thickness(12, 6),
            CornerRadius = new CornerRadius(6)
        };
        toggle.Click += (_, _) =>
        {
            var svc = AxialEngineService.Instance;
            svc.Angle = svc.Angle switch
            {
                AxialAngleMode.Deg => AxialAngleMode.Rad,
                AxialAngleMode.Rad => AxialAngleMode.Gon,
                _ => AxialAngleMode.Deg
            };
            string label = svc.Angle switch
            {
                AxialAngleMode.Deg => "DEG",
                AxialAngleMode.Rad => "RAD",
                _ => "GON"
            };
            toggle.Content = "Winkelmodus: " + label;
        };

        ModuleActions.Children.Add(toggle);
        ModuleActions.Children.Add(angle);
    }

    private void ShowActiveModule()
    {
        var m = ModuleRegistry.Active;
        if (m is null) return;
        currentModule = m;
        ModuleTitle.Text = m.Title;
        ModuleHost.Content = m.CreateView();
    }

    private void OnToolMenu(object sender, RoutedEventArgs e)
    {
        var flyout = new MenuFlyout();

        foreach (var m in ModuleRegistry.All)
        {
            var item = new MenuFlyoutItem
            {
                Text = m.Title,
                Icon = new FontIcon { Glyph = m.IconGlyph }
            };
            var id = m.Id;
            item.Click += (_, _) =>
            {
                if (ModuleRegistry.Activate(id)) ShowActiveModule();
            };
            flyout.Items.Add(item);
        }

        flyout.Items.Add(new MenuFlyoutSeparator());
        var historyItem = new MenuFlyoutItem { Text = "Verlauf leeren" };
        historyItem.Click += (_, _) => AxialEngineService.Instance.ClearHistory();
        flyout.Items.Add(historyItem);

        flyout.ShowAt(ToolMenuButton);
    }
}
