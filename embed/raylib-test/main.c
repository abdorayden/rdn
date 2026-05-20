#include <raylib.h>
#include <stdio.h>
#include <string.h>
#include "../../include/src.h"
#include "raylib-6.0_linux_amd64/include/raylib.h"

bool rdn_init_window(RDNApi* api) {
    if (api->stack_size(api) < 3) {
        api->raise_error(api , "init-window requires 3 params");
        return false;
    }

    long width;
    long height;
    char* title;

    if(api->to_integer(api , -3 , &width ) && api->to_integer(api , -2 , &height) && (title = api->to_string(api , -1))) {
        InitWindow((int)width, (int)height, title);
    }else {
        api->raise_error(api , "idk some error");
        return false;
    }
    return true;
}

bool rdn_close_window(RDNApi* api) {
    CloseWindow();
    return true;
}

bool rdn_begin_drawing(RDNApi* api) {
    BeginDrawing();
    return true;
}

bool rdn_end_drawing(RDNApi* api) {
    EndDrawing();
    return true;
}

bool rdn_window_should_close(RDNApi* api) {
    api->push_boolean(api , WindowShouldClose());
    return true;
}

bool rdn_clear_bg(RDNApi* api) {
    if (api->stack_size(api) < 1) {
        api->raise_error(api , "requires hex color");
        return false;
    }
    long hex_color = 0;
    if (!api->to_integer(api , -1 , &hex_color)) {
        api->raise_error(api , "khra");
        return false;
    }
    ClearBackground(GetColor((unsigned int)hex_color));
    return true;
}

bool rdn_draw_rect(RDNApi* api) {
    if (api->stack_size(api) < 5) {
        api->raise_error(api , "requires 5");
        return false;
    }
    long x;
    long y;
    long width;
    long height;
    long hex_color = 0;

    bool result = api->to_integer(api , -1 , &hex_color);
    result &= api->to_integer(api , -2 , &height);
    result &= api->to_integer(api , -3 , &width);
    result &= api->to_integer(api , -4 , &y);
    result &= api->to_integer(api , -5 , &x);
    if (!result) {
        api->raise_error(api , "khra");
        return false;
    }
    DrawRectangle((int)x, (int)y, (int) width, (int) height, GetColor(hex_color));
    return true;
}

bool rdn_get_width(RDNApi* api) {
    api->push_integer(api , (int)GetScreenWidth());
    return true;
}

bool rdn_get_height(RDNApi* api) {
    api->push_integer(api , (int)GetScreenHeight());
    return true;
}

int main(void)
{
    RDNState stack = {0};
    Vars vars = {0};
    Funcs funcs = {0};

    ray_append(&funcs, create_native_func_entry("init-window", 
                rdn_init_window, NULL));


    ray_append(&funcs, create_native_func_entry("close-window", 
                rdn_close_window, NULL));


    ray_append(&funcs, create_native_func_entry("window-should-close", 
                rdn_window_should_close, NULL));

    ray_append(&funcs, create_native_func_entry("begin-drawing", 
                rdn_begin_drawing, NULL));

    ray_append(&funcs, create_native_func_entry("End-drawing", 
                rdn_end_drawing, NULL));


    ray_append(&funcs, create_native_func_entry("clear-background", 
                rdn_clear_bg, NULL));

    ray_append(&funcs, create_native_func_entry("draw-rect", 
                rdn_draw_rect, NULL));

    ray_append(&funcs, create_native_func_entry("get-screen-width", 
                rdn_get_width, NULL));

    ray_append(&funcs, create_native_func_entry("get-screen-height", 
                rdn_get_height, NULL));

    char* src = read_file("main.rdn");
    evaluate_source(&stack, &vars, &funcs, src);
    return 0;
}

int main2(void)
{
    InitWindow(800, 600, "test");

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RED);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
#include "../../src/src.c"
