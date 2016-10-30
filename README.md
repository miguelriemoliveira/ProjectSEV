# RustBot
Software tools for the project Sistemas Embarcados de Vistoria (SEV). We refer to the hardware platform as RustBot because the AVT Mako cameras look kind of rusty (sorry for the lack of creativity).

## Table of Contents

* [The Robot](#therobot)
* [Installation](#installation)
* [Usage](#usage)
* [Calibration](#calibration)
* [Tunning Stereo Parameters](#tunningparameters)
* [Recording Raw Data](#recordingrawdata)
* [Finding IP Address of Cameras](#findingcameraip)

## <a name="therobot"></a>The Robot

The RustyBot is a stereo vision system with two AVT Mako cameras mounted on a plate. The baseline is around 0.1 meters, and the cameras are horizontally aligned. Here's a picture of the system

![robot](https://github.com/miguelriemoliveira/RustBot/blob/master/docs/robot.jpg)

## <a name="installation"></a>Installation

First, the Vimba sdk must be downloaded and compiled. We will refer to the folder where the sdk is installed as **$(Vimba_2_0)**.

You may download this sdk from [https://www.alliedvision.com/en/products/software.html](https://www.alliedvision.com/en/products/software.html). Then follow the installation instructions in **$(Vimba_2_0)/Documentation/ReleaseNotes.txt** to install the sdk.

After that, the ros vimba wrapper at [http://wiki.ros.org/avt_vimba_camera](http://wiki.ros.org/avt_vimba_camera) should work. So install the ros wrapper

```bash
git clone https://github.com/srv/avt_vimba_camera.git
catkin_make
```

## <a name="usage"></a>Usage

To start the complete stereo system, just run 

```bash
roslaunch rustbot_bringup all.launch
```


To launch a single camera use

```bash
roslaunch rustbot_bringup left_camera.launch
```

Note that to run stereo on high resolution images the processing takes a long time. Typically, for 1600x1200 images, our SGBM stereo outputs disparity images at 0.2 Hz. If you want a point cloud to be generated for each disparity image, then, because the point_cloud2 nodelet uses a message filter to get approximately synced image, camera_info and disparity messages, you must run the stereo_processing with a low frame rate and a large queue size. That way the point_cloud2 nodelet can get synced messages and produce the point clouds, e.g.:

```bash
roslaunch rustbot_bringup all.launch fps:=1 queue_size:=50
```

## <a name="calibration"></a>Calibration

Start the system without the stero processing

```bash
roslaunch rustbot_bringup all.launch do_stereo:=false
```

then startup de calibration

```bash
rosrun camera_calibration cameracalibrator.py --size 7x5 --square 0.03 right:=/stereo/right/image_raw left:=/stereo/left/image_raw right_camera:=/stereo/right left_camera:=/stereo/left --approximate=0.05
```

## <a name="tunningparameters"></a>Tunning Stereo Parameters

To tune the parameters of the stereo algorithm, run

```bash
roslaunch rustbot_bringup all.launch fps:=3 config_stereo:=true
```

and then

```bash
roslaunch rustbot_bringup visualize.launch 
```

Then you can change the parameters an see the effect they have in the disparity map / point cloud in real time.
If you reached a set of parameters you would like to save, do the following:

```bash
roscd rustbot_calibration/calibration/ && rosparam dump stereo_image_proc.yaml /stereo/stereo_image_proc
```

## <a name="recordingrawdata"></a>Recording Raw Data

To record raw data we must first launch the camera drivers (no need to run stereo processing, since this will be done offline)

```bash
roslaunch rustbot_bringup all.launch fps:=10 do_stereo:=false
```

Then, to record messages, run

```bash
roslaunch rustbot_bringup record.launch
```

After breaking the recorder node, the bag file can be found on the desktop.

## <a name="findingcameraip"></a>Finding IP Address of Cameras

To find out the ip address and the id of your cameras. From the **$(VimbaPath)/examples/ListCamera** folder, run

```bash
cd $(Vimba_2_0)
/Tools/Viewer/Bin/x86_64bit/VimbaViewer
```

You shoud see the VimbaViewer gui with all connected cameras with their id (guid). 

![vimba_viewer](https://github.com/miguelriemoliveira/RustBot/blob/master/docs/vimba_viewer.png)

Select a camera and then, on the right side pane, select All propperties and search for "IP Address" to find out the the ip address.

![get_ipaddress](https://github.com/miguelriemoliveira/RustBot/blob/master/docs/get_ipaddress.png)



You may edit the file **launch/mono_camera.launch** and 
 

By setting the correct guid and ip of your camera.

You can get the guid and ip of your camera by using the ListCamera or VimbaViewer binaries in

Vimba_2_0/VimbaCPP/Examples/Bin/x86_64bit
