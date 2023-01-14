CSRCS			:= $(wildcard $(SRC_DIR)/$(DIR)/*.cpp) $(wildcard $(SRC_DIR)/$(DIR)/*.c)
CHDRS			:= $(wildcard $(SRC_DIR)/$(DIR)/*.h)
COBJS			:= $(addsuffix .o, $(addprefix $(BUILD_DIR)/, $(basename $(CSRCS))))
CDEPS			:= $(COBJS:%.o=%.d)

EXTLINK   		:= $(DIR).d

