include $(SRC_DIR)/sm_io/modules/modules.mk
include $(SRC_DIR)/sm_io/rw_param/rw_param.mk
include $(SRC_DIR)/sm_io/protocols/protocols.mk
include $(SRC_DIR)/sm_io/chips/chips.mk

sm_io_DIR = $(SRC_DIR)/sm_io

sm_io_OBJS = $(sm_io_DIR)/sm_io.o \
	     $(sm_io_DIR)/sm_io_bootstrap.o \
	     $(sm_io_DIR)/sm_io_err.o \
	     $(sm_io_modules_OBJS) \
	     $(sm_io_rw_param_OBJS) \
	     $(sm_io_protocols_OBJS) \
	     $(sm_io_chips_OBJS)
