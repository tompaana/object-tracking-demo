/*
 * Copyright (c) 2014-2015 Microsoft Mobile
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading.Tasks;
using Windows.ApplicationModel.Activation;
using Windows.Graphics.Imaging;
using Windows.Storage;
using Windows.Storage.Pickers;
using Windows.Storage.Streams;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Media.Imaging;

namespace ObjectTrackingDemo
{
    public class ImageData : IDisposable
    {
        public MemoryStream ImageMemoryStream
        {
            get;
            set;
        }

        public int ImageWidthInPixels
        {
            get;
            set;
        }

        public int ImageHeightInPixels
        {
            get;
            set;
        }

        public void Initialize()
        {
            Dispose();
            ImageMemoryStream = new MemoryStream();
        }

        public void SeekStreamToBeginning()
        {
            if (ImageMemoryStream != null)
            {
                ImageMemoryStream.Seek(0, SeekOrigin.Begin);
            }
        }

        public void Dispose()
        {
            if (ImageMemoryStream != null)
            {
                ImageMemoryStream.Dispose();
            }

            ImageWidthInPixels = 0;
            ImageHeightInPixels = 0;
        }
    }

    /// <summary>
    /// A utility class for loading and saving image files to the file system.
    /// </summary>
    public class ImageFileManager
    {
        private const string DebugTag = "ImageFileManager: ";
        private const string KeyOperation = "Operation";
        private const string SelectImageOperationName = "SelectImage";
        private const string SelectDestinationOperationName = "SelectDestination";
        private const string JpegFileTypeDescription = "JPEG file";
        private readonly string[] _supportedImageFilePostfixes = { ".jpg", ".jpeg", ".png" };
        private readonly List<string> _supportedSaveImageFilePostfixes = new List<string> { ".jpg" };

        public event EventHandler<bool> ImageFileLoadedResult;
        public event EventHandler<bool> ImageFileSavedResult;

        private WriteableBitmap _writeableBitmapToSave;
        private IBuffer _imageBufferToSave;

        private static ImageFileManager _instance;
        public static ImageFileManager Instance
        {
            get
            {
                if (_instance == null)
                {
                    _instance = new ImageFileManager();
                }

                return _instance;
            }
        }

        public ImageData ImageData
        {
            get;
            private set;
        }

        private ImageFileManager()
        {
            ImageData = new ImageData();
        }

        /// <summary>
        /// 
        /// </summary>
        /// <returns></returns>
        public async Task<bool> GetImageFileAsync()
        {
            bool success = false;

            var picker = new FileOpenPicker
            {
                SuggestedStartLocation = PickerLocationId.PicturesLibrary,
                ViewMode = PickerViewMode.Thumbnail
            };

            // Filter to include a sample subset of file types
            picker.FileTypeFilter.Clear();

            foreach (string postfix in _supportedImageFilePostfixes)
            {
                picker.FileTypeFilter.Add(postfix);
            }

#if WINDOWS_PHONE_APP
            picker.ContinuationData[KeyOperation] = SelectImageOperationName;
            picker.PickSingleFileAndContinue();
            success = true;
#else
            Windows.Storage.StorageFile file = await picker.PickSingleFileAsync();

            if (file != null)
            {
                success = await HandleSelectedImageFileAsync(file);
            }
#endif
            return success;
        }

        /// <summary>
        /// Saves the given writeable bitmap to a file.
        /// </summary>
        /// <param name="writeableBitmap">The writeable bitmap to save.</param>
        /// <param name="filenamePrefix">The desired prefix for the filename.</param>
        /// <returns>True if successful, false otherwise.</returns>
        public async Task<bool> SaveImageFileAsync(WriteableBitmap writeableBitmap, string filenamePrefix)
        {
            _writeableBitmapToSave = writeableBitmap;
            return await SaveImageFileAsync(filenamePrefix);
        }

        /// <summary>
        /// Saves the given image buffer to a file.
        /// </summary>
        /// <param name="imageBuffer"></param>
        /// <param name="filenamePrefix">The desired prefix for the filename.</param>
        /// <returns>True if successful, false otherwise.</returns>
        public async Task<bool> SaveImageFileAsync(IBuffer imageBuffer, string filenamePrefix)
        {
            _imageBufferToSave = imageBuffer;
            return await SaveImageFileAsync(filenamePrefix);
        }

#if WINDOWS_PHONE_APP
        public async void ContinueFileOpenPickerAsync(FileOpenPickerContinuationEventArgs args)
        {
            System.Diagnostics.Debug.WriteLine(DebugTag + "ContinueFileOpenPicker()");
            bool success = false;

            if (args.Files == null
                || args.Files.Count == 0
                || args.Files[0] == null
                || (args.ContinuationData[KeyOperation] as string) != SelectImageOperationName)
            {
                System.Diagnostics.Debug.WriteLine(DebugTag + "ContinueFileOpenPicker(): Invalid arguments!");
            }
            else
            {
                StorageFile file = args.Files[0];
                success = await HandleSelectedImageFileAsync(file);
            }

            NotifyLoadedResult(success);
            App.ContinuationManager.MarkAsStale();
        }

        public async void ContinueFileSavePickerAsync(FileSavePickerContinuationEventArgs args)
        {
            System.Diagnostics.Debug.WriteLine(DebugTag + "ContinueFileSavePicker()");
            bool success = false;
            StorageFile file = args.File;

            if (file != null && (args.ContinuationData[KeyOperation] as string) == SelectDestinationOperationName)
            {
                success = await SaveImageFileAsync(file);
            }
            else
            {
                _writeableBitmapToSave = null;
                _imageBufferToSave = null;
            }

            NotifySavedResult(success);
            App.ContinuationManager.MarkAsStale();
        }
#endif

        /// <summary>
        /// Reads the given image file and writes it to the buffers while also
        /// scaling a preview image.
        /// 
        /// Note that this method can't handle null argument!
        /// </summary>
        /// <param name="file">The selected image file.</param>
        /// <returns>True if successful, false otherwise.</returns>
        private async Task<bool> HandleSelectedImageFileAsync(StorageFile file)
        {
            System.Diagnostics.Debug.WriteLine(DebugTag + "HandleSelectedImageFileAsync(): " + file.Name);
            var fileStream = await file.OpenAsync(Windows.Storage.FileAccessMode.Read);

            ImageData.Initialize();

            var image = new BitmapImage();
            image.SetSource(fileStream);
            ImageData.ImageWidthInPixels = image.PixelWidth;
            ImageData.ImageHeightInPixels = image.PixelHeight;

            bool success = false;

            try
            {
                // JPEG images can be used as such
                Stream stream = fileStream.AsStream();
                stream.Position = 0;
                stream.CopyTo(ImageData.ImageMemoryStream);
                stream.Dispose();
                success = true;
            }
            catch (Exception e)
            {
                System.Diagnostics.Debug.WriteLine(DebugTag
                    + "Cannot use stream as such (not probably in JPEG format): " + e.Message);
            }

            if (!success)
            {
                try
                {
                    await ImageProcessingUtils.FileStreamToJpegStreamAsync(fileStream,
                        ImageData.ImageWidthInPixels,
                        ImageData.ImageHeightInPixels,
                        (IRandomAccessStream)ImageData.ImageMemoryStream.AsInputStream());

                    success = true;
                }
                catch (Exception e)
                {
                    System.Diagnostics.Debug.WriteLine(DebugTag
                        + "Failed to convert the file stream content into JPEG format: "
                        + e.ToString());
                }
            }

            fileStream.Dispose();
            ImageData.SeekStreamToBeginning();
            return success;
        }

        /// <summary>
        /// Initiates saving the previously set image data to a file.
        /// </summary>
        /// <param name="filenamePrefix">The desired prefix for the filename.</param>
        /// <returns>True if successful, false otherwise.</returns>
        private async Task<bool> SaveImageFileAsync(string filenamePrefix)
        {
            bool success = false;

            var picker = new FileSavePicker
            {
                SuggestedStartLocation = PickerLocationId.PicturesLibrary
            };

            picker.FileTypeChoices.Add(JpegFileTypeDescription, _supportedSaveImageFilePostfixes);
            picker.SuggestedFileName = filenamePrefix + FormattedCurrentDateTimeString() + _supportedSaveImageFilePostfixes[0];
            System.Diagnostics.Debug.WriteLine(DebugTag + "SaveImageFileAsync(): Suggested filename is " + picker.SuggestedFileName);

#if WINDOWS_PHONE_APP
            picker.ContinuationData[KeyOperation] = SelectDestinationOperationName;
            picker.PickSaveFileAndContinue();
            success = true;
#else
            StorageFile file = await picker.PickSaveFileAsync();

            if (file != null)
            {
                success = await SaveImageFileAsync(file);
            }
            else
            {
                _writeableBitmapToSave = null;
                _imageBufferToSave = null;
            }
#endif
            return success;
        }

        /// <summary>
        /// Saves the local image buffer to the the given file.
        /// </summary>
        /// <param name="file">The file where to save the image buffer.</param>
        /// <returns>True if successful, false otherwise.</returns>
        private async Task<bool> SaveImageFileAsync(StorageFile file)
        {
            bool success = false;

            try
            {
                using (IRandomAccessStream fileStream = await file.OpenAsync(FileAccessMode.ReadWrite))
                {
                    if (_imageBufferToSave != null)
                    {
                        await fileStream.WriteAsync(_imageBufferToSave);
                        await fileStream.FlushAsync();
                    }
                    else if (_writeableBitmapToSave != null)
                    {
                        Guid bitmapEncoderGuid = BitmapEncoder.JpegEncoderId;
                        BitmapEncoder encoder = await BitmapEncoder.CreateAsync(bitmapEncoderGuid, fileStream);
                        Stream pixelStream = _writeableBitmapToSave.PixelBuffer.AsStream();
                        byte[] pixelArray = new byte[pixelStream.Length];
                        await pixelStream.ReadAsync(pixelArray, 0, pixelArray.Length);

                        encoder.SetPixelData(BitmapPixelFormat.Bgra8, BitmapAlphaMode.Ignore,
                                            (uint)_writeableBitmapToSave.PixelWidth,
                                            (uint)_writeableBitmapToSave.PixelHeight,
                                            96.0,
                                            96.0,
                                            pixelArray);
                        await encoder.FlushAsync();
                    }
                    else
                    {
                        throw new Exception("No data to save");
                    }
                }

                success = true;
            }
            catch (Exception)
            {
            }

            _writeableBitmapToSave = null;
            _imageBufferToSave = null;
            return success;
        }

        /// <summary>
        /// Notifies possible listeners about the result of the image file
        /// loading operation.
        /// </summary>
        /// <param name="wasSuccessful">True if the result was a success, false otherwise.</param>
        private void NotifyLoadedResult(bool wasSuccessful)
        {
            EventHandler<bool> handler = ImageFileLoadedResult;

            if (handler != null)
            {
                handler(this, wasSuccessful);
            }
        }

        /// <summary>
        /// Notifies possible listeners about the result of the image file
        /// saving operation.
        /// </summary>
        /// <param name="wasSuccessful">True if the result was a success, false otherwise.</param>
        private void NotifySavedResult(bool wasSuccessful)
        {
            EventHandler<bool> handler = ImageFileSavedResult;

            if (handler != null)
            {
                handler(this, wasSuccessful);
            }
        }

        private string FormattedCurrentDateTimeString()
        {
            return string.Format("{0:yyyy-mm-dd_Hmmss}", DateTimeOffset.Now);
        }
    }
}
