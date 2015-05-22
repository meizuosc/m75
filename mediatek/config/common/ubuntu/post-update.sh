#!/sbin/sh
#
# Copyright 2014 Canonical Ltd.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
touch /cache/device-post-update.log
AID_ROOT=0
AID_SHELL=2000
AID_SYSTEM=1000
AID_RADIO=1001
AID_AUDIO=1005
AID_MEDIA_RW=1023
AID_APP=10000
AID_INET=3003

# build.prop
chmod 00644 /device/build.prop
chown $AID_ROOT:$AID_ROOT /device/build.prop

# app
chown $AID_ROOT:$AID_ROOT -R /device/app/
chmod 00755 -R /device/app
chmod 00644 $(find /device/app -type f)

# bin
chmod 00755 -R /device/bin/
chown -h $AID_ROOT:$AID_SHELL -R /device/bin/
chmod 02750 /device/bin/netcfg
chown $AID_ROOT:$AID_INET /device/bin/netcfg

# boot
chown $AID_ROOT:$AID_ROOT -R /device/boot/
chmod 00755 $(find /device/boot -type d)
chmod 00644 $(find /device/boot -type f)

# data
chown $AID_ROOT:$AID_ROOT -R /device/data/
chmod 00755 $(find /device/data -type d)
chmod 00644 $(find /device/data -type f)

chown $AID_ROOT:$AID_ROOT -R /device/framework
chmod 00755 $(find /device/framework -type d)
chmod 00644 $(find /device/framework -type f)

chown $AID_ROOT:$AID_ROOT -R /device/etc
chmod 00755 $(find /device/etc -type d)
chmod 00644 $(find /device/etc -type f)
chmod 00555 /device/etc/ppp/ip-down
chmod 00555 /device/etc/ppp/ip-up

chown $AID_ROOT:$AID_ROOT -R /device/lib
chmod 00755 $(find /device/lib -type d)
chmod 00644 $(find /device/lib -type f)

chown $AID_ROOT:$AID_ROOT -R /device/media
chmod 00755 $(find /device/media -type d)
chmod 00644 $(find /device/media -type f)

chown $AID_ROOT:$AID_ROOT -R /device/products
chmod 00755 $(find /device/products -type d)
chmod 00644 $(find /device/products -type f)

chown $AID_ROOT:$AID_ROOT -R /device/res
chmod 00755 $(find /device/res -type d)
chmod 00644 $(find /device/res -type f)

chown $AID_ROOT:$AID_ROOT -R /device/tts
chmod 00755 $(find /device/tts -type d)
chmod 00644 $(find /device/tts -type f)

chown $AID_ROOT:$AID_ROOT -R /device/ubuntu
chmod 00755 $(find /device/ubuntu -type d)
chmod 00644 $(find /device/ubuntu -type f)

chown $AID_ROOT:$AID_ROOT -R /device/usr
chmod 00755 $(find /device/usr -type d)
chmod 00644 $(find /device/usr -type f)

chown $AID_ROOT:$AID_SHELL $(find /device/vendor -type d)
chmod 00755 $(find /device/vendor -type d)
chown $AID_ROOT:$AID_ROOT $(find /device/vendor -type f)
chmod 00644 $(find /device/vendor -type f)
chmod 00755 -R /device/vendor/bin/

chmod 00755 -R /device/xbin/
chown $AID_ROOT:$AID_SHELL -R /device/xbin/
