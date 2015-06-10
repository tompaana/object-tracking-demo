using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Enumeration;
using Windows.Foundation.Collections;
using Windows.Media.Capture;
using Windows.Media.Devices;
using Windows.Media.MediaProperties;
using Windows.Storage.Streams;
using VideoEffect;

namespace ObjectTrackingDemo
{
    public class VideoEngine
    {
        // Constants
        private const string EffectActivationId = "ObjectFinderEffectTransform.ObjectFinderEffectEffect";
        private const string BufferEffectActivationId = "BufferEffect.BufferEffect";
        private const string PropertyKeyThreshold = "Threshold";
        private const string PropertyKeyY = "PropertyY";
        private const string PropertyKeyU = "PropertyU";
        private const string PropertyKeyV = "PropertyV";
        private const string propertyKeyCommunication = "Communication";
        private readonly MediaStreamType PreviewMediaStreamType = MediaStreamType.VideoPreview;
        private readonly MediaStreamType RecordMediaStreamType = MediaStreamType.VideoRecord;
        private const uint PreviewFrameRate = 30;
        private const int MinimumThreshold = 0;
        private const int MaximumThreshold = 255;

        public event EventHandler<string> ShowMessageRequest;

        private InMemoryRandomAccessStream _recordingStream = new InMemoryRandomAccessStream();
        private StateManager _stateManager = new StateManager();
        private VideoDeviceController _videoDeviceController;
        private DeviceInformation _deviceInformation;
        private DeviceInformationCollection _deviceInformationCollection;
        private VideoEncodingProperties _hfrVideoEncodingProperties;
        private PropertySet _properties;
        private float[] _yuv;
        private bool _changingThreshold;

#if WINDOWS_APP
        private LowLagPhotoCapture _lowLagPhotoCapture;
#endif

        private static VideoEngine _instance;
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

        private MediaCapture _mediaCapture = null;
        public MediaCapture MediaCapture
        {
            get
            {
                return _mediaCapture;
            }
            private set
            {
                _mediaCapture = value;
            }
        }

        private VideoEffectMessenger _messenger;
        public VideoEffectMessenger Messenger
        {
            get
            {
                return _messenger;
            }
        }

        public StateManager StateManager
        {
            get
            {
                return _stateManager;
            }
        }

        public bool Flash
        {
            get
            {
                if (_videoDeviceController != null)
                {
                    return _videoDeviceController.FlashControl.Enabled;
                }

                return false;
            }
            set
            {
                if (_videoDeviceController != null && _videoDeviceController.FlashControl.Supported)
                {
                    try
                    {
                        _videoDeviceController.FlashControl.Enabled = value;
                    }
                    catch (Exception e)
                    {
                        System.Diagnostics.Debug.WriteLine("Cannot change flash settings: " + e.ToString());
                    }
                }
            }
        }

