using System;
using Windows.Graphics.Display;

namespace ObjectTrackingDemo
{
    public class DisplayUtils
    {
        /// <summary>
        /// Resolves the screen resolution.
        /// </summary>
        /// <param name="widthInPixels">The width of the resolution in pixels.</param>
        /// <param name="heightInPixels">The height of the resolution in pixels.</param>
        public void ResolveScreenResolution(out double widthInPixels, out double heightInPixels)
        {
            DisplayInformation displayInformation = DisplayInformation.GetForCurrentView();
            double rawPixelsPerViewPixel = 1.0d;

#if WINDOWS_PHONE_APP
            rawPixelsPerViewPixel = displayInformation.RawPixelsPerViewPixel;
#else
            switch (displayInformation.ResolutionScale)
            {
                case ResolutionScale.Scale100Percent:
                    rawPixelsPerViewPixel = 1.0d;
                    break;
                case ResolutionScale.Scale120Percent:
                    rawPixelsPerViewPixel = 1.2d;
                    break;
                case ResolutionScale.Scale140Percent:
                    rawPixelsPerViewPixel = 1.4d;
                    break;
                case ResolutionScale.Scale150Percent:
                    rawPixelsPerViewPixel = 1.5d;
                    break;
                case ResolutionScale.Scale160Percent:
                    rawPixelsPerViewPixel = 1.6d;
                    break;
                case ResolutionScale.Scale180Percent:
                    rawPixelsPerViewPixel = 1.8d;
                    break;
                case ResolutionScale.Scale225Percent:
                    rawPixelsPerViewPixel = 2.25d;
                    break;
            }
#endif

            double boundsWidth = Windows.ApplicationModel.Core.CoreApplication.MainView.CoreWindow.Bounds.Width;
            double boundsHeight = Windows.ApplicationModel.Core.CoreApplication.MainView.CoreWindow.Bounds.Height;
            widthInPixels = Math.Round(boundsWidth * rawPixelsPerViewPixel, 0);
            heightInPixels = Math.Round(boundsHeight * rawPixelsPerViewPixel, 0);
            System.Diagnostics.Debug.WriteLine("Screen resolution is " + widthInPixels + "x" + heightInPixels);
        }

        /// <summary>
        /// Resolves the display size of the device running this app.
        /// </summary>
        /// <returns>The display size in inches or less than zero if unable to resolve.</returns>
        public double ResolveDisplaySizeInInches()
        {
            double displaySize = -1d;

            double screenResolutionX = 0d;
            double screenResolutionY = 0d;
            ResolveScreenResolution(out screenResolutionX, out screenResolutionY);

            DisplayInformation displayInformation = DisplayInformation.GetForCurrentView();
            float rawDpiX = displayInformation.RawDpiX;
            float rawDpiY = displayInformation.RawDpiY;

            if (rawDpiX > 0 && rawDpiY > 0)
            {
                displaySize = Math.Sqrt(
                    Math.Pow(screenResolutionX / rawDpiX, 2) +
                    Math.Pow(screenResolutionY / rawDpiY, 2));
                displaySize = Math.Round(displaySize, 1); // One decimal is enough
            }

            return displaySize;
        }
    }
}
