TOP_DIR = ../../..
OBJ_PREFIX = /TI/OGLES2/ChameleonMan
TARGET = $(BIN_DIR)/ChameleonMan
TARGETS = $(TARGET)

CXXFLAGS += -I../include/OGLES2 -I../include -DBUILD_OGLES2

include $(TOP_DIR)/Make.rules

$(TARGET): $(OBJ_DIR) $(DEPS) $(OBJS) $(BIN_DIR)
ifeq ($(V),0)
	@echo [LD] $@
	@$(CXX) $(LDFLAGS) -pthread -o $@ $(OBJS) -lShellgl2 -lToolsgl2 -lEGL -lGLESv2 -lX11 -lm
else
	$(CXX) $(LDFLAGS) -pthread -o $@ $(OBJS) -lShellgl2 -lToolsgl2 -lEGL -lGLESv2 -lX11 -lm
endif
