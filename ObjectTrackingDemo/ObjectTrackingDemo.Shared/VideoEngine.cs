using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Windows.Devices.Enumeration;
using Windows.Foundation.Collections;
using Windows.Media.Capture;
using Windows.Media.Devices;
using Windows.Media.MediaProperties;
using Windows.Storage.Streams;
using VideoEffect;

#if WINDOWS_PHONE_APP
using Windows.Devices.Sensors;
#endif

namespace ObjectTrackingDemo
{
    public enum Mode
    {
        Passthrough = 0,
        ChromaFilter = 1,
        EdgeDetection = 2,
        ChromaDelta = 3
    };

    /// <summary>
    /// This class manages the device camera and the native effects.
    /// </summary>
    public class VideoEngine
    {
        // Constants
        public const string RealtimeTransformActivationId = "RealtimeTransform.RealtimeTransform";
        public const string BufferTransformActivationId = "BufferTransform.BufferTransform";
        public readonly MediaStreamType PreviewMediaStreamType = MediaStreamType.VideoPreview;
        public readonly MediaStreamType RecordMediaStreamType = MediaStreamType.VideoRecord;
        public const int ExposureAutoValue = -1;

        private const string propertyKeyCommunication = "Communication";
        private const int MinimumThreshold = 0;
        private const int MaximumThreshold = 255;
        private const uint PreviewFrameRate = 30;

#if WINDOWS_PHONE_APP
        private const uint MaximumVideoWidth = 800;
#else
        private const uint MaximumVideoWidth = 1280;
#endif

        public event EventHandler<string> ShowMessageRequest;

        private InMemoryRandomAccessStream _recordingStream = new InMemoryRandomAccessStream();
        private VideoDeviceController _videoDeviceController;
        private DeviceInformation _deviceInformation;
        private DeviceInformationCollection _deviceInformationCollection;
        private VideoEncodingProperties _hfrVideoEncodingProperties;
        private float[] _yuv;
        private int _exposureStep;

#if WINDOWS_PHONE_APP
        private LightSensor _lightSensor = LightSensor.GetDefault();
        List<double> _lightMeasurements = new List<double>();
        double _lastLightMeasurement;
#endif

        private static VideoEngine _instance;
        /// <summary>
        /// The singleton instance of this class.
        /// </summary>
        public static VideoEngine Instance
        {
            get
            {
                if (_instance == null)
                {
                    _instance = new VideoEngine();
                }

                return _instance;
            }
        }

        #region Properties

        public MediaCapture MediaCapture
        {
            get;
            private set;
        }

        public VideoEffectMessenger Messenger
        {
            get;
            private set;
        }

        public StateManager StateManager
        {
            get;
            private set;
        }

        public bool IsFlashSupported
        {
            get
            {
                return (_videoDeviceController != null && _videoDeviceController.FlashControl.Supported);
            }
        }
        public bool Flash
        {
            get
            {
                bool enabled = false;

                if (IsFlashSupported)
                {
                    enabled = _videoDeviceController.FlashControl.Enabled;
                }

                return enabled;
            }
            set
            {
                if (IsFlashSupported)
                {
                     _videoDeviceController.FlashControl.Enabled = value;
                }
            }
        }

        public bool IsTorchSupported
        {
            get
            {
                return (_videoDeviceController != null && _videoDeviceController.TorchControl.Supported);
            }
        }

        public bool Torch
        {
            get
            {
                bool enabled = false;

                if (IsTorchSupported)
                {
                    enabled = _videoDeviceController.TorchControl.Enabled;
                }

                return enabled;
            }
            set
            {
                if (IsTorchSupported)
                {
                    _videoDeviceController.TorchControl.Enabled = value;
                }
            }
        }

        public bool IsIsoSupported
        {
            get
            {
                return (_videoDeviceController != null && _videoDeviceController.IsoSpeedControl.Supported);
            }
        }

