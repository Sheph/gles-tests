TOP_DIR = ../../..
OBJ_PREFIX = /TI/OGLES2/Tools/
TARGET = $(LIB_DIR)/libToolsgl2.a
TARGETS = $(TARGET)

CXXFLAGS += -I../include/OGLES2 -I../include -DBUILD_OGLES2

include $(TOP_DIR)/Make.rules

$(TARGET): $(OBJ_DIR) $(DEPS) $(OBJS) $(LIB_DIR)
ifeq ($(V),0)
	@echo [AR] $@
	@$(AR) crs $@ $(OBJS)
else
	$(AR) crs $@ $(OBJS)
endif
