# Livector
A super simple vectorscope for you to try out

## Target system: WIN32

### Two threads are at work. 
- The first handles drawing 48000 points on screen at a fixed 240fps. 
- The second thread captures the default audio device buffer every 10ms and plots acording to the left and right channel. 

Currently normalized at 32bit depth (Works with lower depths but could become a bit small) 
and 48000 points array for 1 second of 48KHz playback (Will work with other sample rates). 