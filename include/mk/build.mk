include $(MK_INCLUDE_DIR)/localvars.mk

LIBS      = $(addprefix -l, $(LIBPKGS))

$(TARGET): private _COBJS							:= $(COBJS)
$(TARGET): private _DIR								:= $(DIR)
$(TARGET): private _TARGET							:= $(TARGET)

$(TARGET): $(COBJS) $(addprefix $(LIB_DIR)/, $(SRCLIBS))
	@echo "Building $(@F)..."
	@$(CXX) $(_COBJS) $(CFLAGS) -I$(EXTINCDIR) -L$(LIB_DIR) $(LIBS) -o $@
	@ln -fs $(_TARGET) .

include $(MK_INCLUDE_DIR)/subroutines.mk