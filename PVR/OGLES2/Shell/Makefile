TOP_DIR = ../../..
OBJ_PREFIX = /TI/OGLES2/Shell/
TARGET = $(LIB_DIR)/libShellgl2.a
TARGETS = $(TARGET)

SOURCES = PVRShellAPI.cpp \
          PVRShell.cpp \
          PVRShellOS_X11.cpp

CXXFLAGS += -IX11 -I../include/OGLES2 -I../include -DBUILD_OGLES2 -DPVRSHELL_FPS_OUTPUT

include $(TOP_DIR)/Make.rules

$(TARGET): $(OBJ_DIR) $(DEPS) $(OBJS) $(LIB_DIR)
ifeq ($(V),0)
	@echo [AR] $@
	@$(AR) crs $@ $(OBJS)
else
	$(AR) crs $@ $(OBJS)
endif
