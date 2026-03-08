// Lucky Wheel - Flipper Zero App (Unleashed Firmware)
// Spin once a day for prizes!

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <dolphin/dolphin.h>
#include <storage/storage.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define APP_NAME     "Lucky Wheel"
#define DATA_PATH    APP_DATA_PATH("lucky_wheel.dat")
#define WHEEL_CX     54
#define WHEEL_CY     32
#define WHEEL_R      26
#define SEG_COUNT    8
#define SPIN_FRAMES  60

typedef enum {
    PrizeCoin10 = 0,
    PrizeCoin50,
    PrizeCoin100,
    PrizeCoinBonus,
    PrizeXP,
    PrizeLevel,
    PrizeCheat,
    PrizeNothing,
} PrizeType;

static const char* prize_names[] = {
    "10 Coins!",
    "50 Coins!",
    "100 Coins!",
    "150 Coins!",
    "XP Boost!",
    "LEVEL UP!!",
    "Cheat Code!",
    "Try Again...",
};

static const char* cheat_pool[] = {
    "FLIPGOLD", "DARKSIDE", "DOLPHIN8", "XPBOOST1",
    "COINRUSH",  "NEONWAVE", "HACKTIME", "SECRETZ9",
};
#define CHEAT_POOL_SIZE 8
#define CHEAT_LEN 9

// Background skin IDs
#define BG_COUNT 6
static const char* bg_names[]    = {"Default","Dots","Checker","Stripes","Grid","Stars"};
static const uint32_t bg_costs[] = {0, 100, 250, 400, 500, 750};

typedef struct {
    uint64_t last_spin_timestamp;
    uint32_t coins;
    char     last_cheat[CHEAT_LEN];
    uint8_t  cheat_coinrush;
    uint8_t  cheat_xpboost;
    uint8_t  cheat_extra_spin;
    uint8_t  cheat_levelup;
    uint8_t  cheat_neonwave;
    uint8_t  bg_owned;   // bitmask of owned backgrounds (bit 0 = default always owned)
    uint8_t  bg_active;  // currently equipped background id
} SaveData;

typedef struct {
    SaveData  save;
    bool      can_spin;
    bool      spinning;
    bool      show_result;
    uint8_t   spin_frame;
    float     spin_angle;
    float     spin_speed;
    PrizeType won_prize;

    ViewPort*        view_port;
    Gui*             gui;
    FuriMessageQueue* event_queue;
    NotificationApp* notifications;
    FuriTimer*       timer;
    bool             running;

    // Cheat code entry
    bool     cheat_mode;
    char     cheat_input[CHEAT_LEN];
    uint8_t  cheat_pos;
    char     cheat_msg[32];

    // Background shop
    bool     shop_mode;
    uint8_t  shop_cursor; // which bg we are browsing
} AppState;

// ── Persistence ───────────────────────────────────────────────────────────────

