
#! /usr/bin/bash
media-ctl -d /dev/media0 --links "'m00_f_ov5640 2-003c':0 -> 'ckispv0_isp':0 [1]"
media-ctl -d /dev/media0 --links "'fake_bt1120 1-0057':0 -> 'ckispv0_isp':0 [0]"
media-ctl -d /dev/media0 --set-v4l2 '"m00_f_ov5640 2-003c":0 [fmt:YUYV8_2X8/1280x720@1/30]'
media-ctl -d /dev/media0 --set-v4l2 '"ckispv0_isp":0 [fmt: YUYV8_2X8/1280x720 crop: (0,0)/1280x720]'
media-ctl -d /dev/media0 --set-v4l2 '"ckispv0_isp":2 [fmt:YUYV8_2X8/1280x720 crop: (0,0)/1280x720]'
media-ctl -d /dev/media0 --set-v4l2 '"ckispv0_resizer_selfpath":0 [fmt:YUYV8_2X8/1280x720]'
media-ctl -d /dev/media0 --set-v4l2 '"ckispv0_resizer_mainpath":0 [fmt:YUYV8_2X8/1280x720]'
media-ctl -d /dev/media0 --set-v4l2 '"ckispv0_resizer_selfpath":1 [fmt:YUYV8_2X8/1280x720]'
media-ctl -d /dev/media0 --set-v4l2 '"ckispv0_resizer_mainpath":1 [fmt:YUYV8_2X8/1280x720]'
v4l2-ctl  -d /dev/video0 --set-fmt-video "width=1280,height=720,pixelformat=NV21"
v4l2-ctl  -d /dev/video1 --set-fmt-video "width=1280,height=720,pixelformat=NV21"
