#include <M5Unified.h>
#include <lvgl.h>
#include <WiFi.h>
#include <Adafruit_NeoPixel.h> 
#include "secret.h"

// --- CONFIGURATION ---
#define LED_PIN     9      
#define NUM_LEDS    10     
Adafruit_NeoPixel baseLeds(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// --- Variables Globales LVGL ---
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[320 * 20];
lv_obj_t *card_h, *card_m, *info_label;
int last_h = -1, last_m = -1;
lv_obj_t *bat_cont, *bat_label, *bat_icon_group, *bat_body, *bat_fill, *bat_nub, *bat_bolt;

// --- Flush LVGL ---
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    M5.Display.pushImage(area->x1, area->y1, w, h, (uint16_t *)color_p);
    lv_disp_flush_ready(disp);
}

// --- Création unité horloge ---
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

// --- Widget Batterie ---
void create_battery_widget() {
    bat_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(bat_cont, LV_SIZE_CONTENT, 30); 
    lv_obj_align(bat_cont, LV_ALIGN_TOP_RIGHT, -8, 5);
    lv_obj_set_style_bg_opa(bat_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bat_cont, 0, 0);
    lv_obj_clear_flag(bat_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(bat_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bat_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(bat_cont, 0, 0);
    lv_obj_set_style_pad_gap(bat_cont, 6, 0); 

    bat_label = lv_label_create(bat_cont);
    lv_obj_set_style_text_font(bat_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(bat_label, lv_color_white(), 0);
    lv_label_set_text(bat_label, "");

    bat_icon_group = lv_obj_create(bat_cont);
    lv_obj_set_size(bat_icon_group, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(bat_icon_group, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bat_icon_group, 0, 0);
    lv_obj_set_style_pad_all(bat_icon_group, 0, 0);
    lv_obj_set_flex_flow(bat_icon_group, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bat_icon_group, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(bat_icon_group, 1, 0);

    bat_body = lv_obj_create(bat_icon_group);
    lv_obj_set_size(bat_body, 26, 14);
    lv_obj_set_style_bg_opa(bat_body, LV_OPA_TRANSP, 0); 
    lv_obj_set_style_border_color(bat_body, lv_color_white(), 0);
    lv_obj_set_style_border_width(bat_body, 2, 0);
    lv_obj_set_style_radius(bat_body, 4, 0);
    lv_obj_set_style_pad_all(bat_body, 1, 0); 
    lv_obj_clear_flag(bat_body, LV_OBJ_FLAG_SCROLLABLE);
    
    bat_fill = lv_obj_create(bat_body);
    lv_obj_set_height(bat_fill, lv_pct(100));
    lv_obj_set_width(bat_fill, 0);
    lv_obj_align(bat_fill, LV_ALIGN_LEFT_MID, 0, 0); 
    lv_obj_set_style_bg_color(bat_fill, lv_color_white(), 0);
    lv_obj_set_style_border_width(bat_fill, 0, 0);
    lv_obj_set_style_radius(bat_fill, 1, 0);

    bat_bolt = lv_label_create(bat_body);
    lv_obj_set_style_text_color(bat_bolt, lv_color_black(), 0); 
    lv_label_set_text(bat_bolt, LV_SYMBOL_CHARGE); 
    lv_obj_center(bat_bolt); 
    lv_obj_add_flag(bat_bolt, LV_OBJ_FLAG_HIDDEN); 

    bat_nub = lv_obj_create(bat_icon_group); 
    lv_obj_set_size(bat_nub, 3, 6);
    lv_obj_set_style_bg_color(bat_nub, lv_color_white(), 0);
    lv_obj_set_style_border_width(bat_nub, 0, 0);
    lv_obj_set_style_radius(bat_nub, 2, 0);
}

// --- LOGIQUE UNIFIÉE (LEDs + UI) ---
void update_battery_display() {
    int32_t bat_voltage = M5.Power.getBatteryVoltage();
    int32_t vbus = M5.Power.getVBUSVoltage(); 
    int level = M5.Power.getBatteryLevel();

    // 1. DÉTECTION DE L'ÉTAT "CONNECTÉ"
    // Soit il y a du jus (VBUS > 1V)
    // Soit on est à 100% (La base a coupé le jus, mais on force l'état connecté)
    bool has_power_input = (vbus > 1000);
    bool is_full = (level == 100);
    
    // C'est ici que je corrige : la variable "connected_mode" pilote TOUT (LEDs et Écran)
    bool connected_mode = has_power_input || is_full;

    // 2. GESTION LED BASE (Reliée au connected_mode)
    if (connected_mode) {
        // Mode Pulsation (Respiration)
        // Comme VBUS peut être à 0, c'est la batterie interne qui alimente les LEDs
        int brightness = (int)(120.0 + 120.0 * sin(millis() / 500.0));
        if (brightness < 0) brightness = 0;
        if (brightness > 255) brightness = 255;
        
        for(int i=0; i<NUM_LEDS; i++) baseLeds.setPixelColor(i, baseLeds.Color(0, brightness, 0));
        baseLeds.show();
    } else {
        // Éteint
        baseLeds.clear();
        baseLeds.show();
    }

    // 3. DEBUG VISUEL
    if (info_label) {
        lv_obj_clear_flag(info_label, LV_OBJ_FLAG_HIDDEN);
        
        // J'ajoute un indicateur textuel pour comprendre pourquoi c'est vert
        const char* status = "";
        if (has_power_input) status = "PWR";
        else if (is_full) status = "FULL";
        else status = "BAT";

        lv_label_set_text_fmt(info_label, "BAT:%dmV IN:%dmV [%s]", bat_voltage, vbus, status);
        
        // Couleur texte debug
        if (connected_mode) lv_obj_set_style_text_color(info_label, lv_color_hex(0x00FF00), 0); // Vert
        else lv_obj_set_style_text_color(info_label, lv_color_hex(0xFF0000), 0); // Rouge
    }

    // --- WIDGET AFFICHAGE ---
    
    // ÉTAT 1 : Vraiment rien (Pas de batterie, juste câble USB sans le reste)
    // Note : Avec la logique 100%, on risque de cacher si voltage < 3200.
    // Mais si voltage < 3200, on n'est jamais à 100%, donc pas de risque.
    if (bat_voltage < 3200) {
        if (!lv_obj_has_flag(bat_cont, LV_OBJ_FLAG_HIDDEN)) lv_obj_add_flag(bat_cont, LV_OBJ_FLAG_HIDDEN);
        return; 
    }
    if (lv_obj_has_flag(bat_cont, LV_OBJ_FLAG_HIDDEN)) lv_obj_clear_flag(bat_cont, LV_OBJ_FLAG_HIDDEN);

    if (level > 100) level = 100;
    if (level < 0) level = 0;
    lv_label_set_text_fmt(bat_label, "%d%%", level);
    
    int max_w = 22; 
    int w = (level * max_w) / 100;
    if (level > 0 && w < 2) w = 2;
    lv_obj_set_width(bat_fill, w);

    // --- COULEURS ---
    if (connected_mode) {
        // VERT + ÉCLAIR (Car on est soit alimenté, soit à 100% sur base)
        lv_color_t c_green = lv_color_hex(0x3DDC84);
        lv_obj_set_style_bg_color(bat_fill, c_green, 0);
        lv_obj_set_style_border_color(bat_body, c_green, 0);
        lv_obj_set_style_text_color(bat_label, c_green, 0);
        lv_obj_set_style_bg_color(bat_nub, c_green, 0);
        if (lv_obj_has_flag(bat_bolt, LV_OBJ_FLAG_HIDDEN)) lv_obj_clear_flag(bat_bolt, LV_OBJ_FLAG_HIDDEN);
    } 
    else {
        // BLANC / ROUGE (Décharge)
        if (!lv_obj_has_flag(bat_bolt, LV_OBJ_FLAG_HIDDEN)) lv_obj_add_flag(bat_bolt, LV_OBJ_FLAG_HIDDEN);
        if (level < 20) {
            lv_color_t c_red = lv_color_hex(0xFF4444);
            lv_obj_set_style_bg_color(bat_fill, c_red, 0);
            lv_obj_set_style_border_color(bat_body, lv_color_white(), 0);
            lv_obj_set_style_text_color(bat_label, lv_color_white(), 0);
            lv_obj_set_style_bg_color(bat_nub, lv_color_white(), 0);
        } else {
            lv_obj_set_style_bg_color(bat_fill, lv_color_white(), 0);
            lv_obj_set_style_border_color(bat_body, lv_color_white(), 0);
            lv_obj_set_style_text_color(bat_label, lv_color_white(), 0);
            lv_obj_set_style_bg_color(bat_nub, lv_color_white(), 0);
        }
    }
}

// --- Animation Flip ---
void play_flip_anim(lv_obj_t* card, int new_val) {
    static char text_buf[24][4]; 
    static int b_idx = 0;
    b_idx = (b_idx + 1) % 24;
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

// --- Setup Principal ---
void setup() {
    auto cfg = M5.config();
    cfg.output_power = true; 
    M5.begin(cfg);
    
    // Force le 5V sortant (pour alimenter les LEDs quand IN=0mV)
    M5.Power.setExtOutput(true);

    // Init LEDs Base
    baseLeds.begin();
    baseLeds.show(); 

    M5.Display.setBrightness(120);
    M5.Display.fillScreen(TFT_BLACK);

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, 320 * 20);
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 320; 
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);

    info_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(info_label, lv_color_hex(0x666666), 0);
    lv_obj_align(info_label, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_label_set_text(info_label, "Connexion WiFi...");
    lv_timer_handler();

    WiFi.begin(ssid, password);
    unsigned long start_wifi = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start_wifi < 8000)) { 
        delay(250); 
        lv_timer_handler(); 
    }

    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.google.com");

    card_h = create_clock_unit(lv_scr_act(), 15);
    card_m = create_clock_unit(lv_scr_act(), 165);
    create_battery_widget();

    struct tm ti;
    if(getLocalTime(&ti, 500)) {
        last_h = ti.tm_hour;
        last_m = ti.tm_min;
        M5.Rtc.setDateTime(&ti);
        lv_label_set_text(info_label, "Synchro NTP OK");
    } else {
        auto dt = M5.Rtc.getDateTime();
        last_h = dt.time.hours;
        last_m = dt.time.minutes;
        lv_label_set_text(info_label, "Mode RTC (Offline)");
    }
    
    lv_label_set_text_fmt(lv_obj_get_child(card_h, 0), "%02d", last_h);
    lv_label_set_text_fmt(lv_obj_get_child(card_m, 0), "%02d", last_m);
    
    update_battery_display();

    // Timer de boucle
    lv_timer_create([](lv_timer_t* t){
        int current_h, current_m;
        struct tm now;
        
        if (getLocalTime(&now, 50)) {
            current_h = now.tm_hour;
            current_m = now.tm_min;
            if (now.tm_sec == 0) M5.Rtc.setDateTime(&now);
        } else {
            auto dt = M5.Rtc.getDateTime();
            current_h = dt.time.hours;
            current_m = dt.time.minutes;
        }

        if (current_m != last_m) {
            if (current_h != last_h) {
                play_flip_anim(card_h, current_h);
                last_h = current_h;
            }
            play_flip_anim(card_m, current_m);
            last_m = current_m; 
        }

        update_battery_display();

    }, 1000, NULL);
}

void loop() {
    M5.update();
    lv_timer_handler();
    delay(5);
}