static void app_save(AppState* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, DATA_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, &app->save, sizeof(SaveData));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void app_load(AppState* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, DATA_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_read(file, &app->save, sizeof(SaveData));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// ── Date ──────────────────────────────────────────────────────────────────────

static uint64_t get_timestamp(void) {
    return furi_hal_rtc_get_timestamp();
}

#define SECONDS_IN_24H (24ULL * 60ULL * 60ULL)

// ── Prize ─────────────────────────────────────────────────────────────────────

static PrizeType pick_prize_with_cheats(AppState* app) {
    // Check guaranteed prizes from cheats first
    if(app->save.cheat_levelup) {
        app->save.cheat_levelup = 0;
        return PrizeLevel;
    }
    if(app->save.cheat_xpboost) {
        app->save.cheat_xpboost = 0;
        return PrizeXP;
    }
    // Normal weighted roll
    uint32_t r = furi_hal_random_get() % 1000;
    if(r < 100) return PrizeLevel;
    if(r < 300) return PrizeXP;
    if(r < 370) return PrizeCheat;
    if(r < 450) return PrizeCoinBonus;
    if(r < 550) return PrizeCoin100;
    if(r < 750) return PrizeCoin50;
    if(r < 930) return PrizeCoin10;
    return PrizeNothing;
}

static uint32_t prize_coins_with_cheats(PrizeType p, AppState* app) {
    uint32_t base = 0;
    switch(p) {
        case PrizeCoin10:    base = 10;  break;
        case PrizeCoin50:    base = 50;  break;
        case PrizeCoin100:   base = 100; break;
        case PrizeCoinBonus: base = 100 + (furi_hal_random_get() % 51); break;
        default:             base = 0;   break;
    }
    if(base > 0) {
        if(app->save.cheat_neonwave) { app->save.cheat_neonwave = 0; base *= 3; }
        else if(app->save.cheat_coinrush) { app->save.cheat_coinrush = 0; base *= 2; }
    }
    return base;
}


// ── Cheat Code Processor ──────────────────────────────────────────────────────

static void apply_cheat(AppState* app) {
    char* code = app->cheat_input;

    if(strncmp(code, "FLIPGOLD", 8) == 0) {
        app->save.coins += 500;
        strncpy(app->cheat_msg, "+500 COINS!", sizeof(app->cheat_msg));
    } else if(strncmp(code, "COINRUSH", 8) == 0) {
        app->save.cheat_coinrush = 1;
        strncpy(app->cheat_msg, "2X COINS NEXT SPIN!", sizeof(app->cheat_msg));
    } else if(strncmp(code, "NEONWAVE", 8) == 0) {
        app->save.cheat_neonwave = 1;
        strncpy(app->cheat_msg, "3X COINS NEXT SPIN!", sizeof(app->cheat_msg));
    } else if(strncmp(code, "XPBOOST1", 8) == 0) {
        app->save.cheat_xpboost = 1;
        strncpy(app->cheat_msg, "GUARANTEED XP!", sizeof(app->cheat_msg));
    } else if(strncmp(code, "HACKTIME", 8) == 0) {
        app->save.cheat_extra_spin = 1;
        app->can_spin = true;
        app->save.last_spin_timestamp = 0;
        strncpy(app->cheat_msg, "EXTRA SPIN!", sizeof(app->cheat_msg));
    } else if(strncmp(code, "DOLPHIN8", 8) == 0) {
        Dolphin* dolphin = furi_record_open(RECORD_DOLPHIN);
        for(int i = 0; i < 200; i++) dolphin_deed(DolphinDeedPluginGameWin);
        dolphin_flush(dolphin);
        furi_record_close(RECORD_DOLPHIN);
        strncpy(app->cheat_msg, "DOLPHIN LEVELED!", sizeof(app->cheat_msg));
    } else if(strncmp(code, "DARKSIDE", 8) == 0) {
        app->save.cheat_levelup = 1;
        strncpy(app->cheat_msg, "GUARANTEED LEVEL!", sizeof(app->cheat_msg));
    } else if(strncmp(code, "SECRETZ9", 8) == 0) {
        app->save.coins += 999;
        app->save.cheat_coinrush = 1;
        app->save.cheat_extra_spin = 1;
        app->can_spin = true;
        app->save.last_spin_timestamp = 0;
        strncpy(app->cheat_msg, "JACKPOT MODE!", sizeof(app->cheat_msg));
    } else {
        strncpy(app->cheat_msg, "INVALID CODE!", sizeof(app->cheat_msg));
    }

    app_save(app);
    memset(app->cheat_input, 0, CHEAT_LEN);
    app->cheat_pos  = 0;
    app->cheat_mode = false;
}


// ── Background Renderer ───────────────────────────────────────────────────────
static void draw_background(Canvas* canvas, uint8_t bg_id) {
    if(bg_id == 0) return; // default = blank

    if(bg_id == 1) { // Dots
        for(int x = 4; x < 88; x += 8)
            for(int y = 4; y < 64; y += 8)
                canvas_draw_dot(canvas, x, y);

    } else if(bg_id == 2) { // Checkerboard
        for(int x = 0; x < 88; x += 8)
            for(int y = 0; y < 64; y += 8)
                if(((x/8)+(y/8)) % 2 == 0)
                    canvas_draw_box(canvas, x, y, 4, 4);

    } else if(bg_id == 3) { // Diagonal stripes
        for(int d = 0; d < 152; d += 6) {
            int x1 = d < 88 ? d : 88;
            int y1 = d < 88 ? 0 : d - 88;
            int x2 = d < 64 ? 0 : d - 64;
            int y2 = d < 64 ? d : 64;
            if(x1 < 88 && y2 <= 64)
                canvas_draw_line(canvas, x1, y1, x2, y2);
        }

    } else if(bg_id == 4) { // Grid
        for(int x = 0; x < 88; x += 10)
            canvas_draw_line(canvas, x, 0, x, 63);
        for(int y = 0; y < 64; y += 10)
            canvas_draw_line(canvas, 0, y, 87, y);

    } else if(bg_id == 5) { // Stars
        // Simple star pattern using fixed positions
        static const uint8_t sx[] = {5,15,28,42,55,68,80,10,22,36,50,64,76,3,18,33,47,61,75};
        static const uint8_t sy[] = {5,18,8,22,6,15,28,40,55,48,35,58,42,32,45,60,20,38,52};
        for(int i = 0; i < 19; i++) {
            if(sx[i] < 88) {
                canvas_draw_dot(canvas, sx[i],   sy[i]);
                canvas_draw_dot(canvas, sx[i]+1, sy[i]);
                canvas_draw_dot(canvas, sx[i],   sy[i]+1);
                canvas_draw_dot(canvas, sx[i]-1, sy[i]);
                canvas_draw_dot(canvas, sx[i],   sy[i]-1);
            }
        }
    }
}

// ── Shop Screen ───────────────────────────────────────────────────────────────
static void draw_shop(Canvas* canvas, AppState* app) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Title
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "BG SHOP");
    canvas_draw_line(canvas, 0, 13, 128, 13);

    uint8_t id = app->shop_cursor;
    bool owned = (app->save.bg_owned >> id) & 1;

    // Preview area (left side 88px wide, rows 15-55)
    canvas_draw_rframe(canvas, 1, 15, 86, 40, 2);
    // Clip preview background to box
    if(id > 0) {
        // Simplified preview — just draw the pattern scaled down in the box
        canvas_set_color(canvas, ColorBlack);
        if(id == 1) { // dots
            for(int x=5; x<85; x+=8) for(int y=19; y<53; y+=8) canvas_draw_dot(canvas,x,y);
        } else if(id == 2) { // checker
            for(int x=2; x<85; x+=8) for(int y=16; y<54; y+=8)
                if(((x/8)+(y/8))%2==0) canvas_draw_box(canvas,x,y,4,4);
        } else if(id == 3) { // stripes
            for(int d=0; d<130; d+=6) {
                int x1=d<86?d:86, y1=d<86?15:d-71;
                int x2=d<40?2:d-38, y2=d<40?d+15:55;
                if(x1<87&&y2<=55) canvas_draw_line(canvas,x1,y1,x2,y2);
            }
        } else if(id == 4) { // grid
            for(int x=1; x<87; x+=10) canvas_draw_line(canvas,x,15,x,55);
            for(int y=15; y<56; y+=10) canvas_draw_line(canvas,1,y,86,y);
        } else if(id == 5) { // stars
            static const uint8_t px[]={10,25,40,55,70,82,15,35,60,78};
            static const uint8_t py[]={20,32,22,40,28,18,48,52,44,50};
            for(int i=0;i<10;i++){
                canvas_draw_dot(canvas,px[i],py[i]);
                canvas_draw_dot(canvas,px[i]+1,py[i]);
                canvas_draw_dot(canvas,px[i]-1,py[i]);
                canvas_draw_dot(canvas,px[i],py[i]+1);
                canvas_draw_dot(canvas,px[i],py[i]-1);
            }
        }
    }
    canvas_draw_rframe(canvas, 1, 15, 86, 40, 2); // redraw border on top

    // Nav arrows
    if(id > 0)           canvas_draw_str(canvas, 2,  12, "<");
    if(id < BG_COUNT-1)  canvas_draw_str(canvas, 120, 12, ">");

    // Right info panel
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(canvas, 91, 18, bg_names[id]);

    char cbuf[20];
    if(owned) {
        canvas_draw_str(canvas, 91, 28, "OWNED");
        if(app->save.bg_active == id)
            canvas_draw_str(canvas, 91, 38, "ACTIVE");
        else
            canvas_draw_str(canvas, 91, 38, "OK=USE");
    } else {
        snprintf(cbuf, sizeof(cbuf), "%lu c", (unsigned long)bg_costs[id]);
        canvas_draw_str(canvas, 91, 28, cbuf);
        if(app->save.coins >= bg_costs[id])
            canvas_draw_str(canvas, 91, 38, "OK=BUY");
        else
            canvas_draw_str(canvas, 91, 38, "NO $$$");
    }

    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str_aligned(canvas, 64, 57, AlignCenter, AlignTop, "BACK=exit");
}

