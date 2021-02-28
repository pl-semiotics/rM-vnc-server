.PHONY: all clean

all: build/rM$(REMARKABLE_VERSION)-vnc-server build/rM$(REMARKABLE_VERSION)-vnc-server-standalone
clean:
	rm -rf build

build:
	mkdir -p build
build/%.o: %.c | build
	$(CC) $(CFLAGS) -c $< -o $@ $(LDFLAGS)
build/%: | build
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

build/vncs.o: backend.h
build/backend.o: backend.h
build/backend-mxc.o: backend.h
build/backend-qsg.o: backend.h

build/mxc_epdc_fb_damage.bin: | build
	$(OBJCOPY) -I binary -O elf32-littlearm -B arm $(MXC_EPDC_FB_DAMAGE_KO) $@
build/backend-mxc-standalone.o: mxc-standalone.ld build/backend-mxc.o build/mxc_epdc_fb_damage.bin | build
	$(LD) -Tmxc-standalone.ld -i -o $@ build/backend-mxc.o

build/rM1-vnc-server: build/vncs.o build/backend.o build/backend-mxc.o
build/rM1-vnc-server: private override LDFLAGS += -Wl,-Bstatic -lvncserver -Wl,-Bdynamic -lz -lpthread -lrM-input-devices
build/rM1-vnc-server-standalone: build/vncs.o build/backend.o build/backend-mxc-standalone.o
build/rM1-vnc-server-standalone: private override LDFLAGS += -lvncserver -lz -lpthread -lrM-input-devices-standalone -ludev
build/rM2-vnc-server: build/vncs.o build/backend.o build/backend-qsg.o
build/rM2-vnc-server: private override LDFLAGS += -Wl,-Bstatic -lvncserver -Wl,-Bdynamic -lqsgepaper-snoop -lz -lpthread -lrM-input-devices -lsystemd
build/rM2-vnc-server-standalone: build/vncs.o build/backend.o build/backend-qsg.o
build/rM2-vnc-server-standalone: private override LDFLAGS += -lvncserver -lqsgepaper-snoop-standalone -lz -lpthread -lcrypto -llzma -lrM-input-devices-standalone -ludev -lsystemd
