#!/bin/sh
echo PRINTF_FRAME_BEGIN
/root/printf-frame
echo PRINTF_FRAME_RC:$?
/bin/busybox poweroff -f
