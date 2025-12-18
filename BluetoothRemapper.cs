using System;
using System.Threading;
using System.Threading.Tasks;
using SharpDX.DirectInput;
using Nefarius.ViGEm.Client;
using Nefarius.ViGEm.Client.Targets;
using Nefarius.ViGEm.Client.Targets.Xbox360;



namespace BitDoFixer
{
    internal static class BluetoothRemapper
{
    private const int Deadzone = 4000;

    public static async Task RunAsync(CancellationToken token, Action<string>? logCallback = null, Action<string>? statusCallback = null)
    {
        void Log(string m) => logCallback?.Invoke(m);
        
        Log("Bluetooth (DInput) -> Virtual Xbox 360 Remapper Started");

        var directInput = new DirectInput();

        var devices = directInput.GetDevices(DeviceType.Gamepad, DeviceEnumerationFlags.AttachedOnly);
        if (devices.Count == 0) devices = directInput.GetDevices(DeviceType.Joystick, DeviceEnumerationFlags.AttachedOnly);

        if (devices.Count == 0)
        {
            Log("ERROR: DInput gamepad/joystick not found!");
            statusCallback?.Invoke("Not Found");
            return;
        }

        var chosen = devices[0];
        Log($"Source Device: {chosen.InstanceName}");
        statusCallback?.Invoke("Connected");

        using var joystick = new Joystick(directInput, chosen.InstanceGuid);
        joystick.Properties.BufferSize = 128; // Buffer
        joystick.Acquire();

        using var client = new ViGEmClient();
        var controller = client.CreateXbox360Controller(0x045E, 0x028E);
        
        controller.FeedbackReceived += (sender, args) => { };

        controller.Connect();
        controller.Connect();
        Log("Virtual Xbox Controller Connected. Ready!");

        var timer = new PeriodicTimer(TimeSpan.FromMilliseconds(5));

        while (await timer.WaitForNextTickAsync(token))
        {
            joystick.Poll();
            var state = joystick.GetCurrentState();
            if (state is null) continue;

            short lx = NormalizeAxis(state.X);
            short ly = NormalizeAxis(state.Y);

            short rx = NormalizeAxis(state.Z);
            short ry = NormalizeAxis(state.RotationZ);
            
            lx = ApplyDeadzone(lx);
            ly = ApplyDeadzone(ly);
            rx = ApplyDeadzone(rx);
            ry = ApplyDeadzone(ry);

            controller.SetAxisValue(Xbox360Axis.LeftThumbX, lx);
            controller.SetAxisValue(Xbox360Axis.LeftThumbY, (short)-ly); 
            controller.SetAxisValue(Xbox360Axis.RightThumbX, rx);
            controller.SetAxisValue(Xbox360Axis.RightThumbY, (short)-ry); 

            byte lt = 0; if (GetBtn(state, 8)) lt = 255;
            byte rt = 0; if (GetBtn(state, 9)) rt = 255;
            controller.SetSliderValue(Xbox360Slider.LeftTrigger, lt);
            controller.SetSliderValue(Xbox360Slider.RightTrigger, rt);

            SetButton(controller, Xbox360Button.A, GetBtn(state, 0)); 
            SetButton(controller, Xbox360Button.B, GetBtn(state, 1));
            SetButton(controller, Xbox360Button.X, GetBtn(state, 3)); 
            SetButton(controller, Xbox360Button.Y, GetBtn(state, 4));

            SetButton(controller, Xbox360Button.LeftShoulder, GetBtn(state, 6));
            SetButton(controller, Xbox360Button.RightShoulder, GetBtn(state, 7));

            SetButton(controller, Xbox360Button.Back, GetBtn(state, 10));
            SetButton(controller, Xbox360Button.Start, GetBtn(state, 11));
            
            SetButton(controller, Xbox360Button.LeftThumb, GetBtn(state, 13));
            SetButton(controller, Xbox360Button.RightThumb, GetBtn(state, 14));

            ApplyDpad(controller, state.PointOfViewControllers);

            controller.SubmitReport();
        }
    }

    private static bool GetBtn(JoystickState s, int index)
    {
        var b = s.Buttons;
        if (b is null) return false;
        if (index < 0 || index >= b.Length) return false;
        return b[index];
    }

    private static void SetButton(IXbox360Controller c, Xbox360Button btn, bool pressed)
    {
        if (pressed) c.SetButtonState(btn, true);
        else c.SetButtonState(btn, false);
    }

    private static void ApplyDpad(IXbox360Controller c, int[] povs)
    {
        c.SetButtonState(Xbox360Button.Up, false);
        c.SetButtonState(Xbox360Button.Down, false);
        c.SetButtonState(Xbox360Button.Left, false);
        c.SetButtonState(Xbox360Button.Right, false);

        if (povs is null || povs.Length == 0) return;

        int pov = povs[0];
        if (pov < 0) return;

        bool up = (pov >= 31500 || pov <= 4500);
        bool right = (pov >= 4500 && pov <= 13500);
        bool down = (pov >= 13500 && pov <= 22500);
        bool left = (pov >= 22500 && pov <= 31500);

        c.SetButtonState(Xbox360Button.Up, up);
        c.SetButtonState(Xbox360Button.Right, right);
        c.SetButtonState(Xbox360Button.Down, down);
        c.SetButtonState(Xbox360Button.Left, left);
    }

    private static short NormalizeAxis(int v)
    {
        // SharpDX axis çoğu zaman 0..65535
        // merkez 32767 civarı; bunu -32768..32767’ye çevir
        int centered = v - 32767;
        if (centered < short.MinValue) centered = short.MinValue;
        if (centered > short.MaxValue) centered = short.MaxValue;
        return (short)centered;
    }

    private static short ApplyDeadzone(short v)
    {
        if (v > -Deadzone && v < Deadzone) return 0;
        return v;
    }

    private static byte ToTrigger(int v)
    {
        if (v < 0) v = 0;
        if (v > 65535) v = 65535;
        return (byte)(v / 257); // 65535/255≈257
    }
    }
}
