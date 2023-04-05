## make clean subroutine
.PHONY: clean_$(DIR)

clean_$(DIR): private _CLEAN := $(COBJS) $(LIB_DIR)/lib$(DIR).a
clean_$(DIR): private _DIR   := $(DIR)

clean_$(DIR): 
	@echo "Cleaning $(_DIR)...";
	@rm -f $(_CLEAN)