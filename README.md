# Object Tracking Demo #

## Description ##

This is a Windows universal application demonstrating how to track objects in a
feed provided by camera.

![Screenshot 2](https://raw.githubusercontent.com/tompaana/object-tracking-demo/master/Doc/OTDScreenshot3Small.png)

## Disclaimer ##

You know how they say you shouldn't start a speech with an apology (because it
drops the interest of any listener even before you start). Well, that true.
However, I'm *sorry*, but:

1. **No, this is not an example how you write good code.** It's a project with a
   lot of bubble gum to keep of, whatever it does, not totally falling apart.
   And often, it falls apart i.e. crashes are eminent. Hell, it might even break
   your device, so be careful with it, ok? It might get better with time
   (patches), but we will not promise anything.
2. **No, this is not a feature complete, 100 % verified and tested demo.** It
   started as a test, a proof-of-concept if you will, and it hasn't evolved
   further although it roughly does what it was meant to do. There's still a
   long way to go so that we could say it even functions on 50 % capacity.
   That said, it still provides some rudimentary architecture, which makes
   sense and you are free to utilize that should you want to implement something
   similar. It provides a set of different, reusable methods for image
   processing and analysis. And every bit is yours to keep - unless you're
   planning some evil scheme of world domination or something.

## What does it do? ##

![Object motion captured](https://raw.githubusercontent.com/tompaana/object-tracking-demo/master/Doc/ObjectMotionCapturedScaled.png)

## How do I use it? ##

**Take. It. Slow.** When you start, select the color of the object by tapping
the viewfinder. The app will select the color in the point tapped. To fine-tune
the color, tap the color selection button (which also displays the selected
color). Start with very **low threshold value** and increase it little by little
as needed until the app recognizes the desired object. Note that you need to
tap the magic wand icon to start the video engine (the effect).

If you use too high threshold, especially on the phone, the app may feel like
it has stopped responding. This is because there is too much data sent for
further processing.

## How does it work? ##

[TODO]

![Effect workloads based on states](https://raw.githubusercontent.com/tompaana/object-tracking-demo/master/Doc/EffectWorkloadsBasedOnState.png)

## Where can I learn more? ##

[TODO]

## Known issues ##

* Phone build: The app is sometimes really slow or even jams. The root cause is
  the object mapping algorithm, which does not handle certain kind of data well.
  To manage the problem use as low threshold value as possible.
* The app crashes often.
