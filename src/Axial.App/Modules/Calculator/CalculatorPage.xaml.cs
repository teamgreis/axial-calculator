using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using Axial.App.Services;

namespace Axial.App.Modules.Calculator;

public sealed partial class CalculatorPage : Page
{
    public CalculatorPage()
    {
        InitializeComponent();
    }

    private void OnKey(object sender, RoutedEventArgs e)
    {
        if (sender is not Button b || b.Tag is not string tag) return;

        switch (tag)
        {
            case "clear":
                InputBox.Text = "";
                ResultText.Text = "—";
                ErrorText.Visibility = Visibility.Collapsed;
                break;
            case "back":
                if (InputBox.Text.Length > 0)
                    InputBox.Text = InputBox.Text[..^1];
                break;
            case "=":
                Evaluate();
                break;
            default:
                // auto-close missing parenthesis hint: Tag direkt anhängen
                var box = InputBox;
                int caret = box.SelectionStart >= 0 ? box.SelectionStart : box.Text.Length;
                string insert = tag;
                box.Text = box.Text.Insert(caret, insert);
                box.SelectionStart = caret + insert.Length;
                break;
        }
    }

    private void OnInputKeyDown(object sender, KeyRoutedEventArgs e)
    {
        if (e.Key == Windows.System.VirtualKey.Enter)
        {
            Evaluate();
            e.Handled = true;
        }
    }

    private void Evaluate()
    {
        ErrorText.Visibility = Visibility.Collapsed;
        string expr = InputBox.Text?.Trim() ?? "";
        if (expr.Length == 0)
        {
            ResultText.Text = "—";
            return;
        }

        AxialResult result = AxialEngineService.Instance.Evaluate(expr);
        if (result.Ok)
        {
            ResultText.Text = result.Text;
            if (result.ParensFixed > 0)
            {
                ShowNote($"Klammern automatisch geschlossen ({result.ParensFixed}).");
            }
        }
        else
        {
            ResultText.Text = "—";
            string msg = result.ErrorMessage ?? "Fehler";
            if (!string.IsNullOrEmpty(result.ErrorHint))
                msg += " — " + result.ErrorHint;
            if (result.ErrorPos >= 0)
                msg = $"Position {result.ErrorPos}: {msg}";
            ShowNote(msg);
        }
    }

    private void ShowNote(string message)
    {
        ErrorText.Text = message;
        ErrorText.Visibility = Visibility.Visible;
    }
}
