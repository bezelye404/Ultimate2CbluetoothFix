using System.Windows;

namespace BitDoFixer
{
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            AppDomain.CurrentDomain.UnhandledException += (s, args) =>
                LogCrash((Exception)args.ExceptionObject);

            DispatcherUnhandledException += (s, args) =>
            {
                LogCrash(args.Exception);
                args.Handled = true;
            };

            base.OnStartup(e);
        }

        private void LogCrash(Exception ex)
        {
            string log = $"CRASH [{DateTime.Now}]: {ex.Message}\n{ex.StackTrace}\n\n";
            if(ex.InnerException != null)
                log += $"INNER: {ex.InnerException.Message}\n{ex.InnerException.StackTrace}\n";
            
            System.IO.File.AppendAllText("crash.log", log);
            MessageBox.Show($"Application Crashed: {ex.Message}", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }
}