// ── Draw ──────────────────────────────────────────────────────────────────────

static void draw_callback(Canvas* canvas, void* ctx) {
    AppState* app = ctx;
    furi_assert(app);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(app->shop_mode) {
        draw_shop(canvas, app);
        return;
    }

    // Draw active background pattern
    draw_background(canvas, app->save.bg_active);
    canvas_set_color(canvas, ColorBlack);

    if(app->show_result) {
        // Result screen - big bold winner display
        canvas_draw_rframe(canvas, 0, 0, 128, 64, 4);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignTop, "YOU WON!");
        canvas_draw_line(canvas, 4, 18, 124, 18);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignTop,
                                prize_names[app->won_prize]);
        char buf[32];
        snprintf(buf, sizeof(buf), "Total: %lu coins", (unsigned long)app->save.coins);
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignTop, buf);
        if(app->won_prize == PrizeCheat && app->save.last_cheat[0]) {
            canvas_set_font(canvas, FontKeyboard);
            canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignTop,
                                    app->save.last_cheat);
        }
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignTop, "[OK] Continue");
        return;
    }

    // ── Wheel ──────────────────────────────────────────────
    float seg_angle = (float)(2.0 * M_PI / SEG_COUNT);
    static const char* seg_labels[] = {
        "10c","XP","50c","???","100c","LVL","CHT","150c"
    };

    // Draw filled alternating segments (checkerboard pattern)
    for(int s = 0; s < SEG_COUNT; s++) {
        float a_start = app->spin_angle + s * seg_angle;
        float a_mid   = a_start + seg_angle * 0.5f;

        // Draw segment divider lines
        int x1 = WHEEL_CX + (int)(WHEEL_R * cosf(a_start));
        int y1 = WHEEL_CY + (int)(WHEEL_R * sinf(a_start));
        canvas_draw_line(canvas, WHEEL_CX, WHEEL_CY, x1, y1);

        // Fill every other segment solid (like a real prize wheel)
        if(s % 2 == 0) {
            // Dense fill to simulate solid segment
            for(float r2 = 2; r2 < WHEEL_R - 1; r2 += 1.5f) {
                for(float a2 = a_start + 0.05f; a2 < a_start + seg_angle - 0.05f; a2 += 0.9f / r2) {
                    int px = WHEEL_CX + (int)(r2 * cosf(a2));
                    int py = WHEEL_CY + (int)(r2 * sinf(a2));
                    canvas_draw_dot(canvas, px, py);
                }
            }
        }

        // Draw label in each segment
        float lx = WHEEL_CX + (WHEEL_R * 0.65f) * cosf(a_mid);
        float ly = WHEEL_CY + (WHEEL_R * 0.65f) * sinf(a_mid);
        canvas_set_font(canvas, FontKeyboard);

        // Invert text color for filled segments so it stays readable
        if(s % 2 == 0) {
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }
        canvas_draw_str(canvas, (int)lx - 7, (int)ly + 3, seg_labels[s]);
        canvas_set_color(canvas, ColorBlack);
    }

    // Outer ring (thick border like real wheel)
    canvas_draw_circle(canvas, WHEEL_CX, WHEEL_CY, WHEEL_R);
    canvas_draw_circle(canvas, WHEEL_CX, WHEEL_CY, WHEEL_R - 1);

    // Bullseye center (3 rings like in the photo)
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_disc(canvas, WHEEL_CX, WHEEL_CY, 7);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_circle(canvas, WHEEL_CX, WHEEL_CY, 7);
    canvas_draw_circle(canvas, WHEEL_CX, WHEEL_CY, 4);
    canvas_draw_disc(canvas, WHEEL_CX, WHEEL_CY, 2);

    // Triangle pointer at TOP of wheel (like the photo)
    // Points downward into the wheel
    int px = WHEEL_CX;
    int pt = WHEEL_CY - WHEEL_R - 1;
    canvas_draw_line(canvas, px - 5, pt - 7, px + 5, pt - 7); // top of triangle
    canvas_draw_line(canvas, px - 5, pt - 7, px,     pt);      // left side
    canvas_draw_line(canvas, px + 5, pt - 7, px,     pt);      // right side
    // Fill triangle
    for(int ty = pt - 6; ty < pt; ty++) {
        int hw = (int)((float)(pt - ty) * 5.0f / 7.0f);
        canvas_draw_line(canvas, px - hw, ty, px + hw, ty);
    }

    // ── Right side info panel ──────────────────────────────
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, 88, 0, 40, 64, 2);

    char coin_buf[16];
    snprintf(coin_buf, sizeof(coin_buf), "%lu", (unsigned long)app->save.coins);
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str_aligned(canvas, 108, 4, AlignCenter, AlignTop, "COINS");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 108, 14, AlignCenter, AlignTop, coin_buf);
    canvas_draw_line(canvas, 90, 24, 126, 24);

    if(app->spinning) {
        canvas_draw_str_aligned(canvas, 108, 28, AlignCenter, AlignTop, "...");
    } else if(app->can_spin) {
        canvas_set_font(canvas, FontKeyboard);
        canvas_draw_str_aligned(canvas, 108, 26, AlignCenter, AlignTop, "OK=SPIN");
        canvas_draw_str_aligned(canvas, 108, 36, AlignCenter, AlignTop, "UP=SHOP");
        canvas_draw_str_aligned(canvas, 108, 46, AlignCenter, AlignTop, "RT=CHEAT");
    } else {
        canvas_set_font(canvas, FontKeyboard);
        canvas_draw_str_aligned(canvas, 108, 30, AlignCenter, AlignTop, "COME");
        canvas_draw_str_aligned(canvas, 108, 40, AlignCenter, AlignTop, "BACK");
        canvas_draw_str_aligned(canvas, 108, 50, AlignCenter, AlignTop, "TMRW");
    }
}

