using System;
using System.Threading;
using System.Threading.Tasks;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Devices.Enumeration;
using Windows.Storage.Streams;

namespace BitDoFixer
{
    internal static class BluetoothBatteryMonitor
    {
        private static readonly Guid BatteryServiceUuid = GattServiceUuids.Battery; 
        private static readonly Guid BatteryLevelUuid = GattCharacteristicUuids.BatteryLevel;
        private static string? _cachedDeviceId = null;

        public static async Task RunAsync(
            int initialDelaySeconds,
            int intervalSeconds,
            CancellationToken token,
            Action<string>? logCallback = null,
            Action<string, int>? batteryCallback = null)
        {
            void Log(string m) => logCallback?.Invoke(m);

            var loc = Localization.Instance;
            Log(loc.LogBatteryStart(initialDelaySeconds, intervalSeconds));

            try
            {
                await Task.Delay(TimeSpan.FromSeconds(initialDelaySeconds), token);
                await PollBatteryAsync(Log, batteryCallback);

                using var timer = new PeriodicTimer(TimeSpan.FromSeconds(intervalSeconds));
                while (await timer.WaitForNextTickAsync(token))
                {
                    await PollBatteryAsync(Log, batteryCallback);
                }
            }
            catch (OperationCanceledException)
            {
                // Expected on cancellation
            }
            catch (Exception ex)
            {
                Log(Localization.Instance.LogBatteryFatal(ex.Message));
            }
        }

        private static async Task PollBatteryAsync(Action<string> Log, Action<string, int>? batteryCallback)
        {
            try
            {
                // Try reading from cached device first to avoid costly system-wide device enumerations
                if (!string.IsNullOrEmpty(_cachedDeviceId))
                {
                    bool success = await TryReadBatteryFromIdAsync(_cachedDeviceId, Log, batteryCallback);
                    if (success) return;
                    _cachedDeviceId = null;
                }

                string selector = GattDeviceService.GetDeviceSelectorFromUuid(BatteryServiceUuid);
                var devices = await DeviceInformation.FindAllAsync(selector);

                if (devices.Count == 0) return;

                foreach (var devInfo in devices)
                {
                    if (!devInfo.Name.Contains("8BitDo", StringComparison.OrdinalIgnoreCase))
                    {
                        continue;
                    }

                    bool success = await TryReadBatteryFromIdAsync(devInfo.Id, Log, batteryCallback);
                    if (success)
                    {
                        _cachedDeviceId = devInfo.Id;
                        break;
                    }
                }
            }
            catch (Exception ex)
            {
                Log(Localization.Instance.LogBatteryScanError(ex.Message));
            }
        }

        private static async Task<bool> TryReadBatteryFromIdAsync(string deviceId, Action<string> Log, Action<string, int>? batteryCallback)
        {
            try
            {
                using var service = await GattDeviceService.FromIdAsync(deviceId);
                if (service?.Device == null) return false;

                var characteristics = await service.GetCharacteristicsForUuidAsync(BatteryLevelUuid);
                if (characteristics.Status != GattCommunicationStatus.Success || characteristics.Characteristics.Count == 0)
                {
                    return false;
                }

                var ch = characteristics.Characteristics[0];
                var result = await ch.ReadValueAsync();
                if (result.Status == GattCommunicationStatus.Success)
                {
                    var reader = DataReader.FromBuffer(result.Value);
                    byte level = reader.ReadByte();

                    Log(Localization.Instance.LogBatteryLevel(service.Device.Name, level));
                    batteryCallback?.Invoke(service.Device.Name, level);
                    return true;
                }
            }
            catch
            {
                // Ignore failure, will retry or scan again
            }

            return false;
        }
    }
}
