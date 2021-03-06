halcsd_DIR = $(SRC_DIR)/apps/halcsd

halcsd_OBJS = $(halcsd_DIR)/halcsd.o

halcsd_OUT = halcsd

ifeq ($(WITH_APP_CFG),y)
halcsd_cfg_OBJS = $(halcsd_DIR)/halcsd_cfg.o
halcsd_cfg_OUT = halcsd_cfg
else
halcsd_cfg_OBJS =
halcsd_cfg_OUT =
endif

halcsd_OUT += $(halcsd_cfg_OUT)

halcsd_all_OUT = halcsd halcsd_cfg
halcsd_all_OBJS = $(halcsd_OBJS) $(halcsd_cfg_OBJS)

halcsd_LIBS = -lbsmp
halcsd_STATIC_LIBS =

halcsd_cfg_LIBS = -lbsmp
halcsd_cfg_STATIC_LIBS =