// ── Input ─────────────────────────────────────────────────────────────────────

static void input_callback(InputEvent* event, void* ctx) {
    AppState* app = ctx;
    furi_assert(app);
    furi_message_queue_put(app->event_queue, event, FuriWaitForever);
}

// ── Timer ─────────────────────────────────────────────────────────────────────

static void timer_callback(void* ctx) {
    AppState* app = ctx;
    furi_assert(app);

    if(!app->spinning) return;

    app->spin_angle += app->spin_speed;
    app->spin_speed *= 0.93f;
    app->spin_frame++;

    if(app->spin_frame >= SPIN_FRAMES && app->spin_speed < 0.05f) {
        app->spinning    = false;
        app->show_result = true;

        uint32_t c = prize_coins_with_cheats(app->won_prize, app);
        app->save.coins += c;

        if(app->won_prize == PrizeXP) {
            // Give real XP via multiple deeds + flush so it applies immediately
            app->save.coins += 25;
            Dolphin* dolphin = furi_record_open(RECORD_DOLPHIN);
            for(int i = 0; i < 50; i++) dolphin_deed(DolphinDeedPluginGameWin);
            dolphin_flush(dolphin);
            furi_record_close(RECORD_DOLPHIN);
        } else if(app->won_prize == PrizeLevel) {
            app->save.coins += 200;
            Dolphin* dolphin = furi_record_open(RECORD_DOLPHIN);
            // Spam 200 deeds (~1600 XP) — enough to guarantee a level up from any level
            for(int i = 0; i < 200; i++) dolphin_deed(DolphinDeedPluginGameWin);
            dolphin_flush(dolphin);
            furi_record_close(RECORD_DOLPHIN);

        } else if(app->won_prize == PrizeCheat) {
            uint8_t ci = (uint8_t)(furi_hal_random_get() % CHEAT_POOL_SIZE);
            strncpy(app->save.last_cheat, cheat_pool[ci], CHEAT_LEN - 1);
            app->save.last_cheat[CHEAT_LEN - 1] = '\0';
        } else if(app->won_prize == PrizeNothing) {
            app->can_spin = true;
            app->save.last_spin_timestamp = 0;
        }

        app_save(app);
        notification_message(app->notifications, &sequence_success);
    }

    view_port_update(app->view_port);
}

