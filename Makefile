include CommonDefs.mak

BIN_DIR = Bin

INC_DIRS = .\
	../Common \
	../../Include \
	../../../External/GL \
	$(OPENNI2_INCLUDE) \
	$(NITE2_INCLUDE) \

SRC_FILES = *.cpp

ifeq ("$(OSTYPE)","Darwin")
	CFLAGS += -DMACOS
	LDFLAGS += -framework OpenGL -framework GLUT
else
	CFLAGS += -DUNIX -DGLX_GLXEXT_LEGACY
	USED_LIBS += glut GL
endif

LIB_DIRS += $(OPENNI2_REDIST) $(NITE2_REDIST64)

USED_LIBS += NiTE2 OpenNI2 opencv_highgui opencv_imgproc opencv_core

EXE_NAME = HandViewer

CFLAGS += -Wall

include CommonCppMakefile
