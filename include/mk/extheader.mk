## make extheader subroutine
.PHONY: extheader_$(DIR)

extheader_$(DIR): $(BUILD_SRC_DIR)/$(DIR)/.extheader.mk

$(BUILD_SRC_DIR)/$(DIR)/.extheader.mk: private _EXTHDRS 		:= $(EXTHDRS)
$(BUILD_SRC_DIR)/$(DIR)/.extheader.mk: private _DIR   			:= $(DIR)
$(BUILD_SRC_DIR)/$(DIR)/.extheader.mk: private _EXTLINK   		:= $(EXTLINK)

$(BUILD_SRC_DIR)/$(DIR)/.extheader.mk: src/$(DIR)/rules.mk
	@$(ECHO) Linking external header files of $(_DIR)...
	@-mkdir -p $(@D)
	@$(ECHO) -n "$(_EXTLINK):" > $@
	@for hdr in $(_EXTHDRS); do \
		$(ECHO) -n "$(EXTINCDIR)/$$hdr " >> $@; \
		rm -f $(EXTINCDIR)/$(_DIR); \
		ln -fs ../$(SRC_DIR)/$(_DIR)/$$hdr $(EXTINCDIR)/$$hdr; \
	done
	@$(ECHO) >> $@
	@for hdr in $(_EXTHDRS); do \
		$(ECHO) "$(EXTINCDIR)/$$hdr: src/$(_DIR)/$$hdr" >> $@; \
		$(ECHO) "	@rm -f $(EXTINCDIR)/$$hdr" >> $@; \
		$(ECHO) "	@ln -fs ../$(SRC_DIR)/$(_DIR)/$$hdr $(EXTINCDIR)/$$hdr" >> $@; \
	done