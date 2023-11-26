# X11 Desktop Cube Makefile

# Variables
CC = gcc
CFLAGS = -Wall -fstack-protector-strong
CFLAGS_DEBUG = -Wall -O0 -g
LDFLAGS = -Wl,-z,relro,-z,now
LDFLAGS_DEBUG = 
LIBS = -lm -lX11 -lXinerama -lGL -lGLU -lGLEW
TARGET = build/desktop_cube
SOURCES = src/*.c
OBJDIR = build
INSTALL_DIR = /opt/cube
SYSTEMD_USER_DIR = ~/.config/systemd/user

# Build Rules
all: release

$(OBJDIR):
	mkdir -p $(OBJDIR)

release: $(OBJDIR) $(SOURCES)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SOURCES) -o $(TARGET) $(LIBS)
	strip $(TARGET)

debug: $(OBJDIR) $(SOURCES)
	$(CC) $(CFLAGS_DEBUG) $(LDFLAGS_DEBUG) $(SOURCES) -o $(TARGET) $(LIBS)

clean:
	rm -rf $(OBJDIR)

# Install rules
install:
	@echo "Installing Desktop Cube..."
	@sudo mkdir -p $(INSTALL_DIR)
	@sudo cp $(TARGET) $(INSTALL_DIR)/desktop_cube
	@mkdir -p $(SYSTEMD_USER_DIR)
	@cp systemd/desktop_cube.service $(SYSTEMD_USER_DIR)/desktop_cube.service
	@systemctl --user daemon-reload
	@echo "Installation complete. To enable autostart, use 'systemctl --user enable desktop_cube.service'"
