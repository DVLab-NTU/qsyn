include $(MK_INCLUDE_DIR)/localvars.mk
include $(MK_INCLUDE_DIR)/subroutines.mk

LIBS      = $(addprefix -l, $(LIBPKGS))

$(TARGET): private _COBJS							:= $(COBJS)
$(TARGET): private _DIR								:= $(DIR)
$(TARGET): private _TARGET							:= $(TARGET)

$(TARGET): $(COBJS) $(addprefix $(LIBDIR)/, $(SRCLIBS))
	@echo "Building $(@F)..."
	@$(CXX) $(_COBJS) $(CFLAGS) -I$(EXTINCDIR) -L$(LIBDIR) $(LIBS) -o $@
	@ln -fs $(_TARGET) .