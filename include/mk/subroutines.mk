## make target
.PHONY: $(DIR)
$(DIR): $(TARGET)

## make external headers
include $(MK_INCLUDE_DIR)/extheader.mk
## make clean
include $(MK_INCLUDE_DIR)/clean.mk

## ------------------------------------------------
## //SECTION - dependencies/external header options
## ------------------------------------------------

ifneq ($(MAKECMDGOALS), cleanall)
ifneq ($(MAKECMDGOALS), clean)
-include	$(CDEPS)
-include	$(BUILD_SRC_DIR)/$(DIR)/.extheader.mk
endif
endif