        public IReadOnlyList<IsoSpeedPreset> SupportedIsoSpeedPresets
        {
            get
            {
                IReadOnlyList<IsoSpeedPreset> isoSpeedPresets = null;

                if (IsIsoSupported)
                {
                    isoSpeedPresets = _videoDeviceController.IsoSpeedControl.SupportedPresets;
                }

                return isoSpeedPresets;
            }
        }

        public IsoSpeedPreset IsoSpeedPreset
        {
            get
            {
                IsoSpeedPreset isoSpeed = IsoSpeedPreset.Auto;

                if (IsIsoSupported)
                {
                    isoSpeed = _videoDeviceController.IsoSpeedControl.Preset;
                }

                return isoSpeed;
            }
        }

        public bool IsExposureSupported
        {
            get
            {
                return (_videoDeviceController != null && _videoDeviceController.ExposureControl.Supported);
            }
        }

        public bool IsAutoExposureOn
        {
            get;
            private set;
        }

        public int ExposureMinStep
        {
            get
            {
                return 0;
            }
        }

        public int ExposureMaxStep
        {
            get
            {
                int maxStep = 0;

                if (IsExposureSupported)
                {
                    /*long maxTicks = _videoDeviceController.ExposureControl.Max.Ticks;
                    long minTicks = _videoDeviceController.ExposureControl.Min.Ticks;

                    if (minTicks > 0)
                    {
                        maxStep = (int)Math.Round((double)(maxTicks / minTicks), 0);
                    }*/
                    maxStep = 200;
                }

                return maxStep;
            }
        }

        public int ExposureStep
        {
            get
            {
                return _exposureStep;
            }
        }

        public int Luminance
        {
            get;
            private set;
        }

        public bool IsLuminanceShown
        {
            get;
            private set;
        }

        public bool Initialized
        {
            get;
            private set;
        }

        public bool Started
        {
            get
            {
                return (PreviewStarted && RecordingStarted);
            }
        }

        public bool PreviewStarted
        {
            get;
            private set;
        }

        public bool RecordingStarted
        {
            get;
            private set;
        }

        public bool EffectSet
        {
            get
            {
                return StateManager.State != VideoEffectState.Idle;
            }
        }

        public int ResolutionWidth
        {
            get;
            private set;
        }
   
        public int ResolutionHeight
        { 
            get;
            private set;
        }

        public PropertySet Properties
        {
            get;
            private set;
        }

        #endregion

        /// <summary>
        /// Constructor
        /// </summary>
        private VideoEngine()
        {
            StateManager = new StateManager();
            Messenger = new VideoEffectMessenger(StateManager);

            IsAutoExposureOn = true;

            Properties = new PropertySet();
            Properties[propertyKeyCommunication] = Messenger;           

            StateManager.StateChanged += OnStateChanged;
        }

