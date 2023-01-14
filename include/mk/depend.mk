## make depend subroutine
.PHONY: depend_$(DIR)

depend_$(DIR): src/$(DIR)/.depend.mk $(CSRCS) $(CHDRS)

src/$(DIR)/.depend.mk: private _DIR   			:= $(DIR)
src/$(DIR)/.depend.mk: private _CSRCS   		:= $(CSRCS)
src/$(DIR)/.depend.mk: private _TARGET   		:= src/$(DIR)/.depend.mk

src/$(DIR)/.depend.mk: 
	@$(ECHO) "Making dependencies to $(_DIR)..."
	@$(CXX) -MM $(DEPENDDIR) $(_CSRCS) > $@