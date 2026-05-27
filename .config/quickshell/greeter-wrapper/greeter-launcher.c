/* greeter-launcher.c - fresh minimal Hyprland+Quickshell greeter wrapper
 *
 * Design goals:
 * - no root-only setup logic
 * - no writes under /etc at runtime
 * - mimic dms-greeter runtime behavior
 * - survive permission issues with safe /tmp fallbacks
 *
 * Build: make
 * Run:   ./build/greeter-launcher
 */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static pid_t child_pid = -1;

static void log_info(const char *msg) {
    fprintf(stderr, "[greeter-launcher] %s\n", msg);
}

static void log_err(const char *msg) {
    fprintf(stderr, "[greeter-launcher] ERROR: %s\n", msg);
}

static void sig_forward(int sig) {
    if (child_pid > 0) {
        kill(child_pid, sig);
    }
}

static bool path_exists(const char *path) {
    return access(path, F_OK) == 0;
}

static bool path_readable(const char *path) {
    return access(path, R_OK) == 0;
}

static bool path_writable_exec(const char *path) {
    return access(path, W_OK | X_OK) == 0;
}

static bool mkdir_p(const char *path, mode_t mode) {
    char tmp[PATH_MAX];
    size_t len;
    char *p;

    if (!path || !*path) {
        return false;
    }

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len == 0) {
        return false;
    }
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
                return false;
            }
            *p = '/';
        }
    }

    if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
        return false;
    }

    return true;
}

