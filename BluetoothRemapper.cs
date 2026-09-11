using System;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using SharpDX.DirectInput;
using Nefarius.ViGEm.Client;
using Nefarius.ViGEm.Client.Targets;
using Nefarius.ViGEm.Client.Targets.Xbox360;

namespace BitDoFixer
{
    public enum RemapperStatus { Searching, Connected, Disconnected, Stopped }

    internal static class BluetoothRemapper
    {
        private const int Deadzone = 4000;
        private const int KeepAliveTicks = 50; // ~250ms keepalive when idle

        public static async Task RunAsync(
            IntPtr hwnd,
            CancellationToken token,
            Action<string>? logCallback = null,
            Action<RemapperStatus, string?>? statusCallback = null)
        {
            void Log(string m) => logCallback?.Invoke(m);

            var loc = Localization.Instance;
            Log(loc.LogMapperStart);

            ViGEmClient? client = null;
            IXbox360Controller? controller = null;

            try
            {
                client = new ViGEmClient();
                controller = client.CreateXbox360Controller(0x045E, 0x028E);
                controller.Connect();
            }
            catch (Exception ex)
            {
                Log($"Failed to initialize ViGEmBus: {ex.Message}");
                statusCallback?.Invoke(RemapperStatus.Disconnected, null);
                client?.Dispose();
                return;
            }

            while (!token.IsCancellationRequested)
            {
                Joystick? joystick = null;
                DirectInput? directInput = null;

                try
                {
                    statusCallback?.Invoke(RemapperStatus.Searching, null);
                    directInput = new DirectInput();

                    // Find 8BitDo or fallback to first gamepad/joystick
                    var devices = directInput.GetDevices(DeviceType.Gamepad, DeviceEnumerationFlags.AttachedOnly).ToList();
                    if (devices.Count == 0)
                    {
                        devices = directInput.GetDevices(DeviceType.Joystick, DeviceEnumerationFlags.AttachedOnly).ToList();
                    }

                    var chosen = devices.FirstOrDefault(d =>
                        d.InstanceName.Contains("8BitDo", StringComparison.OrdinalIgnoreCase) ||
                        d.ProductName.Contains("8BitDo", StringComparison.OrdinalIgnoreCase)) ?? devices.FirstOrDefault();

                    if (chosen == null)
                    {
                        directInput.Dispose();
                        directInput = null;
                        await Task.Delay(2000, token);
                        continue;
                    }

                    Log(loc.LogMapperSource(chosen.InstanceName));

                    joystick = new Joystick(directInput, chosen.InstanceGuid);
                    joystick.SetCooperativeLevel(hwnd, CooperativeLevel.Exclusive | CooperativeLevel.Background);
                    joystick.Acquire();

                    Log(loc.LogMapperReady);
                    statusCallback?.Invoke(RemapperStatus.Connected, chosen.InstanceName);

                    // State cache for dirty-checking (reduces idle CPU & kernel calls by ~98%)
                    short prevLx = 0, prevLy = 0, prevRx = 0, prevRy = 0;
                    byte prevLt = 0, prevRt = 0;
                    int prevButtonsHash = 0;
                    int prevPov = -1;
                    int idleTicks = 0;

                    using var timer = new PeriodicTimer(TimeSpan.FromMilliseconds(5));

                    while (await timer.WaitForNextTickAsync(token))
                    {
                        joystick.Poll();
                        var state = joystick.GetCurrentState();
                        if (state is null) continue;

                        var buttons = state.Buttons;

                        short lx = ApplyDeadzone(NormalizeAxis(state.X));
                        short ly = NegateAxis(ApplyDeadzone(NormalizeAxis(state.Y)));
                        short rx = ApplyDeadzone(NormalizeAxis(state.Z));
                        short ry = NegateAxis(ApplyDeadzone(NormalizeAxis(state.RotationZ)));

                        byte lt = (buttons != null && buttons.Length > 8 && buttons[8]) ? (byte)255 : (byte)0;
                        byte rt = (buttons != null && buttons.Length > 9 && buttons[9]) ? (byte)255 : (byte)0;

                        int pov = (state.PointOfViewControllers != null && state.PointOfViewControllers.Length > 0)
                            ? state.PointOfViewControllers[0]
                            : -1;

                        int buttonsHash = ComputeButtonsHash(buttons);

                        bool changed = (lx != prevLx) || (ly != prevLy) ||
                                       (rx != prevRx) || (ry != prevRy) ||
                                       (lt != prevLt) || (rt != prevRt) ||
                                       (buttonsHash != prevButtonsHash) ||
                                       (pov != prevPov);

                        if (!changed)
                        {
                            idleTicks++;
                            if (idleTicks < KeepAliveTicks)
                            {
                                continue; // Skip kernel submit report when idle
                            }
                        }

                        // State changed or keep-alive tick
                        idleTicks = 0;
                        prevLx = lx; prevLy = ly;
                        prevRx = rx; prevRy = ry;
                        prevLt = lt; prevRt = rt;
                        prevButtonsHash = buttonsHash;
                        prevPov = pov;

                        controller.SetAxisValue(Xbox360Axis.LeftThumbX, lx);
                        controller.SetAxisValue(Xbox360Axis.LeftThumbY, ly);
                        controller.SetAxisValue(Xbox360Axis.RightThumbX, rx);
                        controller.SetAxisValue(Xbox360Axis.RightThumbY, ry);

                        controller.SetSliderValue(Xbox360Slider.LeftTrigger, lt);
                        controller.SetSliderValue(Xbox360Slider.RightTrigger, rt);

                        if (buttons != null)
                        {
                            SetButton(controller, Xbox360Button.A, GetBtn(buttons, 0));
                            SetButton(controller, Xbox360Button.B, GetBtn(buttons, 1));
                            SetButton(controller, Xbox360Button.X, GetBtn(buttons, 3));
                            SetButton(controller, Xbox360Button.Y, GetBtn(buttons, 4));

                            SetButton(controller, Xbox360Button.LeftShoulder, GetBtn(buttons, 6));
                            SetButton(controller, Xbox360Button.RightShoulder, GetBtn(buttons, 7));

                            SetButton(controller, Xbox360Button.Back, GetBtn(buttons, 10));
                            SetButton(controller, Xbox360Button.Start, GetBtn(buttons, 11));

                            SetButton(controller, Xbox360Button.LeftThumb, GetBtn(buttons, 13));
                            SetButton(controller, Xbox360Button.RightThumb, GetBtn(buttons, 14));
                        }

                        ApplyDpad(controller, pov);
                        controller.SubmitReport();
                    }
                }
                catch (OperationCanceledException)
                {
                    break;
                }
                catch (Exception ex)
                {
                    Log(loc.LogMapperError(ex.Message));
                    statusCallback?.Invoke(RemapperStatus.Disconnected, null);
                }
                finally
                {
                    try { joystick?.Unacquire(); } catch { }
                    joystick?.Dispose();
                    directInput?.Dispose();
                }

                if (!token.IsCancellationRequested)
                {
                    // Delay before auto-reconnect attempt
                    try { await Task.Delay(2000, token); } catch (OperationCanceledException) { break; }
                }
            }

            try { controller?.Disconnect(); } catch { }
            client?.Dispose();
        }

