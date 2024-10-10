# Livector
A super simple vectorscope for you to try out

## Target system: WIN32

### Two threads are at work. 
- The first handles drawing 48000 points on screen at a fixed 240fps. 
- The second thread captures the default audio device buffer every 10ms and plots acording to the left and right channel. 

Currently normalized at 32bit depth (Works with lower depths but could become a bit small) 
and 48000 points array for 1 second of 48KHz playback (Will work with other sample rates). 

### Explore branches
- pointcloud: Was made to better emulate velocities through points instead of solid lines.
- travel_brightness: Is an attempt at setting the line brightness to its velocity, unfortunately it is very pourly optimized.
- waveform: As the name entails, this is a waveform (oscilloscope) implementation where the Y axis is amplitude and X axis is time.
