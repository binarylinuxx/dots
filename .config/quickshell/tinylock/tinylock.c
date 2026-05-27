/* tinylock.c — minimal wlroots compositor for ext-session-lock-v1
 * Compile: make
 * Run:     ./build/tinylock
 *
 * Does nothing except:
 *   1. Open a DRM/backend session
 *   2. Advertise ext-session-lock-v1
 *   3. Accept a lock, render lock surfaces via wlr_scene
 *   4. Exit when the lock is unlocked
 */
#define _POSIX_C_SOURCE 200809L
#define WLR_USE_UNSTABLE

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_session_lock_v1.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/log.h>

struct server {
	struct wl_display *wl_display;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_compositor *compositor;
	struct wlr_session_lock_manager_v1 *lock_mgr;
	struct wlr_scene *scene;
	struct wlr_output_layout *output_layout;
	struct wlr_session_lock_v1 *lock;
	bool locked;
	struct wl_list outputs;            /* struct tinylock_output.link */
	struct wl_listener new_output;
	struct wl_listener lock_new;
	struct wl_listener lock_unlock;
	struct wl_listener lock_destroy;
};

struct tinylock_output {
	struct server *server;
	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;
	struct wlr_session_lock_surface_v1 *lock_surface;
	struct wl_list link;
	struct wl_listener frame;
	struct wl_listener lock_new_surface;
	struct wl_listener destroy;
};

static void output_frame(struct wl_listener *listener, void *data) {
	struct tinylock_output *output = wl_container_of(listener, output, frame);
	wlr_scene_output_commit(output->scene_output, NULL);
}

static void handle_lock_new_surface(struct wl_listener *listener, void *data) {
	struct wlr_session_lock_surface_v1 *lock_surface = data;
	struct tinylock_output *output =
		wl_container_of(listener, output, lock_new_surface);

	output->lock_surface = lock_surface;

	struct wlr_scene_tree *tree = lock_surface->surface->data;
	if (tree) {
		wlr_scene_node_reparent(&tree->node,
			&output->scene_output->scene->tree);
	}

	int ow, oh;
	wlr_output_effective_resolution(output->wlr_output, &ow, &oh);
	wlr_session_lock_surface_v1_configure(lock_surface, ow, oh);

	wlr_output_schedule_frame(output->wlr_output);
}

static void output_destroy(struct wl_listener *listener, void *data) {
	struct tinylock_output *output =
		wl_container_of(listener, output, destroy);
	wl_list_remove(&output->frame.link);
	wl_list_remove(&output->lock_new_surface.link);
	wl_list_remove(&output->destroy.link);
	wl_list_remove(&output->link);
	free(output);
}

static void new_output(struct wl_listener *listener, void *data) {
	struct server *server = wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;

	wlr_output_init_render(wlr_output, server->allocator, server->renderer);

	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, true);
	wlr_output_commit_state(wlr_output, &state);
	wlr_output_state_finish(&state);

	struct tinylock_output *output = calloc(1, sizeof(*output));
	output->server = server;
	output->wlr_output = wlr_output;
	wl_list_insert(&server->outputs, &output->link);

	output->scene_output = wlr_scene_output_create(server->scene, wlr_output);

	output->frame.notify = output_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);

	if (server->lock) {
		output->lock_new_surface.notify = handle_lock_new_surface;
		wl_signal_add(&server->lock->events.new_surface,
			&output->lock_new_surface);
	}

	output->destroy.notify = output_destroy;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);

	wlr_output_layout_add_auto(server->output_layout, wlr_output);

	wlr_log(WLR_INFO, "tinylock: new output '%s'", wlr_output->name);
}

static void handle_lock_unlock(struct wl_listener *listener, void *data);
static void handle_lock_destroy(struct wl_listener *listener, void *data);

static void handle_lock_new(struct wl_listener *listener, void *data) {
	struct server *server = wl_container_of(listener, server, lock_new);
	struct wlr_session_lock_v1 *lock = data;

	if (server->locked) {
		wlr_session_lock_v1_destroy(lock);
		return;
	}

	server->lock = lock;
	server->locked = true;

	wlr_session_lock_v1_send_locked(lock);
	wlr_log(WLR_INFO, "tinylock: session locked");

	server->lock_unlock.notify = handle_lock_unlock;
	wl_signal_add(&lock->events.unlock, &server->lock_unlock);

	server->lock_destroy.notify = handle_lock_destroy;
	wl_signal_add(&lock->events.destroy, &server->lock_destroy);

	struct tinylock_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		output->lock_new_surface.notify = handle_lock_new_surface;
		wl_signal_add(&lock->events.new_surface,
			&output->lock_new_surface);
		wlr_output_schedule_frame(output->wlr_output);
	}
}

static void handle_lock_unlock(struct wl_listener *listener, void *data) {
	struct server *server = wl_container_of(listener, server, lock_unlock);
	wlr_log(WLR_INFO, "tinylock: session unlocked, exiting");
	wl_display_terminate(server->wl_display);
}

static void handle_lock_destroy(struct wl_listener *listener, void *data) {
	struct server *server = wl_container_of(listener, server, lock_destroy);
	server->lock = NULL;
	server->locked = false;
}

int main(int argc, char *argv[]) {
	wlr_log_init(WLR_INFO, NULL);

	struct server server = { 0 };
	wl_list_init(&server.outputs);

	server.wl_display = wl_display_create();
	server.backend = wlr_backend_autocreate(
		wl_display_get_event_loop(server.wl_display), NULL);
	if (!server.backend) {
		wlr_log(WLR_ERROR, "Failed to create backend");
		return 1;
	}

	server.renderer = wlr_renderer_autocreate(server.backend);
	if (!server.renderer) {
		wlr_log(WLR_ERROR, "Failed to create renderer");
		return 1;
	}
	wlr_renderer_init_wl_display(server.renderer, server.wl_display);

	server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);
	server.compositor = wlr_compositor_create(server.wl_display, 6, server.renderer);
	server.output_layout = wlr_output_layout_create(server.wl_display);
	server.scene = wlr_scene_create();
	server.lock_mgr = wlr_session_lock_manager_v1_create(server.wl_display);

	server.new_output.notify = new_output;
	wl_signal_add(&server.backend->events.new_output, &server.new_output);

	server.lock_new.notify = handle_lock_new;
	wl_signal_add(&server.lock_mgr->events.new_lock, &server.lock_new);

	if (!wlr_backend_start(server.backend)) {
		wlr_log(WLR_ERROR, "Failed to start backend");
		wl_display_destroy(server.wl_display);
		return 1;
	}

	const char *display_name = wl_display_add_socket_auto(server.wl_display);
	if (!display_name) {
		wlr_log(WLR_ERROR, "Failed to add wayland socket");
		wl_display_destroy(server.wl_display);
		return 1;
	}
	wlr_log(WLR_INFO, "tinylock: WAYLAND_DISPLAY=%s", display_name);

	if (argc > 1) {
		pid_t pid = fork();
		if (pid == 0) {
			setenv("WAYLAND_DISPLAY", display_name, 1);
			setenv("QT_QPA_PLATFORM", "wayland", 1);
			execvp(argv[1], &argv[1]);
			_exit(127);
		}
	}

	wl_display_run(server.wl_display);

	wl_display_destroy_clients(server.wl_display);
	wl_display_destroy(server.wl_display);
	return 0;
}
