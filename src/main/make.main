PKGFLAG   =
EXTHDRS   = 

include ../Makefile.in

BINDIR    = ../../bin
TARGET    = $(BINDIR)/$(EXEC)

target: $(TARGET)

$(TARGET): $(COBJS) $(LIBDEPEND)
	@echo "> building $(EXEC)..."
	@$(CXX) $(CFLAGS) -I$(EXTINCDIR) $(COBJS) -L$(LIBDIR) $(INCLIB) -o $@

