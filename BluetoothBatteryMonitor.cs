using System;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Devices.Enumeration;
using Windows.Storage.Streams;


namespace BitDoFixer
{
    internal static class BluetoothBatteryMonitor
{
    private static readonly Guid BatteryServiceUuid = GattServiceUuids.Battery; 
    private static readonly Guid BatteryLevelUuid = GattCharacteristicUuids.BatteryLevel;

    public static async Task RunAsync(int initialDelaySeconds, int intervalSeconds, CancellationToken token, Action<string>? logCallback = null, Action<string, int>? batteryCallback = null)
    {
        void Log(string m) => logCallback?.Invoke(m);

        Log($"BLE Battery Monitor Started (Initial: {initialDelaySeconds}s, Interval: {intervalSeconds}s)");

        await Task.Delay(TimeSpan.FromSeconds(initialDelaySeconds), token);
        
        await PollBatteryAsync(Log, batteryCallback, token);

        var timer = new PeriodicTimer(TimeSpan.FromSeconds(intervalSeconds));

        while (await timer.WaitForNextTickAsync(token))
        {
            await PollBatteryAsync(Log, batteryCallback, token);
        }
    }

    private static async Task PollBatteryAsync(Action<string> Log, Action<string, int>? batteryCallback, CancellationToken token)
    {
        try
        {
            string selector = GattDeviceService.GetDeviceSelectorFromUuid(BatteryServiceUuid);
            
            var devices = await DeviceInformation.FindAllAsync(selector);

            if (devices.Count == 0)
            {
                return;
            }

            foreach (var devInfo in devices)
            {
                if (!devInfo.Name.Contains("8BitDo", StringComparison.OrdinalIgnoreCase))
                {
                    continue;
                }

                try
                {
                    using var service = await GattDeviceService.FromIdAsync(devInfo.Id);
                    if (service != null && service.Device != null)
                    {
                        var characteristics = await service.GetCharacteristicsForUuidAsync(BatteryLevelUuid);
                        if (characteristics.Status == GattCommunicationStatus.Success && characteristics.Characteristics.Count > 0)
                        {
                            var ch = characteristics.Characteristics[0];
                            var result = await ch.ReadValueAsync();
                            
                            if (result.Status == GattCommunicationStatus.Success)
                            {
                                var reader = DataReader.FromBuffer(result.Value);
                                byte level = reader.ReadByte();

                                Log($"{service.Device.Name} Battery: {level}%");
                                batteryCallback?.Invoke(service.Device.Name, level);
                            }
                        }
                    }
                }
                catch (Exception)
                {
                }
            }
        }
        catch (Exception ex)
        {
            Log($"[BLE Scan Error]: {ex.Message}");
        }
    }
    }
}