        public async Task<bool> InitializeAsync()
        {
            if (Initialized)
            {
                // Already intialized
                return true;
            }

            _deviceInformationCollection = await DeviceInformation.FindAllAsync(DeviceClass.VideoCapture);

            // Find the back camera
            _deviceInformation =
                (from camera in _deviceInformationCollection
                 where camera.EnclosureLocation != null
                    && camera.EnclosureLocation.Panel == Windows.Devices.Enumeration.Panel.Back
                 select camera).FirstOrDefault();

            if (_deviceInformation == null && _deviceInformationCollection.Count != 0)
            {
                // Fallback to whatever is available (e.g. webcam on laptop)
                _deviceInformation = _deviceInformationCollection[0];
            }

            if (_deviceInformation == null)
            {
                throw new Exception("No camera device found");
            }

            MediaCapture = new MediaCapture();
            MediaCaptureInitializationSettings settings = new MediaCaptureInitializationSettings();
            settings.StreamingCaptureMode = StreamingCaptureMode.Video;
            settings.VideoDeviceId = _deviceInformation.Id;
            await MediaCapture.InitializeAsync(settings);

#if WINDOWS_PHONE_APP
            _lightSensor.ReportInterval = _lightSensor.MinimumReportInterval;
            _lightSensor.ReadingChanged += OnLightSensorReadingChangedAsync;
#endif

            _videoDeviceController = MediaCapture.VideoDeviceController;

            IList<VideoEncodingProperties> listOfPropertiesWithHighestFrameRate =
                CameraUtils.ResolveAllVideoEncodingPropertiesWithHighestFrameRate(_videoDeviceController, RecordMediaStreamType);

            if (listOfPropertiesWithHighestFrameRate != null && listOfPropertiesWithHighestFrameRate.Count > 0)
            {
                VideoEncodingProperties selectedRecordingVideoEncodingProperties = listOfPropertiesWithHighestFrameRate.ElementAt(0);
                uint selectedRecordingVideoWidth = selectedRecordingVideoEncodingProperties.Width;

                for (int i = 1; i < listOfPropertiesWithHighestFrameRate.Count; ++i)
                {
                    VideoEncodingProperties currentProperties = listOfPropertiesWithHighestFrameRate.ElementAt(i);

                    if (selectedRecordingVideoWidth > MaximumVideoWidth ||
                        (currentProperties.Width <= MaximumVideoWidth && currentProperties.Width > selectedRecordingVideoWidth))
                    {
                        selectedRecordingVideoEncodingProperties = currentProperties;
                        selectedRecordingVideoWidth = selectedRecordingVideoEncodingProperties.Width;
                    }
                }

                _hfrVideoEncodingProperties = selectedRecordingVideoEncodingProperties;
                await _videoDeviceController.SetMediaStreamPropertiesAsync(RecordMediaStreamType, selectedRecordingVideoEncodingProperties);

                VideoEncodingProperties previewVideoEncodingProperties =
                    CameraUtils.FindVideoEncodingProperties(
                        _videoDeviceController, PreviewMediaStreamType,
                        PreviewFrameRate, selectedRecordingVideoWidth, selectedRecordingVideoEncodingProperties.Height);

                System.Diagnostics.Debug.WriteLine("Highest framerate for recording is "
                    + CameraUtils.ResolveFrameRate(selectedRecordingVideoEncodingProperties)
                    + " frames per second with selected resolution of "
                    + selectedRecordingVideoWidth + "x" + selectedRecordingVideoEncodingProperties.Height
                    + "\nThe preview properties for viewfinder are "
                    + CameraUtils.ResolveFrameRate(previewVideoEncodingProperties) + " FPS and "
                    + previewVideoEncodingProperties.Width + "x" + previewVideoEncodingProperties.Height);

                await _videoDeviceController.SetMediaStreamPropertiesAsync(PreviewMediaStreamType, previewVideoEncodingProperties);
            }

            if (_videoDeviceController.WhiteBalanceControl.Supported)
            {
                await _videoDeviceController.WhiteBalanceControl.SetPresetAsync(ColorTemperaturePreset.Fluorescent);
            }

            Initialized = true;
            return Initialized;
        }

        public async Task<bool> StartAsync()
        {
            if (Initialized && !Started)
            {
                await StartPreviewAsync();
                await StartRecordingAsync();
            }

            return Started;
        }

        public async Task<bool> StartPreviewAsync()
        {
            if (Initialized && !PreviewStarted)
            {
                await MediaCapture.StartPreviewAsync();
                PreviewStarted = true;
            }

            return PreviewStarted;
        }

        public async Task<bool> StartRecordingAsync()
        {
            if (Initialized && PreviewStarted && !RecordingStarted)
            {
#if WINDOWS_PHONE_APP
                MediaEncodingProfile recordProfile = MediaEncodingProfile.CreateMp4(VideoEncodingQuality.HD720p);
                recordProfile.Video.FrameRate.Numerator = _hfrVideoEncodingProperties.FrameRate.Numerator;
                recordProfile.Video.FrameRate.Denominator = _hfrVideoEncodingProperties.FrameRate.Denominator;
                double factor = (double)(recordProfile.Video.FrameRate.Numerator) / (recordProfile.Video.FrameRate.Denominator * 4);
                recordProfile.Video.Bitrate = (uint)(recordProfile.Video.Width * recordProfile.Video.Height * factor);
                await MediaCapture.StartRecordToStreamAsync(recordProfile, _recordingStream);
#else
                MediaEncodingProfile recordProfile = MediaEncodingProfile.CreateMp4(VideoEncodingQuality.Auto);
                await MediaCapture.StartRecordToStreamAsync(recordProfile, _recordingStream);
#endif

                // Get camera's resolution
                VideoEncodingProperties resolution =
                    (VideoEncodingProperties)_videoDeviceController.GetMediaStreamProperties(MediaStreamType.VideoRecord);
                ResolutionWidth = (int)resolution.Width;
                ResolutionHeight = (int)resolution.Height;

                Messenger.SettingsChangedFlag = true;
                await MediaCapture.AddEffectAsync(RecordMediaStreamType, BufferTransformActivationId, Properties);

                RecordingStarted = true;
            }

            return RecordingStarted;
        }