// ── Main ──────────────────────────────────────────────────────────────────────

int32_t lucky_wheel_app(void* p) {
    UNUSED(p);

    AppState* app = malloc(sizeof(AppState));
    furi_assert(app);
    memset(app, 0, sizeof(AppState));

    app->running     = true;
    app->spin_angle  = 0.0f;
    app->can_spin    = true;

    app_load(app);

    app->save.bg_owned |= 0x01; // always own default
    uint64_t now = get_timestamp();
    if(app->save.last_spin_timestamp != 0 &&
       (now - app->save.last_spin_timestamp) < SECONDS_IN_24H) {
        app->can_spin = false;
    }

    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    furi_assert(app->event_queue);

    app->view_port = view_port_alloc();
    furi_assert(app->view_port);
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    furi_assert(app->gui);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    furi_assert(app->notifications);

    app->timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, app);
    furi_assert(app->timer);
    furi_timer_start(app->timer, 33);

    // Event loop
    InputEvent event;
    while(app->running) {
        if(furi_message_queue_get(app->event_queue, &event, 100) == FuriStatusOk) {
            if(event.type == InputTypeShort || event.type == InputTypeLong) {

                if(app->show_result) {
                    if(event.key == InputKeyOk || event.key == InputKeyBack) {
                        app->show_result = false;
                        memset(app->cheat_msg, 0, sizeof(app->cheat_msg));
                        view_port_update(app->view_port);
                    }
                } else if(app->shop_mode) {
                    if(event.key == InputKeyBack) {
                        app->shop_mode = false;
                    } else if(event.key == InputKeyLeft && app->shop_cursor > 0) {
                        app->shop_cursor--;
                    } else if(event.key == InputKeyRight && app->shop_cursor < BG_COUNT-1) {
                        app->shop_cursor++;
                    } else if(event.key == InputKeyOk) {
                        uint8_t id  = app->shop_cursor;
                        bool owned  = (app->save.bg_owned >> id) & 1;
                        if(owned) {
                            app->save.bg_active = id;
                            app->shop_mode = false;
                            app_save(app);
                        } else if(app->save.coins >= bg_costs[id]) {
                            app->save.coins    -= bg_costs[id];
                            app->save.bg_owned |= (1 << id);
                            app->save.bg_active = id;
                            app->shop_mode = false;
                            app_save(app);
                        }
                    }
                    view_port_update(app->view_port);
                } else if(app->cheat_mode) {
                    // Cheat code entry: up/down/left/right/ok = A-Z input
                    // Use directional keys to cycle letters, OK to confirm, Back to delete
                    static const char key_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
                    static uint8_t char_idx = 0;
                    if(event.key == InputKeyBack) {
                        if(app->cheat_pos > 0) {
                            app->cheat_pos--;
                            app->cheat_input[app->cheat_pos] = 0;
                        } else {
                            app->cheat_mode = false;
                        }
                    } else if(event.key == InputKeyUp) {
                        char_idx = (char_idx + 1) % (uint8_t)strlen(key_chars);
                        if(app->cheat_pos < CHEAT_LEN-1)
                            app->cheat_input[app->cheat_pos] = key_chars[char_idx];
                    } else if(event.key == InputKeyDown) {
                        char_idx = (char_idx == 0) ? (uint8_t)(strlen(key_chars)-1) : char_idx-1;
                        if(app->cheat_pos < CHEAT_LEN-1)
                            app->cheat_input[app->cheat_pos] = key_chars[char_idx];
                    } else if(event.key == InputKeyOk) {
                        if(app->cheat_pos < CHEAT_LEN-1 && app->cheat_input[app->cheat_pos]) {
                            app->cheat_pos++;
                            char_idx = 0;
                        }
                        if(app->cheat_pos >= 8) {
                            apply_cheat(app);
                        }
                    } else if(event.key == InputKeyRight) {
                        // Confirm current char and move forward
                        if(app->cheat_pos < CHEAT_LEN-1 && app->cheat_input[app->cheat_pos]) {
                            app->cheat_pos++;
                            char_idx = 0;
                        }
                    }
                    view_port_update(app->view_port);
                } else {
                    if(event.key == InputKeyBack) {
                        app->running = false;
                    } else if(event.key == InputKeyRight && !app->spinning) {
                        app->cheat_mode = true;
                        app->cheat_pos  = 0;
                        memset(app->cheat_input, 0, CHEAT_LEN);
                        view_port_update(app->view_port);
                    } else if(event.key == InputKeyUp && !app->spinning) {
                        app->shop_mode   = true;
                        app->shop_cursor = app->save.bg_active;
                        view_port_update(app->view_port);
                    } else if(event.key == InputKeyOk && !app->spinning && app->can_spin) {
                        app->spinning    = true;
                        app->spin_frame  = 0;
                        app->spin_speed  = 0.35f + (float)(furi_hal_random_get() % 30) / 100.0f;
                        app->won_prize   = pick_prize_with_cheats(app);
                        app->save.last_spin_timestamp = get_timestamp();
                        app->can_spin    = false;
                        app_save(app); // Save immediately so closing mid-spin still locks the day
                        notification_message(app->notifications, &sequence_single_vibro);
                        view_port_update(app->view_port);
                    }
                }
            }
        }
    }

    // Cleanup
    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_message_queue_free(app->event_queue);
    free(app);

    return 0;
}