list-sensors
============
Console tool for showing all available sensors.
This tool is written in c++ and using libsdbusplus.

list-sensors can be used for list sensors on remote host.
for example: run on build-host to list sensors on salvador
```
   sshpass -p0penBmc ./list-sensors -H salvador.dev.yadro.com
```
