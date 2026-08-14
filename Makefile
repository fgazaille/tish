DEBUG = FALSE

GCC = nspire-gcc
AS  = nspire-as
GXX = nspire-g++
LD  = nspire-ld
GENZEHN = genzehn

GCCFLAGS = -Wall -W -marm -Iinclude -MMD -MP
LDFLAGS = -Wl,--nspireio
ZEHNFLAGS = --name "tish"

ifeq ($(DEBUG),FALSE)
	GCCFLAGS += -Os
else
	GCCFLAGS += -O0 -g
endif

OBJS = src/tish.o
-include $(OBJS:.o=.d)
EXE = tish
DISTDIR = .
vpath %.tns $(DISTDIR)
vpath %.elf $(DISTDIR)

all: $(EXE).tns

%.o: %.c
	$(GXX) $(GCCFLAGS) -c $< -o $@
%.o: %.cpp
	$(GXX) $(GCCFLAGS) -c $< -o $@

%.o: %.S
	$(AS) -c $< -o $@

$(EXE).elf: $(OBJS)
	mkdir -p $(DISTDIR)
	$(LD) $^ -o $@ $(LDFLAGS)

$(EXE).tns: $(EXE).elf
	$(GENZEHN) --input $^ --output $@.zehn $(ZEHNFLAGS)
	make-prg $@.zehn $@
	rm $@.zehn

clean:
	rm -f $(OBJS) $(OBJS:.o=.d) $(DISTDIR)/$(EXE).tns $(DISTDIR)/$(EXE).elf $(DISTDIR)/$(EXE).zehn