        public bool Torch
        {
            get
            {
                if (_videoDeviceController != null)
                {
                    try
                    {
                        return _videoDeviceController.TorchControl.Enabled;
                    }
                    catch (Exception)
                    {
                        return false;
                    }
                }

                return false;
            }
            set
            {
                if (_videoDeviceController != null && _videoDeviceController.TorchControl.Supported)
                {
                    try
                    {
                        _videoDeviceController.TorchControl.Enabled = value;
                    }
                    catch (Exception e)
                    {
                        System.Diagnostics.Debug.WriteLine("Cannot change torch settings: " + e.ToString());
                    }
                }
            }
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
                return _stateManager.State != VideoEffectState.Idle;
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

        private VideoEngine()
        {
            _yuv = new float[3]
            {
                128f, 128f, 128f
            };

            _messenger = new VideoEffectMessenger(_stateManager);

            _properties = new PropertySet();
            _properties[PropertyKeyThreshold] = 0f;
            _properties[PropertyKeyY] = _yuv[0];
            _properties[PropertyKeyU] = _yuv[1];
            _properties[PropertyKeyV] = _yuv[2];
            _properties[propertyKeyCommunication] = _messenger;

            _stateManager.StateChanged += OnStateChanged;
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="yuv"></param>
        /// <param name="g"></param>
        /// <param name="b"></param>
        public async void SetYUVAsync(byte[] yuv)
        {
            if (EffectSet)
            {
                await MediaCapture.ClearEffectsAsync(PreviewMediaStreamType);
            }

            _yuv[0] = (float)yuv[0];
            _yuv[1] = (float)yuv[1];
            _yuv[2] = (float)yuv[2];
            _properties[PropertyKeyY] = _yuv[0];
            _properties[PropertyKeyU] = _yuv[1];
            _properties[PropertyKeyV] = _yuv[2];
            _properties[propertyKeyCommunication] = _messenger;

            if (EffectSet)
            {
                await MediaCapture.AddEffectAsync(PreviewMediaStreamType, EffectActivationId, _properties);
            }
        }

        public async Task<bool> InitializeAsync()
        {
            if (!Initialized)
            {
                _deviceInformationCollection = await DeviceInformation.FindAllAsync(DeviceClass.VideoCapture);                
                _deviceInformation = null;

                if (_deviceInformationCollection.Count != 0)
                {
                    _deviceInformation = _deviceInformationCollection[0];
                }

                if (_deviceInformation != null)
                {
                    MediaCapture = new MediaCapture();
                    MediaCaptureInitializationSettings settings = new MediaCaptureInitializationSettings();
                    settings.StreamingCaptureMode = StreamingCaptureMode.Video;
                    settings.VideoDeviceId = _deviceInformation.Id;
                    await MediaCapture.InitializeAsync(settings);
                }

                _videoDeviceController = MediaCapture.VideoDeviceController;

                VideoEncodingProperties propertiesWithHighestFrameRate =
                    Utils.ResolveVideoEncodingPropertiesWithHighestFrameRate(_videoDeviceController, RecordMediaStreamType);

                if (propertiesWithHighestFrameRate != null)
                {
                    System.Diagnostics.Debug.WriteLine("Highest framerate is "
                        + Utils.ResolveFrameRate(propertiesWithHighestFrameRate) + " frames per second with resolution "
                        + propertiesWithHighestFrameRate.Width + "x" + propertiesWithHighestFrameRate.Height);

                    _hfrVideoEncodingProperties = propertiesWithHighestFrameRate;
                    await MediaCapture.VideoDeviceController.SetMediaStreamPropertiesAsync(RecordMediaStreamType, propertiesWithHighestFrameRate);

                    VideoEncodingProperties propertiesForPreview =
                        Utils.FindVideoEncodingProperties(
                            _videoDeviceController, PreviewMediaStreamType,
                            PreviewFrameRate, propertiesWithHighestFrameRate.Width, propertiesWithHighestFrameRate.Height);

                    await MediaCapture.VideoDeviceController.SetMediaStreamPropertiesAsync(PreviewMediaStreamType, propertiesForPreview);
                }

                Initialized = true;
            }

#if WINDOWS_PHONE_APP
            await MediaCapture.AddEffectAsync(RecordMediaStreamType, BufferEffectActivationId, _properties);
            await MediaCapture.VideoDeviceController.WhiteBalanceControl.SetPresetAsync(ColorTemperaturePreset.Fluorescent);
#endif // WINDOWS_PHONE_APP

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
#else // WINDOWS_PHONE_APP
                MediaEncodingProfile recordProfile = MediaEncodingProfile.CreateMp4(VideoEncodingQuality.Auto);
                await MediaCapture.StartRecordToStreamAsync(recordProfile, _recordingStream);
                _lowLagPhotoCapture = await MediaCapture.PrepareLowLagPhotoCaptureAsync(ImageEncodingProperties.CreateJpeg());
#endif // WINDOWS_PHONE_APP

                // Get camera's resolution
                VideoEncodingProperties resolution = (VideoEncodingProperties)MediaCapture.VideoDeviceController.GetMediaStreamProperties(MediaStreamType.VideoRecord);
                ResolutionWidth = (int)resolution.Width;
                ResolutionHeight = (int)resolution.Height;
           
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
#if WINDOWS_APP
                await _lowLagPhotoCapture.FinishAsync();
#else
                await MediaCapture.StopRecordAsync();
#endif
                await MediaCapture.StopPreviewAsync();
                MediaCapture.Dispose();
                MediaCapture = null;
                //EffectSet = false;
                PreviewStarted = false;
                RecordingStarted = false;
                Initialized = false;
            }
        }

#if WINDOWS_APP
        public async Task<CapturedPhoto> CapturePhotoAsync()
        {
            if (Initialized)
            {
                return await _lowLagPhotoCapture.CaptureAsync();
            }

            return null;
        }
#endif

        public async Task<bool> ToggleEffectAsync()
        {
            if (Started)
            {
                if (EffectSet)
                {
                    await MediaCapture.ClearEffectsAsync(PreviewMediaStreamType);
                    _stateManager.State = VideoEffectState.Idle;
                }
                else
                {
                    await MediaCapture.AddEffectAsync(PreviewMediaStreamType, EffectActivationId, _properties);
                    _stateManager.State = VideoEffectState.Locking;
                }
            }

            return EffectSet;
        }

        public async Task<bool> SetThresholdAsync(float threshold)
        {
            if (Initialized && !_changingThreshold && threshold >= MinimumThreshold && threshold <= MaximumThreshold)
            {
                System.Diagnostics.Debug.WriteLine("SetThresholdAsync: " + threshold);
                _changingThreshold = true;

                await MediaCapture.ClearEffectsAsync(PreviewMediaStreamType);
                _properties[PropertyKeyThreshold] = threshold;
                await MediaCapture.AddEffectAsync(PreviewMediaStreamType, EffectActivationId, _properties);

                _changingThreshold = false;
                //EffectSet = true;

                return true;
            }

            return false;
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
    }
}
