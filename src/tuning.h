#ifndef GBTD_TUNING_H
#define GBTD_TUNING_H

/* Pure-data tuning constants — host-compilable.
 *
 * MUST NOT include any GBDK header. This header is shared between the
 * on-device build (via gtypes.h) and the host-side test harness
 * (tests/test_math.c) so that constant drift causes a real test failure
 * (or compile error) instead of the tests silently mirroring stale values.
 *
 * See specs/iter2.md and memory/decisions.md for the rationale behind
 * each value. Append-only: add new constants here, do not redefine in .c. */

/* Pool sizes ---------------------------------------------------------- */
#define MAX_ENEMIES     14   /* iter-2: bumped 12 -> 14 (F6) */
#define MAX_PROJECTILES  8
#define MAX_TOWERS      16

/* Economy ------------------------------------------------------------- */
#define START_HP                5
#define START_ENERGY           30
#define KILL_BOUNTY             3   /* legacy alias for BUG_BOUNTY */
#define MAX_ENERGY            255
/* Iter-2 / iter-3 #20: passive income. */
#define PASSIVE_INCOME_PERIOD 180   /* +1 energy every 3 s */

/* Iter-3 #21: enemy hit flash duration (frames). 3 ~= 50 ms. */
#define FLASH_FRAMES     3

/* Enemies ------------------------------------------------------------- */
#define BUG_HP           3
#define BUG_SPEED       0x0080      /* 0.5 px/frame */
#define BUG_BOUNTY       3

#define ROBOT_HP         6
#define ROBOT_SPEED     0x00C0      /* 0.75 px/frame */
#define ROBOT_BOUNTY     5

/* Towers — antivirus (type 0) and firewall (type 1) ------------------ */
#define TOWER_COST              10  /* legacy alias = TOWER_AV_COST */
#define TOWER_RANGE_PX          24
#define TOWER_RANGE_SQ          (TOWER_RANGE_PX * TOWER_RANGE_PX)
#define TOWER_COOLDOWN          60
#define TOWER_DAMAGE             1

#define TOWER_AV_COST           10
#define TOWER_AV_UPG_COST       15
#define TOWER_AV_COOLDOWN       60
#define TOWER_AV_COOLDOWN_L1    40
#define TOWER_AV_DAMAGE          1
#define TOWER_AV_DAMAGE_L1       2
#define TOWER_AV_RANGE_PX       24

#define TOWER_FW_COST           15
#define TOWER_FW_UPG_COST       20
#define TOWER_FW_COOLDOWN      120
#define TOWER_FW_COOLDOWN_L1    90
#define TOWER_FW_DAMAGE          3
#define TOWER_FW_DAMAGE_L1       4
#define TOWER_FW_RANGE_PX       40
#define TOWER_FW_RANGE_SQ       (TOWER_FW_RANGE_PX * TOWER_FW_RANGE_PX)

/* Projectiles --------------------------------------------------------- */
#define PROJ_SPEED      0x0200      /* 2 px/frame */
#define PROJ_HIT_PX      4
#define PROJ_HIT_SQ     (PROJ_HIT_PX * PROJ_HIT_PX)
#define PROJ_DAMAGE      1

/* Waves --------------------------------------------------------------- */
#define MAX_WAVES        10
#define INTER_WAVE_DELAY 180
#define FIRST_GRACE       60

/* Iter-3 #18: Armored enemy ------------------------------------------ */
#define ARMORED_HP      12
#define ARMORED_SPEED   0x0040      /* 0.25 px/frame */
#define ARMORED_BOUNTY   8

/* Iter-3 #18: EMP tower (type 2) ------------------------------------- */
#define TOWER_EMP_COST          18
#define TOWER_EMP_UPG_COST      12
#define TOWER_EMP_COOLDOWN     120
#define TOWER_EMP_COOLDOWN_L1  120
#define TOWER_EMP_STUN          60   /* stun duration (frames), level 0 */
#define TOWER_EMP_STUN_L1       90   /* stun duration (frames), level 1 */
#define TOWER_EMP_RANGE_PX      16

#endif /* GBTD_TUNING_H */