        public async Task<bool> StopRecordingAsync()
        {
            if (RecordingStarted)
            {
                await MediaCapture.StopRecordAsync();
                RecordingStarted = false;
            }

            return RecordingStarted;
        }

        public async void DisposeAsync()
        {
            if (Initialized && MediaCapture != null)
            {
                await MediaCapture.StopRecordAsync();
                await MediaCapture.StopPreviewAsync();
                MediaCapture.Dispose();
                MediaCapture = null;
                PreviewStarted = false;
                RecordingStarted = false;
                Initialized = false;
            }
        }

        /// <summary>
        /// Sets the target YUV values for the effect.
        /// </summary>
        /// <param name="yuv">The new target YUV.</param>
        public void SetYuv(byte[] yuv)
        {
            App.Settings.TargetColor = ImageProcessingUtils.YuvToColor(yuv);
            Messenger.SettingsChangedFlag = true;
        }

        /// <summary>
        /// Sets the given threshold value for the effect.
        /// </summary>
        /// <param name="threshold">The new threshold value.</param>
        /// <returns>True, if successful. False otherwise.</returns>
        public bool SetThreshold(float threshold)
        {
            if (Initialized && threshold >= MinimumThreshold && threshold <= MaximumThreshold)
            {
                App.Settings.Threshold = threshold;
                Messenger.SettingsChangedFlag = true;
                return true;
            }

            return false;
        }

        /// <summary>
        /// Sets the given ISO speed.
        /// </summary>
        /// <param name="isoSpeedPreset"></param>
        /// <returns>True, if successful.</returns>
        public async Task<bool> SetIsoSpeedAsync(IsoSpeedPreset isoSpeedPreset)
        {
            bool success = false;

            if (IsIsoSupported && isoSpeedPreset != IsoSpeedPreset)
            {
                await _videoDeviceController.IsoSpeedControl.SetPresetAsync(isoSpeedPreset);
                success = true;
            }

            return success;
        }

