#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <enet/enet.h>

static void test_sdl3_init(void **state) {
    (void)state;
    bool success = SDL_Init(SDL_INIT_EVENTS);
    assert_true(success);
    SDL_Quit();
}

static void test_sdl3_mixer_init(void **state) {
    (void)state;
    bool success = MIX_Init();
    assert_true(success);
    MIX_Quit();
}

static void test_enet_init(void **state) {
    (void)state;
    int res = enet_initialize();
    assert_int_equal(res, 0);
    enet_deinitialize();
}

static void test_enet_host_create(void **state) {
    (void)state;
    enet_initialize();
    ENetAddress address = {0};
    address.host = ENET_HOST_ANY;
    address.port = 12345;

    ENetHost *server = enet_host_create(&address, 32, 2, 0, 0);
    assert_non_null(server);
    
    enet_host_destroy(server);
    enet_deinitialize();
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_sdl3_init),
        cmocka_unit_test(test_sdl3_mixer_init),
        cmocka_unit_test(test_enet_init),
        cmocka_unit_test(test_enet_host_create),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