        private static int ComputeButtonsHash(bool[]? buttons)
        {
            if (buttons == null) return 0;
            int hash = 0;
            int max = Math.Min(buttons.Length, 16);
            for (int i = 0; i < max; i++)
            {
                if (buttons[i]) hash |= (1 << i);
            }
            return hash;
        }

        private static bool GetBtn(bool[] buttons, int index)
        {
            if (index < 0 || index >= buttons.Length) return false;
            return buttons[index];
        }

        private static void SetButton(IXbox360Controller c, Xbox360Button btn, bool pressed)
        {
            c.SetButtonState(btn, pressed);
        }

        private static void ApplyDpad(IXbox360Controller c, int pov)
        {
            if (pov < 0)
            {
                c.SetButtonState(Xbox360Button.Up, false);
                c.SetButtonState(Xbox360Button.Right, false);
                c.SetButtonState(Xbox360Button.Down, false);
                c.SetButtonState(Xbox360Button.Left, false);
                return;
            }

            c.SetButtonState(Xbox360Button.Up, (pov >= 31500 || pov <= 4500));
            c.SetButtonState(Xbox360Button.Right, (pov >= 4500 && pov <= 13500));
            c.SetButtonState(Xbox360Button.Down, (pov >= 13500 && pov <= 22500));
            c.SetButtonState(Xbox360Button.Left, (pov >= 22500 && pov <= 31500));
        }

        private static short NormalizeAxis(int v)
        {
            int centered = v - 32767;
            if (centered < short.MinValue) return short.MinValue;
            if (centered > short.MaxValue) return short.MaxValue;
            return (short)centered;
        }

        private static short ApplyDeadzone(short v)
        {
            if (v > -Deadzone && v < Deadzone) return 0;
            return v;
        }

        private static short NegateAxis(short v)
        {
            if (v == short.MinValue) return short.MaxValue;
            return (short)-v;
        }
    }
}
