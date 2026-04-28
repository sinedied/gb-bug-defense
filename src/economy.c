#include "economy.h"
#include "hud.h"
#include "map.h"

static u8 s_hp;
static u8 s_energy;

void economy_init(void) {
    s_hp = START_HP;
    s_energy = START_ENERGY;
    map_set_computer_damaged(false);
    hud_mark_hp_dirty();
    hud_mark_e_dirty();
}

u8 economy_get_hp(void)     { return s_hp; }
u8 economy_get_energy(void) { return s_energy; }

void economy_damage(u8 dmg) {
    if (dmg >= s_hp) s_hp = 0;
    else s_hp -= dmg;
    map_set_computer_damaged(s_hp <= (START_HP / 2));
    hud_mark_hp_dirty();
}

bool economy_try_spend(u8 cost) {
    if (s_energy < cost) return false;
    s_energy -= cost;
    hud_mark_e_dirty();
    return true;
}

void economy_award(u8 amount) {
    u16 sum = (u16)s_energy + amount;
    s_energy = (sum > MAX_ENERGY) ? MAX_ENERGY : (u8)sum;
    hud_mark_e_dirty();
}
