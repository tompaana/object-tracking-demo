# Object Tracking Demo #

**New in 2015-07-31 update**

* **Chroma delta** and **edge detection** modes, which do not really do much,
  but display the algorithms in real-time.
* Cleaned up code and improved project structure.
* Camera settings for ISO and exposure added (availability based on device).


## Description ##

This is a Windows universal application demonstrating how to track objects in a
feed provided by camera.

![Screenshot 2](https://raw.githubusercontent.com/tompaana/object-tracking-demo/master/Doc/OTDScreenshot3Small.png)

**Supported platforms:** Windows 8.1 (WinRT), Windows Phone 8.1, Windows Store Apps


## Disclaimer ##

You know how they say you shouldn't start a speech with an apology (because it
drops the interest of any listener even before you start). Well, that true.
However, I'm *sorry*, but:

   **No, this is not a feature complete, 100 % verified and tested demo.** It
   started as a test, a proof-of-concept if you will, and it hasn't evolved
   further although it roughly does what it was meant to do. There's still a
   long way to go so that we could say it even functions on 50 % capacity.
   That said, it still provides some rudimentary architecture, which makes
   sense and you are free to utilize that should you want to implement something
   similar. It provides a set of different, reusable methods for image
   processing and analysis. And every bit is yours to keep - unless you're
   planning some evil scheme of world domination or something.


## What does it do? ##

This:

![Object motion captured](https://raw.githubusercontent.com/tompaana/object-tracking-demo/master/Doc/ObjectMotionCapturedScaled.png)

The features of the demo are:

* Locking to an object/area in the video feed based on the given color and
  threshold
* After locking to a target, the demo will start recording *N* number of frames
  into a ring buffer for later analysis
* When the object displacement exceeds set parameters (the thing moves), it will
  trigger a post-processing operation, which will try to deduce, using the
  frames in the buffer, where the object went
  

## How do I build/compile it? ##

Very easily. If you are targeting a (physical) Windows Phone:

1. Set `ObjectTrackingDemo.WindowsPhone` as **StartUp** project
2. Make sure the build configuration is set to **ARM** (and **Device** instead of emulator)
3. Select **BUILD -> Build Solution**, **BUILD -> Deploy Solution** or click **Device** to run on device

If you are targeting big Windows (e.g. laptop):

1. Set `ObjectTrackingDemo.Windows` as **StartUp** project
2. Make sure the build configuration is set to **x86**
3. Select **BUILD -> Build Solution**, **BUILD -> Deploy Solution** or click **Local machine** (can be something else too) to run on device


## How do I use it? ##

**Chroma filter mode**

**Take. It. Slow.** When you start, select the color of the object by tapping
the viewfinder. The app will select the color in the point tapped. To fine-tune
the color, tap the color selection button (which also displays the selected
color). Start with very **low threshold value** and increase it little by little
as needed until the app recognizes the desired object. Note that you need to
tap the magic wand icon to start the video engine (the effect).

If the color you are targeting is really distinctive (does not appear in the
background), you can try to exaggerate the color selection and use a higher
threshold value.

If you use too high threshold, especially on the phone, the app may feel like
it has stopped responding. This is because there is too much data sent for
further processing.

**Chroma delta mode**

Select a low threshold value and increase. This mode does not really do
anything (yet), but displays the changes in the consecutive frames i.e. if the
camera is stable and nothing is moving in the field of view, nothing is shown
on the viewfinder.

**Edge detection**

Select a low threshold value and increase. This mode also does not do anything
special yet, but displays the edges of objects based on really, really simple
gradient calculation.


## How does it work? ##

The functionality and methods used are explained in the following articles
on high level:

* [Tracking Objects from Video Feed](http://tomipaananen.azurewebsites.net/?p=361) ([Part I](http://tomipaananen.azurewebsites.net/?p=361), [Part II](http://tomipaananen.azurewebsites.net/?p=481) and [Part III](http://tomipaananen.azurewebsites.net/?p=581))
* [Buffering Video Frames](http://juhana.cloudapp.net/?p=181) (about using ring/circular buffers)


### Few words on architecture ###

The solution has two separate projects:
[ObjectTrackingDemo](https://github.com/tompaana/object-tracking-demo/tree/master/ObjectTrackingDemo/ObjectTrackingDemo.Shared),
which is the UI project written with XAML and C#, and
[VideoEffect](https://github.com/tompaana/object-tracking-demo/tree/master/VideoEffect/VideoEffect.Shared),
a native component, written with C++, that does the heavy lifting.

**Important classes of `VideoEffect` project**

* Image Processing:
 * [ImageAnalyzer](https://github.com/tompaana/object-tracking-demo/blob/master/VideoEffect/VideoEffect.Shared/ImageProcessing/ImageAnalyzer.h): Utilizes the methods provided by `ImageProcessingUtils` for higher level image analysis.
 * [ImageProcessingUtils](https://github.com/tompaana/object-tracking-demo/blob/master/VideoEffect/VideoEffect.Shared/ImageProcessing/ImageProcessingUtils.h): Provides the basic methods for chroma filtering, mapping objects from binary image, creating convex hulls etc.

* Effects:
 * [ChromaDeltaEffect](https://github.com/tompaana/object-tracking-demo/blob/master/VideoEffect/VideoEffect.Shared/Effects/ChromaDeltaEffect.h): Compares the two consecutive frames and highlights the pixels, which have changed (based on a threshold value).
 * [ChromaFilterEffect](https://github.com/tompaana/object-tracking-demo/blob/master/VideoEffect/VideoEffect.Shared/Effects/ChromaFilterEffect.h): Highlights pixels, which are close enough, determined by the set threshold value, to the set target luma/chroma values.
 * [EdgeDetectionEffect](https://github.com/tompaana/object-tracking-demo/blob/master/VideoEffect/VideoEffect.Shared/Effects/EdgeDetectionEffect.h): Highlights the pixels, which appear to be an edge based on the calculated, simple gradient value.

* Transforms:
 * [BufferTransform](https://github.com/tompaana/object-tracking-demo/blob/master/VideoEffect/VideoEffect.Shared/Transforms/BufferTransform.h): Runs the ring buffer and handles the detection of the object's destination i.e. where did the object go.
 * [RealtimeTransform](https://github.com/tompaana/object-tracking-demo/blob/master/VideoEffect/VideoEffect.Shared/Transforms/RealtimeTransform.h): Manages finding the object, when stationary, locking to it and detecting the moment when the object displacement occurs i.e. when the object moves from its starting position.

The image below describes the workloads of the two video effects based on the
application state. Red color signifies heavy CPU load whereas green means low
CPU load.

![Effect workloads based on states](https://raw.githubusercontent.com/tompaana/object-tracking-demo/master/Doc/EffectWorkloadsBasedOnState.png)


## Known issues ##

* Phone build: The app is sometimes really slow or even jams. The root cause is
  the object mapping algorithm, which does not handle certain kind of data well.
  To manage the problem use as low threshold value as possible.
* The app crashes often :(
* The solution will not compile with Community 2015 RC version of Visual Studio.
