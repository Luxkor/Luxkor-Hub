x86_64-w64-mingw32-windres app.rc -O coff -o app.res.o
x86_64-w64-mingw32-gcc -Os -s -mwindows -static -o LuxkorHub.exe luxkor_hub.c app.res.o -lwinhttp -lgdiplus -lgdi32 -luser32 -lshell32 -lshlwapi -ldwmapi