static int run_argv(char *const argv[]) {
    pid_t pid;
    int status;

    pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        execvp(argv[0], argv);
        _exit(127);
    }

    if (waitpid(pid, &status, 0) < 0) {
        return -1;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

static bool parent_dir(const char *path, char *out, size_t out_sz) {
    char *slash;
    if (!path || !*path) {
        return false;
    }
    snprintf(out, out_sz, "%s", path);
    slash = strrchr(out, '/');
    if (!slash || slash == out) {
        return false;
    }
    *slash = '\0';
    return true;
}

static bool same_path(const char *a, const char *b) {
    if (!a || !b) {
        return false;
    }
    return strcmp(a, b) == 0;
}

static bool find_cmd_in_path(const char *cmd, char *out, size_t out_sz) {
    const char *path_env;
    char *path_copy;
    char *saveptr = NULL;
    char *dir;

    if (!cmd || !*cmd) {
        return false;
    }

    if (strchr(cmd, '/')) {
        if (access(cmd, X_OK) == 0) {
            snprintf(out, out_sz, "%s", cmd);
            return true;
        }
        return false;
    }

    path_env = getenv("PATH");
    if (!path_env || !*path_env) {
        path_env = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";
    }

    path_copy = strdup(path_env);
    if (!path_copy) {
        return false;
    }

    for (dir = strtok_r(path_copy, ":", &saveptr); dir; dir = strtok_r(NULL, ":", &saveptr)) {
        char candidate[PATH_MAX];
        snprintf(candidate, sizeof(candidate), "%s/%s", dir, cmd);
        if (access(candidate, X_OK) == 0) {
            snprintf(out, out_sz, "%s", candidate);
            free(path_copy);
            return true;
        }
    }

    free(path_copy);
    return false;
}

static bool is_valid_qs_dir(const char *dir) {
    char shell_qml[PATH_MAX];
    if (!dir || !*dir) {
        return false;
    }
    snprintf(shell_qml, sizeof(shell_qml), "%s/shell.qml", dir);
    return path_readable(shell_qml);
}

static bool try_qs_dir(const char *dir, char *out, size_t out_sz) {
    if (is_valid_qs_dir(dir)) {
        snprintf(out, out_sz, "%s", dir);
        return true;
    }
    return false;
}

static bool try_named_qs_dir(const char *root, const char *name, char *out, size_t out_sz) {
    char candidate[PATH_MAX];

    if (!root || !*root || !name || !*name) {
        return false;
    }

    snprintf(candidate, sizeof(candidate), "%s/%s", root, name);
    return try_qs_dir(candidate, out, out_sz);
}

static bool try_home_blxshell_dir(const char *home, const char *name, char *out, size_t out_sz) {
    char root[PATH_MAX];

    if (!home || !*home || !name || !*name || strncmp(home, "/var/cache/", 11) == 0) {
        return false;
    }

    snprintf(root, sizeof(root), "%s/.local/blxshell", home);
    return try_named_qs_dir(root, name, out, out_sz);
}

static bool resolve_greeter_path(
    const char *explicit_path,
    const char *name,
    const char *orig_home,
    char *out,
    size_t out_sz
) {
    if (explicit_path && *explicit_path) {
        if (is_valid_qs_dir(explicit_path)) {
            snprintf(out, out_sz, "%s", explicit_path);
            return true;
        }
        return false;
    }

    if (!name || !*name) {
        name = "greeter";
    }

    {
        const char *blxshell_path = getenv("BLXSHELL_PATH");
        if (try_named_qs_dir(blxshell_path, name, out, out_sz)) {
            return true;
        }
    }

    {
        const char *override_home = getenv("GREETER_REAL_HOME");
        if (override_home && *override_home) {
            if (try_home_blxshell_dir(override_home, name, out, out_sz)) {
                return true;
            }
            {
                char c2[PATH_MAX];
                snprintf(c2, sizeof(c2), "%s/.config/quickshell/%s", override_home, name);
                if (try_qs_dir(c2, out, out_sz)) {
                    return true;
                }
            }
        }
    }

    if (try_home_blxshell_dir(orig_home, name, out, out_sz)) {
        return true;
    }

    if (orig_home && *orig_home && strncmp(orig_home, "/var/cache/", 11) != 0) {
        char c1[PATH_MAX];
        snprintf(c1, sizeof(c1), "%s/.config/quickshell/%s", orig_home, name);
        if (try_qs_dir(c1, out, out_sz)) {
            return true;
        }
    }

    {
        char c3[PATH_MAX];
        snprintf(c3, sizeof(c3), "/usr/share/quickshell/%s", name);
        if (is_valid_qs_dir(c3)) {
            snprintf(out, out_sz, "%s", c3);
            return true;
        }
    }

    {
        const char *xdg_config_dirs = getenv("XDG_CONFIG_DIRS");
        const char *xdg_data_dirs = getenv("XDG_DATA_DIRS");
        char *copy;
        char *saveptr = NULL;
        char *dir;

        if (!xdg_data_dirs || !*xdg_data_dirs) {
            xdg_data_dirs = "/usr/local/share:/usr/share";
        }

        copy = strdup(xdg_data_dirs);
        if (copy) {
            for (dir = strtok_r(copy, ":", &saveptr); dir; dir = strtok_r(NULL, ":", &saveptr)) {
                char c_share[PATH_MAX];
                snprintf(c_share, sizeof(c_share), "%s/blxshell/%s", dir, name);
                if (try_qs_dir(c_share, out, out_sz)) {
                    free(copy);
                    return true;
                }
            }
            free(copy);
        }

        saveptr = NULL;

        if (!xdg_config_dirs || !*xdg_config_dirs) {
            xdg_config_dirs = "/etc/xdg";
        }

        copy = strdup(xdg_config_dirs);
        if (copy) {
            for (dir = strtok_r(copy, ":", &saveptr); dir; dir = strtok_r(NULL, ":", &saveptr)) {
                char c4[PATH_MAX];
                snprintf(c4, sizeof(c4), "%s/quickshell/%s", dir, name);
                if (try_qs_dir(c4, out, out_sz)) {
                    free(copy);
                    return true;
                }
            }
            free(copy);
        }
    }

    {
        char c0[PATH_MAX];
        snprintf(c0, sizeof(c0), "/var/cache/greeter/.config/quickshell/%s", name);
        if (is_valid_qs_dir(c0)) {
            snprintf(out, out_sz, "%s", c0);
            return true;
        }
    }

    return false;
}

static void choose_cache_dir(const char *requested, char *out, size_t out_sz) {
    if (requested && *requested) {
        snprintf(out, out_sz, "%s", requested);
    } else {
        const char *env_cache = getenv("GREETER_CACHE_DIR");
        if (env_cache && *env_cache) {
            snprintf(out, out_sz, "%s", env_cache);
        } else {
            snprintf(out, out_sz, "/var/cache/greeter");
        }
    }

    if (!path_exists(out)) {
        if (!mkdir_p(out, 0700)) {
            char tmp_fallback[PATH_MAX];
            snprintf(tmp_fallback, sizeof(tmp_fallback), "/tmp/greeter-cache-%d", (int)getuid());
            mkdir_p(tmp_fallback, 0700);
            snprintf(out, out_sz, "%s", tmp_fallback);
            return;
        }
    }

    if (!path_writable_exec(out)) {
        char tmp_fallback[PATH_MAX];
        snprintf(tmp_fallback, sizeof(tmp_fallback), "/tmp/greeter-cache-%d", (int)getuid());
        mkdir_p(tmp_fallback, 0700);
        snprintf(out, out_sz, "%s", tmp_fallback);
    }
}

static bool ensure_cache_subdirs(const char *cache_dir) {
    char p1[PATH_MAX], p2[PATH_MAX], p3[PATH_MAX], p4[PATH_MAX], p5[PATH_MAX];
    snprintf(p1, sizeof(p1), "%s/.config", cache_dir);
    snprintf(p2, sizeof(p2), "%s/.cache", cache_dir);
    snprintf(p3, sizeof(p3), "%s/.local", cache_dir);
    snprintf(p4, sizeof(p4), "%s/.local/state", cache_dir);
    snprintf(p5, sizeof(p5), "%s/.local/share", cache_dir);

    return mkdir_p(p1, 0700) && mkdir_p(p2, 0700) && mkdir_p(p3, 0700) &&
           mkdir_p(p4, 0700) && mkdir_p(p5, 0700);
}

static bool sync_configs_to_cache(
    const char *source_greeter_path,
    const char *greeter_name,
    const char *cache_dir,
    bool debug,
    char *out_launch_path,
    size_t out_launch_path_sz
) {
    char cache_qs_root[PATH_MAX];
    char cache_greeter_path[PATH_MAX];
    char source_qs_root[PATH_MAX];
    char source_config_json[PATH_MAX];
    char source_colors_json[PATH_MAX];
    char target_config_json[PATH_MAX];
    char target_colors_json[PATH_MAX];

    if (!greeter_name || !*greeter_name) {
        greeter_name = "greeter";
    }

    snprintf(cache_qs_root, sizeof(cache_qs_root), "%s/.config/quickshell", cache_dir);
    snprintf(cache_greeter_path, sizeof(cache_greeter_path), "%s/%s", cache_qs_root, greeter_name);
    snprintf(out_launch_path, out_launch_path_sz, "%s", cache_greeter_path);

    if (!mkdir_p(cache_qs_root, 0700)) {
        return false;
    }

    if (!source_greeter_path || !*source_greeter_path || same_path(source_greeter_path, cache_greeter_path)) {
        return is_valid_qs_dir(cache_greeter_path);
    }

    {
        char *rm_argv[] = {"rm", "-rf", cache_greeter_path, NULL};
        (void)run_argv(rm_argv);
    }

    {
        char *cp_argv[] = {"cp", "-a", (char *)source_greeter_path, cache_qs_root, NULL};
        if (run_argv(cp_argv) != 0) {
            if (debug) {
                fprintf(stderr, "[greeter-launcher] sync warning: failed to copy greeter dir from %s\n", source_greeter_path);
            }
            return is_valid_qs_dir(cache_greeter_path);
        }
    }

    if (!parent_dir(source_greeter_path, source_qs_root, sizeof(source_qs_root))) {
        return is_valid_qs_dir(cache_greeter_path);
    }

    snprintf(source_config_json, sizeof(source_config_json), "%s/config.json", source_qs_root);
    snprintf(source_colors_json, sizeof(source_colors_json), "%s/Colors.json", source_qs_root);
    snprintf(target_config_json, sizeof(target_config_json), "%s/config.json", cache_qs_root);
    snprintf(target_colors_json, sizeof(target_colors_json), "%s/Colors.json", cache_qs_root);

    if (path_readable(source_config_json)) {
        char *cp_cfg_argv[] = {"cp", "-f", source_config_json, target_config_json, NULL};
        (void)run_argv(cp_cfg_argv);
    }

    if (path_readable(source_colors_json)) {
        unlink(target_colors_json);
        if (symlink(source_colors_json, target_colors_json) != 0) {
            char *cp_col_argv[] = {"cp", "-f", source_colors_json, target_colors_json, NULL};
            (void)run_argv(cp_col_argv);
        }
    }

    return is_valid_qs_dir(cache_greeter_path);
}

static void ensure_runtime_dir(void) {
    const char *xrd = getenv("XDG_RUNTIME_DIR");
    if (xrd && path_writable_exec(xrd)) {
        return;
    }

    {
        char fallback[PATH_MAX];
        snprintf(fallback, sizeof(fallback), "/tmp/greeter-runtime-%d", (int)getuid());
        mkdir_p(fallback, 0700);
        chmod(fallback, 0700);
        setenv("XDG_RUNTIME_DIR", fallback, 1);
    }
}

static bool write_hypr_config(const char *config_path, const char *qs_bin, const char *greeter_path) {
    FILE *f = fopen(config_path, "w");
    if (!f) {
        return false;
    }

    fprintf(f, "env = BSHELL_RUN_GREETER,1\n");
    fprintf(f, "misc {\n");
    fprintf(f, "    disable_hyprland_logo = true\n");
    fprintf(f, "    disable_splash_rendering = true\n");
    fprintf(f, "    disable_hyprland_guiutils_check = true\n");
    fprintf(f, "    force_default_wallpaper = 0\n");
    fprintf(f, "}\n");
    fprintf(f, "animations { enabled = false }\n");
    fprintf(f, "general { gaps_in = 0; gaps_out = 0; border_size = 0 }\n");
    fprintf(f, "decoration { rounding = 0; blur { enabled = false } shadow { enabled = false } }\n");
    fprintf(
        f,
        "exec-once = sh -c \"%s -p %s >/tmp/greeter-quickshell.log 2>&1; hyprctl dispatch exit\"\n",
        qs_bin,
        greeter_path
    );

    fclose(f);
    chmod(config_path, 0644);
    return true;
}

static void print_help(void) {
    puts("greeter-launcher");
    puts("Usage: greeter-launcher [--path DIR] [--name NAME] [--cache-dir DIR] [--debug]");
    puts("  --path DIR       absolute config path containing shell.qml");
    puts("  --name NAME      config name under quickshell paths (default: greeter)");
    puts("  --cache-dir DIR  greeter cache base (default: /var/cache/greeter)");
    puts("  --debug          verbose startup logs");
}

int main(int argc, char **argv) {
    const char *arg_path = NULL;
    const char *arg_name = "greeter";
    const char *arg_cache = NULL;
    bool debug = false;
    const char *orig_home = getenv("HOME");

    char qs_bin[PATH_MAX];
    char hypr_bin[PATH_MAX];
    char start_hypr_bin[PATH_MAX];
    bool have_start_hypr = false;

    char source_greeter_path[PATH_MAX];
    char greeter_path[PATH_MAX];
    char cache_dir[PATH_MAX];
    char hypr_conf_template[] = "/tmp/greeter-hyprland-XXXXXX";
    int hypr_fd;
    int status;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--path") == 0 || strcmp(argv[i], "-p") == 0) {
            if (i + 1 >= argc) {
                log_err("--path requires a value");
                return 2;
            }
            arg_path = argv[++i];
        } else if (strcmp(argv[i], "--name") == 0) {
            if (i + 1 >= argc) {
                log_err("--name requires a value");
                return 2;
            }
            arg_name = argv[++i];
        } else if (strcmp(argv[i], "--cache-dir") == 0) {
            if (i + 1 >= argc) {
                log_err("--cache-dir requires a value");
                return 2;
            }
            arg_cache = argv[++i];
        } else if (strcmp(argv[i], "--debug") == 0) {
            debug = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help();
            return 0;
        } else {
            fprintf(stderr, "[greeter-launcher] ERROR: unknown arg: %s\n", argv[i]);
            print_help();
            return 2;
        }
    }

    if (!find_cmd_in_path("quickshell", qs_bin, sizeof(qs_bin))) {
        if (!find_cmd_in_path("qs", qs_bin, sizeof(qs_bin))) {
            log_err("neither quickshell nor qs found in PATH");
            return 1;
        }
    }

    if (!find_cmd_in_path("Hyprland", hypr_bin, sizeof(hypr_bin))) {
        log_err("Hyprland not found in PATH");
        return 1;
    }
    have_start_hypr = find_cmd_in_path("start-hyprland", start_hypr_bin, sizeof(start_hypr_bin));

    if (!resolve_greeter_path(arg_path, arg_name, orig_home, source_greeter_path, sizeof(source_greeter_path))) {
        log_err("could not locate greeter config path (missing shell.qml)");
        return 1;
    }

    log_info("resolved greeter path");

    choose_cache_dir(arg_cache, cache_dir, sizeof(cache_dir));
    if (!ensure_cache_subdirs(cache_dir)) {
        log_err("failed to initialize cache subdirectories");
        return 1;
    }

    log_info("cache directory ready");

    if (!sync_configs_to_cache(source_greeter_path, arg_name, cache_dir, debug, greeter_path, sizeof(greeter_path))) {
        /* Fall back to source directly if sync fails. */
        snprintf(greeter_path, sizeof(greeter_path), "%s", source_greeter_path);
        if (debug) {
            fprintf(stderr, "[greeter-launcher] sync fallback to source=%s\n", source_greeter_path);
        }
    }

    setenv("HOME", cache_dir, 1);
    {
        char xdg_config_home[PATH_MAX];
        char xdg_data_home[PATH_MAX];
        char xdg_state_home[PATH_MAX];
        char xdg_cache_home[PATH_MAX];
        snprintf(xdg_config_home, sizeof(xdg_config_home), "%s/.config", cache_dir);
        snprintf(xdg_data_home, sizeof(xdg_data_home), "%s/.local/share", cache_dir);
        snprintf(xdg_state_home, sizeof(xdg_state_home), "%s/.local/state", cache_dir);
        snprintf(xdg_cache_home, sizeof(xdg_cache_home), "%s/.cache", cache_dir);
        setenv("XDG_CONFIG_HOME", xdg_config_home, 1);
        setenv("XDG_DATA_HOME", xdg_data_home, 1);
        setenv("XDG_STATE_HOME", xdg_state_home, 1);
        setenv("XDG_CACHE_HOME", xdg_cache_home, 1);
    }

    setenv("QT_QPA_PLATFORM", "wayland", 1);
    setenv("QT_WAYLAND_DISABLE_WINDOWDECORATION", "1", 1);
    setenv("XDG_SESSION_TYPE", "wayland", 1);
    setenv("EGL_PLATFORM", "gbm", 1);
    setenv("BSHELL_RUN_GREETER", "1", 1);
    setenv("BSHELL_GREET_CFG_DIR", cache_dir, 1);

    if (!getenv("RUST_LOG")) {
        setenv("RUST_LOG", "warn", 1);
    }

    ensure_runtime_dir();

    hypr_fd = mkstemp(hypr_conf_template);
    if (hypr_fd < 0) {
        log_err("mkstemp for Hyprland config failed");
        return 1;
    }
    close(hypr_fd);

    if (!write_hypr_config(hypr_conf_template, qs_bin, greeter_path)) {
        log_err("failed to write temporary Hyprland config");
        unlink(hypr_conf_template);
        return 1;
    }

    if (debug) {
        fprintf(stderr, "[greeter-launcher] qs=%s\n", qs_bin);
        fprintf(stderr, "[greeter-launcher] source=%s\n", source_greeter_path);
        fprintf(stderr, "[greeter-launcher] greeter=%s\n", greeter_path);
        fprintf(stderr, "[greeter-launcher] cache=%s\n", cache_dir);
        fprintf(stderr, "[greeter-launcher] hyprconf=%s\n", hypr_conf_template);
        fprintf(stderr, "[greeter-launcher] xdg_runtime=%s\n", getenv("XDG_RUNTIME_DIR"));
    }

    signal(SIGTERM, sig_forward);
    signal(SIGINT, sig_forward);
    signal(SIGHUP, sig_forward);

    child_pid = fork();
    if (child_pid < 0) {
        log_err("fork failed");
        unlink(hypr_conf_template);
        return 1;
    }

    if (child_pid == 0) {
        if (have_start_hypr) {
            execlp(start_hypr_bin, "start-hyprland", "--", "--config", hypr_conf_template, (char *)NULL);
        } else {
            execlp(hypr_bin, "Hyprland", "-c", hypr_conf_template, (char *)NULL);
        }
        perror("exec Hyprland");
        _exit(127);
    }

    waitpid(child_pid, &status, 0);
    child_pid = -1;
    unlink(hypr_conf_template);

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return 1;
}
