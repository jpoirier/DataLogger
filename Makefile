#
CXX=g++
WINDLLMAIN=

# this assumes we have the proper tag
# if created via the github website then do $ git fetch --tags
GIT_VER=$(shell git describe --abbrev=0 --tags)

# Pass  $ make 386=1 for 32 bit version,
# otherwise the default is 64 bit.
ifdef 386
ARCH=32
ARCH_APL=i386
else
ARCH=64
ARCH_APL=x86_64
endif

HOSTOS=$(shell uname | tr A-Z a-z)
ifeq ($(HOSTOS),darwin)
 # -arch i386 -arch x86_64
 FILE_NAME=mac.xpl
 INCLUDE=-I/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.9.sdk/System/Library/Frameworks/OpenGL.framework/Headers
 LIBS=-framework IOKit -framework CoreFoundation -framework OpenGL
 LNFLAGS=-arch $(ARCH_APL) -dynamiclib -flat_namespace -undefined warning
 # -DTOGGLE_TEST_FEATURE
 CFLAGS=-std=c++11 -arch $(ARCH_APL) -Wall -O3 -D_APPLE_ -DAPL=1 -DIBM=0 -DLIN=0 -DVERSION=$(GIT_VER)
else
 ifeq ($(HOSTOS),linux)
  FILE_NAME=lin.xpl
  LIBS=
  # -m32 -m64
  LNFLAGS=-m$(ARCH) -shared -rdynamic -nodefaultlibs -undefined_warning
  CFLAGS=-std=c++11 -m$(ARCH) -Wall -O3 -DAPL=0 -DIBM=0 -DLIN=1 -fvisibility=hidden -fPIC -DVERSION=$(GIT_VER)
 else # windows
  FILE_NAME=win.xpl
  LIBS=-lXPLM
  LNFLAGS=-m$(ARCH) -Wl,-O1 -shared -L. -L./SDK/Libraries/Win/
  CFLAGS=-std=c++11 -m$(ARCH) -DAPL=0 -DIBM=1 -DLIN=0 -Wall -fpermissive -DVERSION=$(GIT_VER)
  WINDLLMAIN=main_win.o
 endif
endif

# To set user/compiler debug mode (use DPRINTF for stdio): -DDEBUG
# To dynamically check for USB connected saitek panels: -DDO_USBPANEL_CHECK
DEFS=-DXPLM200 -DXPLM210 -DLOGPRINTF

INCLUDE+=-I.
# INCLUDE+=-I../../readerwriterqueue


all:
ifeq ($(HOSTOS),windows)
	$(CXX) -c $(INCLUDE) $(DEFS) $(CFLAGS) main_win.cpp
endif
	$(CXX) -c $(INCLUDE) $(DEFS) $(CFLAGS) main.cpp

	$(CXX) -o $(FILE_NAME) main.o $(WINDLLMAIN) $(LNFLAGS) $(LIBS)

clean:
	$(RM) *.o *.xpl
