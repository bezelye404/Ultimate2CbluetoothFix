using System;

namespace BitDoFixer
{
    public static class Program
    {
        [STAThread]
        public static void Main()
        {
            try
            {
                var app = new BitDoFixer.App();
                app.InitializeComponent();
                app.Run();
            }
            catch (Exception ex)
            {
                string log = $"CRASH [{DateTime.Now}]: {ex.Message}\n{ex.StackTrace}\n\n";
                if (ex.InnerException != null)
                {
                    log += $"INNER: {ex.InnerException.Message}\n{ex.InnerException.StackTrace}\n";
                }
                
                System.IO.File.AppendAllText("crash.log", log);
                // Try to show message box if possible
                try {
                    System.Windows.MessageBox.Show($"FATAL ERROR: {ex.Message}\nCheck crash.log", "8BitDo Fixer", System.Windows.MessageBoxButton.OK, System.Windows.MessageBoxImage.Error);
                } catch { } // If MessageBox fails (e.g. missing dispatcher)
            }
        }
    }
}
