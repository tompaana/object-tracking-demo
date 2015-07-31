using System.Collections.Generic;
using Windows.Media.Capture;
using Windows.Media.Devices;
using Windows.Media.MediaProperties;

namespace ObjectTrackingDemo
{
    /// <summary>
    /// Implements helper methods for camera device.
    /// </summary>
    public class CameraUtils
    {
        /// <summary>
        /// Resolves the frame rate of the given video encoding properties.
        /// </summary>
        /// <param name="properties">The video encoding properties.</param>
        /// <returns>The frame rate of the given video encoding properties.</returns>
        public static uint ResolveFrameRate(VideoEncodingProperties properties)
        {
            uint frameRate = 0;

            if (properties != null)
            {
                frameRate = properties.FrameRate.Numerator / properties.FrameRate.Denominator;
            }

            return frameRate;
        }

        /// <summary>
        /// Resolves the highest frame rate available if the given video device controller
        /// for the given media stream type.
        /// </summary>
        /// <param name="controller">The video device controller.</param>
        /// <param name="mediaStreamType">The media stream type.</param>
        /// <returns>The highest frame rate available.</returns>
        public static uint ResolveHighestFrameRate(
            VideoDeviceController controller, MediaStreamType mediaStreamType)
        {
            uint highestFrameRate = 0;

            if (controller != null)
            {
                IReadOnlyList<IMediaEncodingProperties> availableProperties =
                    controller.GetAvailableMediaStreamProperties(mediaStreamType);

                foreach (IMediaEncodingProperties properties in availableProperties)
                {
                    VideoEncodingProperties videoEncodingProperties = properties as VideoEncodingProperties;

                    if (videoEncodingProperties != null)
                    {
                        uint frameRate = ResolveFrameRate(videoEncodingProperties);

                        if (frameRate > highestFrameRate)
                        {
                            highestFrameRate = frameRate;
                        }
                    }
                }
            }

            return highestFrameRate;
        }

        /// <summary>
        /// Resolves an instance of video encoding properties with the highest frame rate available
        /// matching the given media stream type.
        /// </summary>
        /// <param name="controller">The video device controller.</param>
        /// <param name="mediaStreamType">The media stream type.</param>
        /// <returns>An instance of video encoding properties with the highest frame rate available.</returns>
        public static VideoEncodingProperties ResolveVideoEncodingPropertiesWithHighestFrameRate(
            VideoDeviceController controller, MediaStreamType mediaStreamType)
        {
            VideoEncodingProperties propertiesWithHighestFrameRate = null;

            if (controller != null)
            {
                IReadOnlyList<IMediaEncodingProperties> availableProperties =
                    controller.GetAvailableMediaStreamProperties(mediaStreamType);
                uint highestFrameRate = 0;

                foreach (IMediaEncodingProperties properties in availableProperties)
                {
                    VideoEncodingProperties videoEncodingProperties = properties as VideoEncodingProperties;

                    if (videoEncodingProperties != null)
                    {
                        uint frameRate = ResolveFrameRate(videoEncodingProperties);

                        if (frameRate > highestFrameRate)
                        {
                            propertiesWithHighestFrameRate = videoEncodingProperties;
                            highestFrameRate = frameRate;
                        }
                    }
                }
            }

            return propertiesWithHighestFrameRate;
        }

        /// <summary>
        /// Resolves all the instances of video encoding properties with the highest frame rate
        /// available matching the given media stream type.
        /// </summary>
        /// <param name="controller">The video device controller.</param>
        /// <param name="mediaStreamType">The media stream type.</param>
        /// <returns>All the instances of video encoding properties with the highest frame rate available.</returns>
        public static IList<VideoEncodingProperties> ResolveAllVideoEncodingPropertiesWithHighestFrameRate(
            VideoDeviceController controller, MediaStreamType mediaStreamType)
        {
            uint highestFrameRate = ResolveHighestFrameRate(controller, mediaStreamType);
            IList<VideoEncodingProperties> listOfPropertiesWithHighestFrameRate = null;

            if (highestFrameRate > 0)
            {
                listOfPropertiesWithHighestFrameRate = new List<VideoEncodingProperties>();
                IReadOnlyList<IMediaEncodingProperties> availableProperties =
                    controller.GetAvailableMediaStreamProperties(mediaStreamType);

                foreach (IMediaEncodingProperties properties in availableProperties)
                {
                    VideoEncodingProperties videoEncodingProperties = properties as VideoEncodingProperties;

                    if (videoEncodingProperties != null)
                    {
                        uint frameRate = ResolveFrameRate(videoEncodingProperties);

                        if (frameRate == highestFrameRate)
                        {
                            listOfPropertiesWithHighestFrameRate.Add(videoEncodingProperties);
                        }
                    }
                }
            }

            return listOfPropertiesWithHighestFrameRate;
        }

        /// <summary>
        /// Resolves a video encoding properties instance based on the given arguments.
        /// </summary>
        /// <param name="controller">The video device controller.</param>
        /// <param name="mediaStreamType">The media stream type.</param>
        /// <param name="frameRate">The desired framerate.</param>
        /// <param name="width">The desired width in pixels.</param>
        /// <param name="height">The desired height in pixels.</param>
        /// <returns>A video encoding properties instance matching the given arguments or null if not found.</returns>
        public static VideoEncodingProperties FindVideoEncodingProperties(
            VideoDeviceController controller, MediaStreamType mediaStreamType,
            uint frameRate, uint width, uint height)
        {
            VideoEncodingProperties matchingProperties = null;

            if (controller != null)
            {
                IReadOnlyList<IMediaEncodingProperties> availableProperties =
                    controller.GetAvailableMediaStreamProperties(mediaStreamType);

                foreach (IMediaEncodingProperties properties in availableProperties)
                {
                    VideoEncodingProperties videoEncodingProperties = properties as VideoEncodingProperties;

                    if (videoEncodingProperties != null)
                    {
                        if (ResolveFrameRate(videoEncodingProperties) == frameRate
                            && videoEncodingProperties.Width == width
                            && videoEncodingProperties.Height == height)
                        {
                            matchingProperties = videoEncodingProperties;
                            break;
                        }
                    }
                }
            }

            return matchingProperties;
        }

		/// <summary>
		/// Resolves the recording video format of the camera device.
        /// </summary>
		/// <param name="mediaCapture">The media capture instance.</param>
		/// <returns>The recording video format as string.</returns>
		public static string ResolveVideoRecordFormat(MediaCapture mediaCapture)
        {
            VideoEncodingProperties videoEncodingProperties =
                (VideoEncodingProperties)mediaCapture.VideoDeviceController.GetMediaStreamProperties(MediaStreamType.VideoRecord);
            string subType = videoEncodingProperties.Subtype;
            System.Diagnostics.Debug.WriteLine("ResolveRecordingVideoFormat: " + subType);
            return subType;
        }
    }
}
