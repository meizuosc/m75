#ifndef _MT_VCORE_DVFS_
#define _MT_VCORE_DVFS_

#include <linux/kernel.h>

/**************************************
 * Config and Parameter
 **************************************/
#define VCORE_BASE_UV		700000
#define VCORE_STEP_UV		6250

#define VCORE_INVALID		0x80
#define FREQ_INVALID		0


/**************************************
 * Define and Declare
 **************************************/
#define PMIC_VCORE_AO_VOSEL_ON		0x36c	/* VCORE2 */
#define PMIC_VCORE_PDN_VOSEL_ON		0x24e	/* VDVFS11 */

#define ERR_VCORE_DVS		1
#define ERR_DDR_DFS		2
#define ERR_AXI_DFS		3

enum dvfs_kicker {
	KR_SCREEN_ON,		/* 0 */
	KR_SCREEN_OFF,		/* 1 */
	KR_SDIO_AUTOK,		/* 2 */
	KR_SYSFS,		/* 3 */
	NUM_KICKERS
};

/* for SDIO */
extern int vcorefs_sdio_lock_dvfs(bool in_ot);
extern u32 vcorefs_sdio_get_vcore_nml(void);
extern int vcorefs_sdio_set_vcore_nml(u32 vcore_uv);
extern int vcorefs_sdio_unlock_dvfs(bool in_ot);
extern bool vcorefs_is_95m_segment(void);

/* for CLKMGR */
extern void vcorefs_clkmgr_notify_mm_off(void);
extern void vcorefs_clkmgr_notify_mm_on(void);


/**************************************
 * Macro and Inline
 **************************************/
#define vcore_uv_to_pmic(uv)	/* pmic >= uv */	\
	((((uv) - VCORE_BASE_UV) + (VCORE_STEP_UV - 1)) / VCORE_STEP_UV)

#define vcore_pmic_to_uv(pmic)	\
	((pmic) * VCORE_STEP_UV + VCORE_BASE_UV)

#define VCORE_1_P_125_UV	1125000
#define VCORE_1_P_0_UV		1000000

#define VCORE_1_P_125		vcore_uv_to_pmic(VCORE_1_P_125_UV)	/* 0x44 */
#define VCORE_1_P_0		vcore_uv_to_pmic(VCORE_1_P_0_UV)	/* 0x30 */

#endif
