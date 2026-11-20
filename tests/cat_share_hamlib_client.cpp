// Run only against the loopback fake radio created by the integration test.
#include <hamlib/rig.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

int main(int argc, char** argv)
{
    if (argc != 2 || std::strncmp(argv[1], "127.0.0.1:", 10) != 0) return 1;
    rig_set_debug(RIG_DEBUG_ERR);
    RIG* rig = rig_init(RIG_MODEL_NETRIGCTL);
    if (!rig) return 2;
    std::strncpy(rig->state.rigport.pathname, argv[1], HAMLIB_FILPATHLEN - 1);
    auto check = [](const char* operation, int rc) {
        std::printf("%s: %d\n", operation, rc);
        std::fflush(stdout);
        return rc == RIG_OK;
    };
    if (!check("open", rig_open(rig))) return 3;
    if (!check("mode", rig_set_mode(rig, RIG_VFO_CURR, RIG_MODE_CW, 500))) return 4;
    value_t speed{};
    speed.i = 35;
    if (!check("set speed", rig_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_KEYSPD, speed))) return 5;
    speed.i = 0;
    if (!check("get speed", rig_get_level(rig, RIG_VFO_CURR, RIG_LEVEL_KEYSPD, &speed)) || speed.i != 35) return 6;
    if (!check("send", rig_send_morse(rig, RIG_VFO_CURR, "?"))) return 7;
    // Hamlib may dispatch send_morse via its own FIFO worker.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    if (!check("stop", rig_stop_morse(rig, RIG_VFO_CURR))) return 8;
    if (!check("close", rig_close(rig))) return 9;
    rig_cleanup(rig);
    return 0;
}
