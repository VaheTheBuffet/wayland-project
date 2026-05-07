#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <wayland-client.h>
#include <unistd.h>
#include <sys/mman.h>
#include <assert.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <xkbcommon/xkbcommon.h>
#include "protocols/xdg_shell-client-protocol.h"
#include "draw.h"

#define WIDTH 1920
#define HEIGHT 1080
#define STRIDE (WIDTH * 4)
#define SHM_POOL_SIZE (HEIGHT * STRIDE)
#define FPS 60.0f
#define SPEED 1000

static void draw_frame();
static void update();

static struct wl_compositor *compositor;
static int fd;
static uint8_t *pool_data;
static struct wl_shm *shm;
static struct wl_shm_pool *shm_pool;
static struct wl_buffer *buffer;
static struct wl_surface *surface;
static struct xdg_wm_base *xdg_wm_base;
static struct xdg_surface *xdg_surface;
static struct xdg_toplevel *toplevel;
static struct wl_keyboard *keyboard;
static struct wl_pointer *mouse;
static struct xkb_state *key_state;

static int current_time;
static float x;
static float y;
static bool should_draw;
static context ctx;

static bool KEYSTATE[256];

static void randname(char *buf)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	long r = ts.tv_nsec;
	for (int i = 0; i < 6; ++i) {
		buf[i] = 'A'+(r&15)+(r&16)*2;
		r >>= 5;
	}
}


static int create_shm_file(void)
{
	int retries = 100;
	do {
		char name[] = "/wl_shm-XXXXXX";
		randname(name + sizeof(name) - 7);
		--retries;
		int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd >= 0) {
			shm_unlink(name);
			return fd;
		}
	} while (retries > 0 && errno == EEXIST);
	return -1;
}


int allocate_shm_file(size_t size)
{
	int fd = create_shm_file();
	if (fd < 0)
		return -1;
	int ret;
	do {
		ret = ftruncate(fd, size);
	} while (ret < 0 && errno == EINTR);
	if (ret < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

static void capability(void *data, struct wl_seat *seat, uint32_t capabilities) 
{
    printf("keyaboard detected: %s\nmouse detected: %s\n",
            (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) ? "yes" : "no",
            (capabilities & WL_SEAT_CAPABILITY_POINTER) ? "yes" : "no");
}

static void name(void *data, struct wl_seat *seat, const char* name) 
{
    //Nothing needed here
}

static struct wl_seat_listener seat_listener = {
    .capabilities = capability,
    .name = name,
};

static void keymap(void *data, struct wl_keyboard *keyboard, uint32_t format, int32_t fd, uint32_t size)
{

    assert(format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1);
    const char *keymap_str = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);

    struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

    struct xkb_keymap *keymap = xkb_keymap_new_from_string(context, keymap_str, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);

    xkb_state_unref(key_state);
    key_state = xkb_state_new(keymap);
    
    munmap(keymap_str, size);
    close(fd);

}

static void key_enter(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *array)
{

}

static void key_leave(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface) 
{

}

static void key_key(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state) 
{
    int sym = xkb_state_key_get_one_sym(key_state, key+8);

    if(sym >= 128) {
        return;
    }

    if(state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        KEYSTATE[sym] = true;
    } else {
        KEYSTATE[sym] = false;
    }
}

static void key_modifiers(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group) 
{

}

static void key_repeat_info(void *data, struct wl_keyboard *keyboard, int32_t rate, int32_t delay) 
{

}

static struct wl_keyboard_listener keyboard_listener = {
    .keymap = keymap,
    .enter = key_enter,
    .leave = key_leave,
    .key = key_key,
    .modifiers = key_modifiers,
    .repeat_info = key_repeat_info
};

static void registry_handle_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) 
{
    if (!strcmp(interface, wl_compositor_interface.name)) {
        compositor = wl_registry_bind(registry, name, &wl_compositor_interface, version);

    } else if (!strcmp(interface, wl_shm_interface.name)) {
        shm = wl_registry_bind(registry, name, &wl_shm_interface, version);

    } else if (!strcmp(interface, xdg_wm_base_interface.name)) {
        xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, version);
        xdg_wm_base_add_listener(xdg_wm_base, &xdg_wm_base_listener, NULL);

    } else if (!strcmp(interface, wl_seat_interface.name)) {
        struct wl_seat *seat = wl_registry_bind(registry, name, &wl_seat_interface, version);
        wl_seat_add_listener(seat, &seat_listener, NULL);
        keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(keyboard, &keyboard_listener, NULL);
        mouse = wl_seat_get_pointer(seat);
    }
}


