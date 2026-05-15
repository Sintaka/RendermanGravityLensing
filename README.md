# Gravity Lensing Shader via custom PxrVolume Integrator
After several months of study and development, I was finally able to reproduce the scenes in Interstellar.\
A Custom Volume Material with Renderman, it can achieve a light-bending scene in a modern PBR renderer.\
I'm very sorry if the code I wrote has bothered anyone. When the initial version of the material was developed, it was basically modified directly on the official PxrVolume and PxrVolHomoIntegrator. I didn't have time to streamline it. At that time, AI was not advanced enough, so I had to write out all the revisions by hand.\
<img width="1920" height="1080" alt="GvtySampleScene" src="https://github.com/user-attachments/assets/16946778-9b3c-4673-b5c8-0bfe3ef5dce5" />
It works with Geometry, Points, Volume OUTSIDE THE LENSING. If you make a PxrVolume Overlapping With in, it will crash your computer memory.

## How it progress
When a ray hit the Lensing Volume, For each step forward:\
**ray.dir = normalize(ray.dir + densityColor);** \
**ray.P += density * ray.direction;** \
After 2000 times if still hit nothing, the ray will directly shoot out with no distance limitation.

## Installation

### Maya
Put GravityLensingIntegrator.dll and Args/GravityLensingIntegrator.args into your Rman Plugin load Path and restart Maya.\
**Place .args file in RMAN_RIXPLUGINPATH/Args, you must create Args folder Otherwise It won't work in Maya**\
You can directly put it into your installation path (not recommend) or Check Pixar Documentation about how to install plugin.\
Note that Material ID is set to 1919529, you can manually change this in args file to prevent node id conflict. That day's solar eclipse proved the correctness of Einstein's theory of relativity.


### Houdini
Place the precompiled hda into $HOME/houdiniXX.x/otls \
Add RMAN_RIXPLUGINPATH to houdini.env and put dll file in it.\
```RMAN_RIXPLUGINPATH = "Path2RmanPlugin/Renderman/Plugin/GravityLensing"```\
**Place .args file in RMAN_RIXPLUGINPATH/Args, you must create Args folder Otherwise It won't work in Solaris**

## Samples Scene
### Warning
You Should Never Make this Volume as Aggregate, otherwise Renderman will threat it like a simple smoke.\
For optimization purpose Renderman will **Automaticly** set every Volume to globalAggregateVolume in Geometry->Volume Page, Remember to turn them off.\
**Maybe** it will not crash memory for now but it will make all render patch in the same Shading Location to Evaluate a PxrVolume smoke.\
<img width="620" height="731" alt="AggregateVolumeWarning" src="https://github.com/user-attachments/assets/f152918e-13e3-424d-8efa-48ca4717c2de" />\

## Houdini Scene
This integrator bxdf has neither input ports nor node icons for now, you can find it in PxrMaterialBuilder.\
There are some Parms in the pannel, if you change them nothing will happen because max step is already hardcode to 2000 :)\
But it really doesn't affect the performance because the light rays that hit the surface of the geometry will not be tracked.\
If you want a ray end up earlier, I suggest place a **PxrConstant** Black Sphere in the center of Blackhole.\
You can find example scene in release page.\
You can change that behavior but recompile will be needed. See the Compile Guide below.\
<img width="616" height="738" alt="Parms for nothing" src="https://github.com/user-attachments/assets/7b4a00d8-86d4-4547-b0ee-39b302c93830" />
<img width="2360" height="1286" alt="LensingInSolarisUSD" src="https://github.com/user-attachments/assets/c88b5dde-7823-44df-ae6f-1a4f1a9c5fcd" />


## Compile yourself
If you want to complie the code on windows, you should:\
Install VC143.\
Install Cmake.\
Headers file will install with Renderman Release at Pixar\RenderManProServer-27.2\include on windows.\
linking libprman.lib libstats.lib libpxrcore.lib to your library.

### Install Plugin via HDA
For houdini user, you can't just activate your plugin via args file, you need to convert into hda.\
The convert Python Script will be locate in e.g.\
```Pixar\RenderManForHoudini-27.2\3.11\21.0.596\python3.11libs\args2hda.py```\
Next you must find hython.exe.\
```Houdini 21.0.596\bin\hython.exe```\
write a bat/cmd next to your plugin Args file\
```"pathto/hython.exe" "pathto/args2hda.py"```\
It's really simple, after execute it will automatic generate hda and move it into $HOME/houdiniXX.x/otls.\
Then restart your houdini, if you just update dll file then you don't need to restart it, just stop render process will be ok for hot update.
