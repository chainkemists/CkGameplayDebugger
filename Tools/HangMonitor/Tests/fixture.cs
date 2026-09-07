using System;
using System.IO;
using System.Threading;
using System.Windows.Forms;

internal static class HangCaptureFixture
{
    [STAThread]
    private static int Main(string[] arguments)
    {
        var mode = GetArgument(arguments, "--mode", "exit");
        var readyPath = GetArgument(arguments, "--ready", null);
        var hangDelayMilliseconds = GetIntArgument(arguments, "--hang-delay-ms", 2000);
        var recoveryMilliseconds = GetIntArgument(arguments, "--recovery-ms", 20000);
        var normalExitMilliseconds = GetIntArgument(arguments, "--normal-exit-ms", 1500);

        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);

        using (var form = new Form())
        {
            form.Text = "Ck Hang Capture Fixture";
            form.ShowInTaskbar = false;
            form.StartPosition = FormStartPosition.Manual;
            form.Left = -32000;
            form.Top = -32000;
            form.Width = 240;
            form.Height = 120;

            var timer = new System.Windows.Forms.Timer();
            timer.Interval = Math.Max(1, mode == "hang" ? hangDelayMilliseconds : normalExitMilliseconds);
            timer.Tick += delegate
            {
                timer.Stop();
                timer.Dispose();

                if (mode != "hang")
                {
                    Application.ExitThread();
                    return;
                }

                ThreadPool.QueueUserWorkItem(delegate
                {
                    Thread.Sleep(Math.Max(1000, recoveryMilliseconds));
                    Environment.Exit(0);
                });

                // Deliberately stop dispatching this visible top-level window. ProcDump -h uses
                // Windows' normal hung-window detection; the worker above later exits cleanly.
                Thread.Sleep(Timeout.Infinite);
            };

            form.Shown += delegate
            {
                if (!String.IsNullOrEmpty(readyPath))
                {
                    File.WriteAllText(readyPath, "ready");
                }

                timer.Start();
            };

            Application.Run(form);
        }

        return 0;
    }

    private static string GetArgument(string[] arguments, string name, string defaultValue)
    {
        for (var index = 0; index + 1 < arguments.Length; ++index)
        {
            if (String.Equals(arguments[index], name, StringComparison.OrdinalIgnoreCase))
            {
                return arguments[index + 1];
            }
        }

        return defaultValue;
    }

    private static int GetIntArgument(string[] arguments, string name, int defaultValue)
    {
        int value;
        return Int32.TryParse(GetArgument(arguments, name, null), out value) ? value : defaultValue;
    }
}