static void registry_handle_global_remove( void *data, struct wl_registry *registry, uint32_t name) 
{
    //No need to do anything here
}


static void wl_buffer_release(void *data, struct wl_buffer *wl_buffer)
{
    //signals the semaphore
    should_draw = true;
}


static const struct wl_registry_listener registry_listener = {
	.global = registry_handle_global,
	.global_remove = registry_handle_global_remove,
};

static const struct wl_buffer_listener wl_buffer_listener = { .release = wl_buffer_release, };


static void draw_frame() 
{
    //clear screen
    fill_rec(ctx, 0.0f, 0.0f, WIDTH, HEIGHT, 0xFFAAAAAA);

    //draw scene
    fill_rec(ctx, x, y, 100.0f, 100.0f, 0xFFAABB00);

    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_damage(surface, 0, 0, WIDTH, HEIGHT);
    wl_surface_commit(surface);

    //reset the semaphore which gets signalled by the release event 
    should_draw = false;
}

static void update() 
{
    if (KEYSTATE[XKB_KEY_w]) {
        y -= SPEED / FPS;
    }
    if (KEYSTATE[XKB_KEY_a]) {
        x -= SPEED / FPS;
    }
    if (KEYSTATE[XKB_KEY_s]) {
        y += SPEED / FPS;
    }
    if (KEYSTATE[XKB_KEY_d]) {
        x += SPEED / FPS;
    }
}

static void xdg_surface_handle_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    xdg_surface_ack_configure(xdg_surface, serial);
    // Safe to draw the initial frame now
    if (!should_draw) should_draw = true; 
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_handle_configure,
};

int main(int argc, char *argv[])
{
	struct wl_display *display = wl_display_connect(NULL);
	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(display);

    surface = wl_compositor_create_surface(compositor);


    fd = allocate_shm_file(SHM_POOL_SIZE);
    pool_data = mmap(NULL, SHM_POOL_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    shm_pool = wl_shm_create_pool(shm, fd, SHM_POOL_SIZE);

    buffer = wl_shm_pool_create_buffer(shm_pool, 0, WIDTH, HEIGHT, STRIDE, WL_SHM_FORMAT_XRGB8888);
    wl_buffer_add_listener(buffer, &wl_buffer_listener, NULL);

    uint32_t *pixels = (uint32_t *)&pool_data[0];

    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            if ((x + y / 8 * 8) % 16 < 8)
                pixels[y * WIDTH + x] = 0xFF666666;
            else
                pixels[y * WIDTH + x] = 0xFFEEEEEE;
        }
    }

    xdg_surface = xdg_wm_base_get_xdg_surface(xdg_wm_base, surface);
    xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);
    toplevel = xdg_surface_get_toplevel(xdg_surface);
    xdg_toplevel_set_title(toplevel, "Wayland Window");

    ctx = create_ctx(pixels, WIDTH, HEIGHT);

    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_damage(surface, 0, 0, UINT32_MAX, UINT32_MAX);
    wl_surface_commit(surface);

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
     
    double last_time = (ts.tv_nsec / 1e6) + ts.tv_sec * 1e3;

    struct pollfd fd = { wl_display_get_fd(display), POLLIN, 0 };

    while (true) {
        wl_display_flush(display);
        while (wl_display_prepare_read(display) != 0) {
            wl_display_dispatch_pending(display);
        }

        int ret = poll(&fd, 1, 0); 
        if (fd.revents & POLLIN) {
            wl_display_read_events(display);
        } else {
            wl_display_cancel_read(display);
        }

        wl_display_dispatch_pending(display);

        clock_gettime(CLOCK_MONOTONIC, &ts);
        double now = (ts.tv_nsec / 1e6) + ts.tv_sec * 1e3;

        if (now - last_time > 1000 / FPS && should_draw) {

            //GAME LOOP
            update();
            draw_frame();

            last_time = now;
            //GAME LOOP
        }
    }

	return 0; 
}
