#!/bin/bash

filename="$1"
fileout="${filename%.*}"
proxy="6"
# Hardware encode AMD
ffmpeg -threads 2 -hwaccel vaapi  -vaapi_device /dev/dri/renderD128 \
 -i  "$1" -c:v h264_vaapi -vf "format=nv12,hwupload,scale_vaapi=iw/'$proxy':ih/'$proxy'" \
 -vcodec h264_vaapi  -preset fast -c:a copy \
 -bf 0 -profile:v 66 "$fileout".proxy"$proxy"-mp4.mp4
