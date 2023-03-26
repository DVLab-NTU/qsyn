include $(MK_INCLUDE_DIR)/localvars.mk

TARGET   							:= $(LIB_DIR)/lib$(DIR).a

## make libs subroutine
$(TARGET): private _DIR   			:= $(DIR)
$(TARGET): private _CSRCS			:= $(CSRCS)
$(TARGET): private _COBJS			:= $(COBJS)

$(TARGET): $(COBJS)
	@-mkdir -p $(@D)
	@$(ECHO) "Building $(@F)..."
	@$(AR) $@ $(_COBJS)

include $(MK_INCLUDE_DIR)/subroutines.mk

