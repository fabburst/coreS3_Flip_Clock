#include <M5Unified.h>
#include <lvgl.h>
#include <WiFi.h>
#include "secrets.h"

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[320 * 20];
lv_obj_t *card_h, *card_m, *info_label;
int last_h = -1, last_m = -1;

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    M5.Display.pushImage(area->x1, area->y1, w, h, (uint16_t *)color_p);
    lv_disp_flush_ready(disp);
}

lv_obj_t* create_clock_unit(lv_obj_t* parent, int x) {
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 140, 180);
    lv_obj_set_pos(cont, x, 30);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_radius(cont, 12, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* label = lv_label_create(cont);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label); 
    lv_label_set_text(label, "00");
    return cont;
}

void play_flip_anim(lv_obj_t* card, int new_val) {
    static char text_buf[12][3];
    static int b_idx = 0;
    b_idx = (b_idx + 1) % 12;
    sprintf(text_buf[b_idx], "%02d", new_val);
    lv_obj_set_user_data(card, (void*)text_buf[b_idx]);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, card);
    lv_anim_set_values(&a, 180, 2); 
    lv_anim_set_time(&a, 300);
    lv_anim_set_exec_cb(&a, [](void* var, int32_t v) { lv_obj_set_height((lv_obj_t*)var, v); });
    
    lv_anim_set_ready_cb(&a, [](lv_anim_t* anim) {
        lv_obj_t* c = (lv_obj_t*)anim->var;
        const char* txt = (const char*)lv_obj_get_user_data(c);
        if(txt) lv_label_set_text(lv_obj_get_child(c, 0), txt);
        
        lv_anim_t a_open;
        lv_anim_init(&a_open);
        lv_anim_set_var(&a_open, c);
        lv_anim_set_values(&a_open, 2, 180);
        lv_anim_set_time(&a_open, 300);
        lv_anim_set_exec_cb(&a_open, [](void* var, int32_t v) { lv_obj_set_height((lv_obj_t*)var, v); });
        lv_anim_start(&a_open);
    });
    lv_anim_start(&a);
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setBrightness(120);
    M5.Display.fillScreen(TFT_BLACK);

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, 320 * 20);
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 320; disp_drv.ver_res = 240;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
    info_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(info_label, lv_color_hex(0x666666), 0);
    lv_obj_align(info_label, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_label_set_text(info_label, "Synchro WiFi...");
    lv_timer_handler();

    WiFi.begin(ssid, password);
    unsigned long start_wifi = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start_wifi < 3000)) { delay(100); }

    if (WiFi.status() == WL_CONNECTED) {
        // Règle précise pour la France (CET = GMT+1, CEST = GMT+2)
        configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.google.com");
        
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 5000)) { 
            M5.Rtc.setDateTime(&timeinfo);
        }
    }

    card_h = create_clock_unit(lv_scr_act(), 15);
    card_m = create_clock_unit(lv_scr_act(), 165);

    // Lecture du RTC
    auto dt = M5.Rtc.getDateTime();
    last_h = dt.time.hours;
    last_m = dt.time.minutes;
    lv_label_set_text_fmt(lv_obj_get_child(card_h, 0), "%02d", last_h);
    lv_label_set_text_fmt(lv_obj_get_child(card_m, 0), "%02d", last_m);

    lv_timer_create([](lv_timer_t* t){
        auto dt = M5.Rtc.getDateTime();
        if (dt.time.minutes != last_m) {
            if (dt.time.hours != last_h) {
                play_flip_anim(card_h, dt.time.hours);
                last_h = dt.time.hours;
            }
            play_flip_anim(card_m, dt.time.minutes);
            last_m = dt.time.minutes; 
        }
    }, 1000, NULL);

    lv_timer_create([](lv_timer_t* t){ lv_obj_add_flag(info_label, LV_OBJ_FLAG_HIDDEN); }, 5000, NULL);
}

void loop() {
    M5.update();
    lv_timer_handler();
    delay(5);
}