        /// <summary>
        /// Sets the given exposure value.
        /// </summary>
        /// <param name="exposureStep"></param>
        /// <returns>True, if successful.</returns>
        public async Task<bool> SetExposureAsync(int exposureStep)
        {
            bool success = false;

            if (IsExposureSupported && _exposureStep != exposureStep)
            {
                _exposureStep = exposureStep;
                TimeSpan minExposure = _videoDeviceController.ExposureControl.Min;
                IsAutoExposureOn = false;

                if (_exposureStep == -1) // Automatic exposure time
                {
                    IsAutoExposureOn = true;

#if WINDOWS_PHONE_APP    
                    await AdjustExposureAsync(0);
#else
                    await _videoDeviceController.ExposureControl.SetAutoAsync(true);
#endif
                }
                else
                {
                    await _videoDeviceController.ExposureControl.SetAutoAsync(false);
                    await _videoDeviceController.ExposureControl.SetValueAsync(
                        minExposure + TimeSpan.FromTicks(minExposure.Ticks * _exposureStep));
                }

                success = true;
            }

            return success;
        }

#if WINDOWS_PHONE_APP
        /// <summary>
        /// Adjusts the exposure.
        /// </summary>
        /// <param name="average"></param>
        /// <returns>True, if successful.</returns>
        private async Task<bool> AdjustExposureAsync(double average)
        {
            bool success = false;

            if (IsExposureSupported)
            {
                double illuminance = 0;
                double exposureAdjusted = 0;
                var min = _videoDeviceController.ExposureControl.Min;

                double brightness = 0;
                _videoDeviceController.Brightness.TryGetValue(out brightness);

                if (average == 0 && _lightSensor != null)
                {
                    var reading = _lightSensor.GetCurrentReading();
                    illuminance = reading.IlluminanceInLux;
                }
                else
                {
                    illuminance = average;
                }

                /*
                 * 50, inside. light only from windows
                 * 100, inside. lighs on
                 * 500...1000, outside 
                 */
                if (illuminance < 1000)
                {
                    exposureAdjusted = 25 - (illuminance / 1000) * 25;
                }
                else
                {
                    exposureAdjusted = 0;
                }

                await _videoDeviceController.ExposureControl.SetAutoAsync(false);
                await _videoDeviceController.ExposureControl.SetValueAsync(
                    min + TimeSpan.FromTicks((long)((double)min.Ticks * exposureAdjusted)));

                IsLuminanceShown = true;

                string luminanceAndExposure =
                    "Luminance: " + ((int)illuminance).ToString()
                    + "\nExposure: " + ((int)exposureAdjusted).ToString();

                if (ShowMessageRequest != null)
                {
                    ShowMessageRequest(this, luminanceAndExposure);
                }

                _lastLightMeasurement = exposureAdjusted;
                success = true;
            }

            return success;
        }
#endif

        /// <summary>
        /// Starts/stops the effect.
        /// </summary>
        /// <returns>True, if the effect was set on. False, if it was turned off.</returns>
        public async Task<bool> ToggleEffectAsync()
        {
            if (Started)
            {
                if (EffectSet)
                {
                    await MediaCapture.ClearEffectsAsync(PreviewMediaStreamType);
                    StateManager.State = VideoEffectState.Idle;
                }
                else
                {
                    Messenger.SettingsChangedFlag = true;
                    Messenger.ModeChangedFlag = true;
                    await MediaCapture.AddEffectAsync(PreviewMediaStreamType, RealtimeTransformActivationId, Properties);
                    StateManager.State = VideoEffectState.Locking;
                }
            }

            return EffectSet;
        }

        public void OnModeChanged(object sender, Mode e)
        {
            Messenger.ModeChangedFlag = true;
        }

        internal void OnRemoveNoiseChanged(object sender, bool e)
        {
            Messenger.SettingsChangedFlag = true;
        }

        internal void OnApplyEffectOnlyChanged(object sender, bool e)
        {
            Messenger.SettingsChangedFlag = true;
        }

        public async void OnIsoSettingsChangedAsync(object sender, IsoSpeedPreset e)
        {
            await SetIsoSpeedAsync(e);
        }

        public async void OnExposureSettingsChangedAsync(object sender, int e)
        {
            await SetExposureAsync(e);
        }

        private async void OnStateChanged(VideoEffectState newState, VideoEffectState oldState)
        {
            switch (newState)
            {
                case VideoEffectState.Idle:
                    await MediaCapture.ClearEffectsAsync(PreviewMediaStreamType);
                    break;
                case VideoEffectState.Locked:
                case VideoEffectState.PostProcess:
                    break;
            }
        }

#if WINDOWS_PHONE_APP
        private async void OnLightSensorReadingChangedAsync(LightSensor sender, LightSensorReadingChangedEventArgs args)
        {
            if (IsAutoExposureOn)
            {
                if (_lightMeasurements.Count > 10)
                {
                    double average = _lightMeasurements.Average();
                    double delta = Math.Max(average, _lastLightMeasurement) - Math.Min(average, _lastLightMeasurement);

                    if (Math.Max(average, _lastLightMeasurement) / delta > 0.3)  // 30% change
                    {
                        await AdjustExposureAsync(average);
                    }

                    _lightMeasurements.Clear();
                }
                else
                {
                    _lightMeasurements.Add(args.Reading.IlluminanceInLux);
                }
            }
        }
#endif
    }
}
