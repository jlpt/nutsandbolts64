/*
 * Scripted input plugin for mupen64plus (test harness only).
 *
 * Reads the file named by $NB_INPUT_SCRIPT. Each non-comment line is:
 *
 *     <start_poll> <num_polls> <buttons> [stick_x stick_y]
 *
 * where <buttons> is NONE or a '+' separated list of
 * A B Z L R START DU DD DL DR CU CD CL CR. Polls are counted per
 * controller-1 read issued by the game, which makes scripts follow the
 * game's own frame pacing. The current poll count is logged to stderr every
 * 100 polls so scripts can be lined up with --testshots frames.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define M64P_PLUGIN_PROTOTYPES 1
#include <mupen64plus/m64p_types.h>
#include <mupen64plus/m64p_plugin.h>

typedef struct {
    long start, len;
    unsigned buttons;
    int x, y;
} Step;

static Step sSteps[4096];
static int sNumSteps;
static long sPolls;
static CONTROL *sControls;

static unsigned parse_buttons(char *s) {
    unsigned b = 0;
    char *tok = strtok(s, "+");
    while (tok) {
        if (!strcmp(tok, "A")) b |= 1u << 7;
        else if (!strcmp(tok, "B")) b |= 1u << 6;
        else if (!strcmp(tok, "Z")) b |= 1u << 5;
        else if (!strcmp(tok, "START")) b |= 1u << 4;
        else if (!strcmp(tok, "DU")) b |= 1u << 3;
        else if (!strcmp(tok, "DD")) b |= 1u << 2;
        else if (!strcmp(tok, "DL")) b |= 1u << 1;
        else if (!strcmp(tok, "DR")) b |= 1u << 0;
        else if (!strcmp(tok, "CU")) b |= 1u << 11;
        else if (!strcmp(tok, "CD")) b |= 1u << 10;
        else if (!strcmp(tok, "CL")) b |= 1u << 9;
        else if (!strcmp(tok, "CR")) b |= 1u << 8;
        else if (!strcmp(tok, "R")) b |= 1u << 12;
        else if (!strcmp(tok, "L")) b |= 1u << 13;
        tok = strtok(NULL, "+");
    }
    return b;
}

static void load_script(void) {
    const char *path = getenv("NB_INPUT_SCRIPT");
    char line[256];
    FILE *f;

    sNumSteps = 0;
    if (!path || !(f = fopen(path, "r"))) {
        fprintf(stderr, "[script-input] no script (NB_INPUT_SCRIPT)\n");
        return;
    }
    while (fgets(line, sizeof(line), f) && sNumSteps < 4096) {
        char btn[128];
        Step st = {0};
        if (line[0] == '#' || line[0] == '\n') continue;
        if (sscanf(line, "%ld %ld %127s %d %d", &st.start, &st.len, btn, &st.x, &st.y) < 3) continue;
        st.buttons = strcmp(btn, "NONE") ? parse_buttons(btn) : 0;
        sSteps[sNumSteps++] = st;
    }
    fclose(f);
    fprintf(stderr, "[script-input] loaded %d steps from %s\n", sNumSteps, path);
}

EXPORT m64p_error CALL PluginStartup(m64p_dynlib_handle core, void *ctx, void (*dbg)(void *, int, const char *)) {
    (void)core; (void)ctx; (void)dbg;
    load_script();
    return M64ERR_SUCCESS;
}

EXPORT m64p_error CALL PluginShutdown(void) { return M64ERR_SUCCESS; }

EXPORT m64p_error CALL PluginGetVersion(m64p_plugin_type *type, int *ver, int *api, const char **name, int *caps) {
    if (type) *type = M64PLUGIN_INPUT;
    if (ver) *ver = 0x010000;
    if (api) *api = 0x020100;
    if (name) *name = "NB64 scripted input";
    if (caps) *caps = 0;
    return M64ERR_SUCCESS;
}

EXPORT void CALL InitiateControllers(CONTROL_INFO info) {
    int i;
    sControls = info.Controls;
    for (i = 0; i < 4; i++) {
        sControls[i].Present = (i == 0);
        sControls[i].RawData = 0;
        sControls[i].Plugin = PLUGIN_NONE;
    }
}

EXPORT void CALL GetKeys(int control, BUTTONS *keys) {
    int i;
    keys->Value = 0;
    if (control != 0) return;
    for (i = 0; i < sNumSteps; i++) {
        if (sPolls >= sSteps[i].start && sPolls < sSteps[i].start + sSteps[i].len) {
            keys->Value |= sSteps[i].buttons;
            if (sSteps[i].x || sSteps[i].y) {
                keys->X_AXIS = sSteps[i].x;
                keys->Y_AXIS = sSteps[i].y;
            }
        }
    }
    if (sPolls % 100 == 0) fprintf(stderr, "[script-input] poll %ld\n", sPolls);
    sPolls++;
}

EXPORT void CALL ControllerCommand(int control, unsigned char *cmd) { (void)control; (void)cmd; }
EXPORT void CALL ReadController(int control, unsigned char *cmd) { (void)control; (void)cmd; }
EXPORT int CALL RomOpen(void) { sPolls = 0; return 1; }
EXPORT void CALL RomClosed(void) {}
EXPORT void CALL SDL_KeyDown(int mod, int sym) { (void)mod; (void)sym; }
EXPORT void CALL SDL_KeyUp(int mod, int sym) { (void)mod; (void)sym; }
