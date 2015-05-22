#include <linux/module.h>
#include <linux/string.h>
#include <linux/ctype.h>

#include "core/met_drv.h"
#include "core/trace.h"

//#include "mt_reg_base.h"
#include "mt_smi.h"
//#include "sync_write.h"
//#include "mt_typedefs.h"
#include "smi.h"
#include "smi_name.h"
#include "plf_trace.h"

//#define MET_SMI_DEBUG

#define MET_SMI_SUCCESS		0
#define MET_SMI_FAIL		-1
#define MET_SMI_BUF_SIZE	128
#define NPORT_IN_PM		4
#define NARRAY			180
#define NHEADER_IN_LM		1
#define NDATA_IN_LM		(4+5)
#define NHEADER_IN_PM		1
#define NDATA_IN_PM		1
// bit31~bit24:Parallel Mode
#define MET_SMI_BIT_PM 		24
// bit15~bit12:Master
#define MET_SMI_BIT_MASTER 	12
// bit11~bit6:Port
#define MET_SMI_BIT_PORT 	6
// bit5~bit4:RW
#define MET_SMI_BIT_RW 		4
// bit3~bit2:DST
#define MET_SMI_BIT_DST 	2
// bit1~bit0:REQ
#define MET_SMI_BIT_REQ 	0

extern struct metdevice met_smi;
static int enable_master_cnt = 0;

static int count = SMI_LARB_NUMBER + SMI_COMM_NUMBER;
static int portnum = SMI_ALLPORT_COUNT;

static struct kobject *kobj_smi = NULL;
static struct met_smi smi_larb[SMI_LARB_NUMBER];
static struct met_smi smi_comm[SMI_COMM_NUMBER];

static int toggle_idx = 0;
static int toggle_cnt = 1000;
static int toggle_master = 0;
static int toggle_master_min = -1;
static int toggle_master_max = -1;
/* Request type */
static int larb_req_type = SMI_REQ_ALL;
static int comm_req_type = SMI_REQ_ALL;
/* Parallel mode */
static int parallel_mode = 0;
/* Ports in parallel mode */
static int larb_pm_port[SMI_LARB_NUMBER][NPORT_IN_PM];
static int comm_pm_port[SMI_COMM_NUMBER][NPORT_IN_PM];
/* Read/Write type in parallel mode */
static int comm_pm_rw_type[SMI_COMM_NUMBER][NPORT_IN_PM];
/* Error message */
static char err_msg[MET_SMI_BUF_SIZE];

static struct smi_cfg allport[SMI_ALLPORT_COUNT*4];

struct chip_smi {
	unsigned int master;
	struct smi_desc *desc;
	unsigned int count;
};

static struct chip_smi smi_map[] = {
	{ 0, larb0_desc, SMI_LARB0_DESC_COUNT }, //larb0
	{ 1, larb1_desc, SMI_LARB1_DESC_COUNT }, //larb1
	{ 2, larb2_desc, SMI_LARB2_DESC_COUNT }, //larb2
	{ 3, larb3_desc, SMI_LARB3_DESC_COUNT }, //larb3
	{ 4, larb4_desc, SMI_LARB4_DESC_COUNT }, //larb4
	{ 5, common_desc, SMI_COMMON_DESC_COUNT } //common
};

static ssize_t larb_req_type_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", larb_req_type);
}

static ssize_t larb_req_type_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	int type;

	if (sscanf(buf, "%d", &(type)) != 1) {
		return -EINVAL;
	}
	larb_req_type = type;

	return n;
}

static ssize_t comm_req_type_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", comm_req_type);
}

static ssize_t comm_req_type_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	int type;

	if (sscanf(buf, "%d", &(type)) != 1) {
		return -EINVAL;
	}
	comm_req_type = type;

	return n;
}

static ssize_t parallel_mode_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", parallel_mode);
}

static ssize_t parallel_mode_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	int mode;

	if (sscanf(buf, "%d", &(mode)) != 1) {
		return -EINVAL;
	}
	parallel_mode = mode;

	return n;
}

static ssize_t pm_rwtype1_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	int i;
	switch (comm_pm_rw_type[0][0]) {
	case SMI_READ_ONLY:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "Read");
		break;
	case SMI_WRITE_ONLY:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "Write");
		break;
	default:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "Error");
		break;
	}
	return i;
}

static ssize_t pm_rwtype1_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	if ((n == 0) || (buf == NULL))
		return -EINVAL;
	if (!strncasecmp(buf, "Read", strlen("Read")))
		comm_pm_rw_type[0][0] = SMI_READ_ONLY;
	else if (!strncasecmp(buf, "Write", strlen("Write")))
		comm_pm_rw_type[0][0] = SMI_WRITE_ONLY;
	else
		return -EINVAL;
	return n;
}

static ssize_t pm_rwtype2_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	int i;
	switch (comm_pm_rw_type[0][1]) {
	case SMI_READ_ONLY:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "Read");
		break;
	case SMI_WRITE_ONLY:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "Write");
		break;
	default:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "Error");
		break;
	}
	return i;
}

static ssize_t pm_rwtype2_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	if ((n == 0) || (buf == NULL))
		return -EINVAL;
	if (!strncasecmp(buf, "Read", strlen("Read")))
		comm_pm_rw_type[0][1] = SMI_READ_ONLY;
	else if (!strncasecmp(buf, "Write", strlen("Write")))
		comm_pm_rw_type[0][1] = SMI_WRITE_ONLY;
	else
		return -EINVAL;
	return n;
}

static ssize_t pm_rwtype3_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	int i;
	switch (comm_pm_rw_type[0][2]) {
	case SMI_READ_ONLY:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "Read");
		break;
	case SMI_WRITE_ONLY:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "Write");
		break;
	default:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "Error");
		break;
	}
	return i;
}

static ssize_t pm_rwtype3_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	if ((n == 0) || (buf == NULL))
		return -EINVAL;
	if (!strncasecmp(buf, "Read", strlen("Read")))
		comm_pm_rw_type[0][2] = SMI_READ_ONLY;
	else if (!strncasecmp(buf, "Write", strlen("Write")))
		comm_pm_rw_type[0][2] = SMI_WRITE_ONLY;
	else
		return -EINVAL;
	return n;
}

static ssize_t pm_rwtype4_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	int i;
	switch (comm_pm_rw_type[0][3]) {
	case SMI_READ_ONLY:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "Read");
		break;
	case SMI_WRITE_ONLY:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "Write");
		break;
	default:
		i = snprintf(buf, PAGE_SIZE, "%s\n", "Error");
		break;
	}
	return i;
}

static ssize_t pm_rwtype4_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf,
				size_t n)
{
	if ((n == 0) || (buf == NULL))
		return -EINVAL;
	if (!strncasecmp(buf, "Read", strlen("Read")))
		comm_pm_rw_type[0][3] = SMI_READ_ONLY;
	else if (!strncasecmp(buf, "Write", strlen("Write")))
		comm_pm_rw_type[0][3] = SMI_WRITE_ONLY;
	else
		return -EINVAL;
	return n;
}

static ssize_t toggle_cnt_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", toggle_cnt);
}

static ssize_t toggle_cnt_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t n)
{
	if (sscanf(buf, "%d", &(toggle_cnt)) != 1) {
			return -EINVAL;
	}
	return n;
}

static ssize_t toggle_master_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", toggle_master);
}

static ssize_t toggle_master_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t n)
{
	int t;
	if (sscanf(buf, "%d", &t) != 1) {
		return -EINVAL;
	}
	if ( t >= 0 && t < SMI_LARB_NUMBER+SMI_COMM_NUMBER) {
		toggle_master = t;
		return n;
	}
	return -EINVAL;
}

static ssize_t count_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", count);
}

static ssize_t count_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t n)
{
	return -EINVAL;
}

static ssize_t portnum_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", portnum);
}

static ssize_t portnum_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t n)
{
	return -EINVAL;
}

static ssize_t err_msg_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	if (MET_SMI_BUF_SIZE < PAGE_SIZE)
		return snprintf(buf, MET_SMI_BUF_SIZE, "%s", err_msg);
	else
		return snprintf(buf, PAGE_SIZE, "%s", err_msg);
}

static struct kobj_attribute parallel_mode_attr =
					__ATTR(enable_parallel_mode,
					0644,
					parallel_mode_show,
					parallel_mode_store);
static struct kobj_attribute pm_rwtype1_attr =
					__ATTR(pm_rwtype1,
					0644,
					pm_rwtype1_show,
					pm_rwtype1_store);
static struct kobj_attribute pm_rwtype2_attr =
					__ATTR(pm_rwtype2,
					0644,
					pm_rwtype2_show,
					pm_rwtype2_store);
static struct kobj_attribute pm_rwtype3_attr =
					__ATTR(pm_rwtype3,
					0644,
					pm_rwtype3_show,
					pm_rwtype3_store);
static struct kobj_attribute pm_rwtype4_attr =
					__ATTR(pm_rwtype4,
					0644,
					pm_rwtype4_show,
					pm_rwtype4_store);
static struct kobj_attribute larb_req_type_attr =
					__ATTR(larb_req_type,
					0644,
					larb_req_type_show,
					larb_req_type_store);
static struct kobj_attribute comm_req_type_attr =
					__ATTR(comm_req_type,
					0644,
					comm_req_type_show,
					comm_req_type_store);
static struct kobj_attribute toggle_cnt_attr = __ATTR(toggle_cnt,
						0644,
						toggle_cnt_show,
						toggle_cnt_store);
static struct kobj_attribute toggle_master_attr = __ATTR(toggle_master,
						0644,
						toggle_master_show,
						toggle_master_store);
static struct kobj_attribute count_attr = __ATTR(count,
						0644,
						count_show,
						count_store);
static struct kobj_attribute portnum_attr = __ATTR(portnum,
						0644,
						portnum_show,
						portnum_store);
static struct kobj_attribute err_msg_attr = __ATTR(err_msg,
						0644,
						err_msg_show,
						NULL);

static int do_smi(void)
{
	return met_smi.mode;
}

static void toggle_port(int toggle_idx)
{
	int i;
	i = allport[toggle_idx].master;
	if (i < SMI_LARB_NUMBER) {
		smi_larb[i].port = allport[toggle_idx].port;
		smi_larb[i].rwtype = allport[toggle_idx].rwtype;
		smi_larb[i].desttype = allport[toggle_idx].desttype;
		smi_larb[i].bustype = allport[toggle_idx].bustype;
		MET_SMI_Disable(i);
		MET_SMI_Clear(i);
		MET_SMI_LARB_SetCfg(i,
			0,
			larb_req_type,
			smi_larb[i].rwtype,
			smi_larb[i].desttype);
		MET_SMI_LARB_SetPortNo(i,
			0,
			smi_larb[i].port);
		MET_SMI_Enable(i);
	} else {
		i = i - SMI_LARB_NUMBER;
		smi_comm[i].port = allport[toggle_idx].port;
		smi_comm[i].rwtype = allport[toggle_idx].rwtype;
		smi_comm[i].desttype = allport[toggle_idx].desttype;
		smi_comm[i].bustype = allport[toggle_idx].bustype;
		MET_SMI_Comm_Disable(i);
		MET_SMI_Comm_Clear(i);
		MET_SMI_COMM_SetCfg(i,
				0,
				comm_req_type);
		MET_SMI_COMM_SetPortNo(i,
				0,
				smi_comm[i].port);
		MET_SMI_COMM_SetRWType(i,
				0,
				smi_comm[i].rwtype);
		//SMI_SetCommBMCfg(i, smi_comm[i].port, smi_comm[i].desttype, smi_comm[i].rwtype);
		MET_SMI_Comm_Enable(i);
	};
}

static void smi_init_value(void)
{
	int i;
	int m0=0, m1=0, m2=0, m3=0, m4=0, m5=0, m6=0;

	printk("smi_init_value\n");

	for (i=0; i<SMI_LARB_NUMBER; i++) {
		smi_larb[i].mode = 0;
		smi_larb[i].master = i;
		smi_larb[i].port = 0;
		smi_larb[i].desttype = SMI_DEST_EMI;
		smi_larb[i].rwtype = SMI_RW_ALL;
		smi_larb[i].bustype = SMI_BUS_GMC;
	}

	for (i=0; i<SMI_COMM_NUMBER; i++) {
		smi_comm[i].mode = 0;
		smi_comm[i].master = SMI_LARB_NUMBER + i;
		smi_comm[i].port = 4;	//GPU
		smi_comm[i].desttype = SMI_DEST_EMI;	//EMI
		smi_comm[i].rwtype = SMI_READ_ONLY;
		smi_comm[i].bustype = SMI_BUS_NONE;
	}

	for (i=0;i<SMI_ALLPORT_COUNT*4;i++) {
		if(i<SMI_LARB0_DESC_COUNT*4) {
			allport[i].master = 0;
			allport[i].port = (m0/4);
			allport[i].bustype = 0;
			allport[i].desttype = ((m0%2) ? 3 : 1);
			allport[i].rwtype = ((m0&0x2)? 2: 1);
			m0++;
		} else if(i<(SMI_LARB0_DESC_COUNT +
				SMI_LARB1_DESC_COUNT)*4) {
			allport[i].master = 1;
			allport[i].port = (m1/4);
			allport[i].bustype = 0;
			allport[i].desttype = ((m1%2) ? 3 : 1);
			allport[i].rwtype = ((m1&0x2)? 2: 1);
			m1++;
		} else if(i<(SMI_LARB0_DESC_COUNT +
				SMI_LARB1_DESC_COUNT +
				SMI_LARB2_DESC_COUNT)*4) {
			allport[i].master = 2;
			allport[i].port = (m2/4);
			allport[i].bustype = 0;
			allport[i].desttype = ((m2%2) ? 3 : 1);
			allport[i].rwtype = ((m2&0x2)? 2: 1);
			m2++;
		} else if(i<(SMI_LARB0_DESC_COUNT +
				SMI_LARB1_DESC_COUNT +
				SMI_LARB2_DESC_COUNT +
				SMI_LARB3_DESC_COUNT)*4) {
			allport[i].master = 3;
			allport[i].port = (m3/4);
			allport[i].bustype = 0;
			allport[i].desttype = ((m3%2) ? 3 : 1);
			allport[i].rwtype = ((m3&0x2)? 2: 1);
			m3++;
		} else if(i<(SMI_LARB0_DESC_COUNT +
				SMI_LARB1_DESC_COUNT +
				SMI_LARB2_DESC_COUNT +
				SMI_LARB3_DESC_COUNT +
				SMI_LARB4_DESC_COUNT)*4) {
			allport[i].master = 4;
			allport[i].port = (m4/4);
			allport[i].bustype = 0;
			allport[i].desttype = ((m4%2) ? 3 : 1);
			allport[i].rwtype = ((m4&0x2)? 2: 1);
			m4++;
		} else if(i<(SMI_LARB0_DESC_COUNT +
				SMI_LARB1_DESC_COUNT +
				SMI_LARB2_DESC_COUNT +
				SMI_LARB3_DESC_COUNT +
				SMI_LARB4_DESC_COUNT +
				SMI_LARB5_DESC_COUNT)*4) {
			allport[i].master = 5;
			allport[i].port = (m5/4);
			allport[i].bustype = 0;
			allport[i].desttype = ((m5%2) ? 3 : 1);
			allport[i].rwtype = ((m5&0x2)? 2: 1);
			m5++;
		} else if(i<SMI_ALLPORT_COUNT*4) {
			allport[i].master = 6;
			allport[i].port = (m6/4);
			allport[i].bustype = 1;
			allport[i].desttype = ((m6%2) ? 3 : 1);
			allport[i].rwtype = ((m6&0x2)? 2: 1);
			m6++;
		} else {
			printk("Error: SMI Index overbound");
		}
	}
}

static void smi_init(void)
{
	int i, j;

	MET_SMI_Init();
	MET_SMI_PowerOn();

	if (do_smi() == 1) {
		for (i=0; i< SMI_LARB_NUMBER; i++) {
			MET_SMI_Disable(i);
			if (parallel_mode == 0) {
				MET_SMI_LARB_SetCfg(i,
						0,
						larb_req_type,
						smi_larb[i].rwtype,
						smi_larb[i].desttype);
				MET_SMI_LARB_SetPortNo(
					i,
					0,
					smi_larb[i].port);
			} else {
				/* Don't care: req type, rw type, dst type */
				MET_SMI_LARB_SetCfg(i,
						1,
						larb_req_type,
						smi_larb[i].rwtype,
						smi_larb[i].desttype);
				for (j=0; j<NPORT_IN_PM; j++) {
					if (larb_pm_port[i][j] != -1)
						MET_SMI_LARB_SetPortNo(
							i,
							j,
							larb_pm_port[i][j]);
				}
			}
			if (smi_larb[i].mode == 1) {
				enable_master_cnt += 1;
			}
		}
		for (i=0; i< SMI_COMM_NUMBER; i++) {
			MET_SMI_Comm_Disable(i);
			if (parallel_mode == 0) {
				MET_SMI_COMM_SetCfg(
						i,
						0,
						comm_req_type);
				MET_SMI_COMM_SetPortNo(
						i,
						0,
						smi_comm[i].port);
				MET_SMI_COMM_SetRWType(
						i,
						0,
						smi_comm[i].rwtype);
			} else {
				MET_SMI_COMM_SetCfg(
						i,
						1,
						comm_req_type);
				for (j=0; j<NPORT_IN_PM; j++) {
					if (comm_pm_port[i][j] != -1)
						MET_SMI_COMM_SetPortNo(
							i,
							j,
							comm_pm_port[i][j]);
						MET_SMI_COMM_SetRWType(
							i,
							j,
							comm_pm_rw_type[i][j]);
				}
			}
			if (smi_comm[i].mode == 1) {
				enable_master_cnt += 1;
			}
		}
	} else if (do_smi() == 2) {
		toggle_idx = 0;
		toggle_port(toggle_idx);
	} else if (do_smi() == 3) {
		toggle_master_max = toggle_master_min = -1;
		for (i=0; i<SMI_ALLPORT_COUNT*4; i++) {
			if (allport[i].master == toggle_master) {
				if (toggle_master_min == -1) {
					toggle_master_max = i;
					toggle_master_min = i;
				}
				if (i > toggle_master_max) {
					toggle_master_max = i;
				}
				if (i < toggle_master_min) {
					toggle_master_min = i;
				}
			}
		}
		if (toggle_master_min >=0 ) {
			toggle_idx = toggle_master_min;
			toggle_port(toggle_idx);
		}
	} else if (do_smi() == 4) {
	}
}

static void smi_start(void)
{
    int i;

    for (i=0; i<SMI_LARB_NUMBER; i++) {
        MET_SMI_Enable(i);
    }
    for (i=0; i<SMI_COMM_NUMBER; i++) {
        MET_SMI_Comm_Enable(i);
    }

}

static void smi_start_master(int i)
{
    if (i < SMI_LARB_NUMBER) {
        MET_SMI_Enable(i);
    } else {
        MET_SMI_Comm_Enable(i-SMI_LARB_NUMBER);
    }
}

static void smi_stop(void)
{
    int i;

    for (i=0; i<SMI_LARB_NUMBER; i++) {
        MET_SMI_Clear(i);
    }
    for (i=0; i<SMI_COMM_NUMBER; i++) {
        MET_SMI_Comm_Clear(i);
    }
}

static void smi_stop_master(int i)
{
    if (i < SMI_LARB_NUMBER) {
        MET_SMI_Clear(i);
    } else {
        MET_SMI_Comm_Clear(i-SMI_LARB_NUMBER);
    }
}

static int smi_lm_get_cnt(unsigned int *array, unsigned int size)
{
	int i;
	int ret=0;

	if (size <
		(SMI_LARB_NUMBER + SMI_COMM_NUMBER)*
		(NHEADER_IN_LM + NDATA_IN_LM))
		return ret;

	// Format: Header + Data
	for (i=0; i<SMI_LARB_NUMBER; i++) {
		if (smi_larb[i].mode == 1) {
			// Reset
			array[ret] = 0;
			// Set Header
			array[ret++] =
				(parallel_mode << MET_SMI_BIT_PM) |
				(smi_larb[i].master << MET_SMI_BIT_MASTER) |
				(smi_larb[i].port << MET_SMI_BIT_PORT) |
				(smi_larb[i].rwtype << MET_SMI_BIT_RW) |
				(smi_larb[i].desttype << MET_SMI_BIT_DST) |
				(larb_req_type << MET_SMI_BIT_REQ);
			// Data
			// ACT/REQ/BEAT/BYTE
			array[ret++] = MET_SMI_GetActiveCnt(i);
			array[ret++] = MET_SMI_GetRequestCnt(i);
			array[ret++] = MET_SMI_GetBeatCnt(i);
			array[ret++] = MET_SMI_GetByteCnt(i);

			// CP/DP/OSTD/CPMAX/OSTDMAX
			array[ret++] = MET_SMI_GetCPCnt(i);
			array[ret++] = MET_SMI_GetDPCnt(i);
			array[ret++] = MET_SMI_GetOSTDCnt(i);
			array[ret++] = MET_SMI_GetCP_MAX(i);
			array[ret++] = MET_SMI_GetOSTD_MAX(i);
		}
	}
	for (i=0; i<SMI_COMM_NUMBER; i++) {
		if (smi_comm[i].mode == 1) {
			// Reset
			array[ret] = 0;
			// Set Header
			array[ret++] =
				(parallel_mode << MET_SMI_BIT_PM) |
				(smi_comm[i].master << MET_SMI_BIT_MASTER) |
				(smi_comm[i].port << MET_SMI_BIT_PORT) |
				(smi_comm[i].rwtype << MET_SMI_BIT_RW) |
				// don't care
				(0x1 << MET_SMI_BIT_DST) |
				(comm_req_type << MET_SMI_BIT_REQ);
			// Data
			// ACT/REQ/BEAT/BYTE
			array[ret++] = MET_SMI_Comm_GetActiveCnt(i);
			array[ret++] = MET_SMI_Comm_GetRequestCnt(i);
			array[ret++] = MET_SMI_Comm_GetBeatCnt(i);
			array[ret++] = MET_SMI_Comm_GetByteCnt(i);

			// CP/DP/OSTD/CPMAX/OSTDMAX
			array[ret++] = MET_SMI_Comm_GetCPCnt(i);
			array[ret++] = MET_SMI_Comm_GetDPCnt(i);
			array[ret++] = MET_SMI_Comm_GetOSTDCnt(i);
			array[ret++] = MET_SMI_Comm_GetCP_MAX(i);
			array[ret++] = MET_SMI_Comm_GetOSTD_MAX(i);
		}
	}

	return ret;
}

static int smi_pm_get_cnt(unsigned int *array, unsigned int size)
{
	int i,j;
	int ret=0;

	if (size < (SMI_LARB_NUMBER+SMI_COMM_NUMBER)
		   * (NPORT_IN_PM)
		   * (NHEADER_IN_PM + NDATA_IN_PM))
		return ret;

	// Format: Header + Data
	for (i=0; i<SMI_LARB_NUMBER; i++) {
		for (j=0; j<NPORT_IN_PM; j++) {
			if (larb_pm_port[i][j] != -1) {
				// Reset
				array[ret] = 0;
				// Set Header
				array[ret++] =
					(parallel_mode <<
						MET_SMI_BIT_PM) |
					(smi_larb[i].master <<
						MET_SMI_BIT_MASTER) |
					(larb_pm_port[i][j] <<
						MET_SMI_BIT_PORT) |
					// R/W
					(0x0 << MET_SMI_BIT_RW) |
					// Internal/External
					(0x0 << MET_SMI_BIT_DST) |
					// Ultra/Preultra/Normal
					(0x0 << MET_SMI_BIT_REQ);
				// Data
				// BYTE
				switch (j) {
				case 0:
					array[ret++] =
						MET_SMI_GetActiveCnt(i);
					break;
				case 1:
					array[ret++] =
						MET_SMI_GetRequestCnt(i);
					break;
				case 2:
					array[ret++] =
						MET_SMI_GetBeatCnt(i);
					break;
				case 3:
					array[ret++] =
						MET_SMI_GetByteCnt(i);
					break;
				default:
					array[ret++] = 0;
					break;
				} // switch (j)
			} // if (larb_pm_port[i][j] != -1)
		} // for (j=0; j<NPORT_IN_PM; j++)
	} // for (i=0; i<SMI_LARB_NUMBER; i++)
	for (i=0; i<SMI_COMM_NUMBER; i++) {
		for (j=0; j<NPORT_IN_PM; j++) {
			if (comm_pm_port[i][j] != -1) {
				// Reset
				array[ret] = 0;
				// Set Header
				array[ret++] =
					(parallel_mode <<
						MET_SMI_BIT_PM) |
					(smi_comm[i].master <<
						MET_SMI_BIT_MASTER) |
					(comm_pm_port[i][j] <<
						MET_SMI_BIT_PORT) |
					(comm_pm_rw_type[i][j] <<
						MET_SMI_BIT_RW) |
					// don't care
					(0x1 << MET_SMI_BIT_DST) |
					(comm_req_type << MET_SMI_BIT_REQ);
				// Data
				// BYTE
				switch (j) {
				case 0:
					array[ret++] =
						MET_SMI_Comm_GetByteCnt(i);
					break;
				case 1:
					array[ret++] =
						MET_SMI_Comm_GetActiveCnt(i);
					break;
				case 2:
					array[ret++] =
						MET_SMI_Comm_GetRequestCnt(i);
					break;
				case 3:
					array[ret++] =
						MET_SMI_Comm_GetBeatCnt(i);
					break;
				default:
					array[ret++] = 0;
					break;
				}
			}
		}
	}

	return ret;
}

static unsigned int smi_polling(unsigned int *smi_value, unsigned int size)
{
	int i=0;
	int ret=0;

#ifdef MET_SMI_DEBUG
	for (i=0; i<SMI_LARB_NUMBER; i++) {
		printk("===SMI Larb[%d]: "
			"Ena = %x, "
			"Clr = %x, "
			"Port = %x, "
			"Ctrl = %x\n",
			i,
			MET_SMI_GetEna(i),
			MET_SMI_GetClr(i),
			MET_SMI_GetPortNo(i),
			MET_SMI_GetCon(i));
	}
#endif

	for (i=0; i<SMI_LARB_NUMBER; i++) {
		MET_SMI_Disable(i);
	}

#ifdef MET_SMI_DEBUG
	for (i=0; i<SMI_LARB_NUMBER; i++) {
		printk("SMI Larb[%d]: "
			"Ena = %x, "
			"Clr = %x, "
			"Port = %x, "
			"Ctrl = %x\n",
			i,
			MET_SMI_GetEna(i),
			MET_SMI_GetClr(i),
			MET_SMI_GetPortNo(i),
			MET_SMI_GetCon(i));
		printk("SMI Larb[%d]: "
			"mode = %lx, "
			"master = %lx, "
			"port = %lx, "
			"rwtype = %lx, "
			"desttype = %lx, "
			"bustype = %lx\n",
			i,
			smi_larb[i].mode,
			smi_larb[i].master,
			smi_larb[i].port,
			smi_larb[i].rwtype,
			smi_larb[i].desttype,
			smi_larb[i].bustype);
	}
#endif

#ifdef MET_SMI_DEBUG
	for (i=0; i<SMI_COMM_NUMBER; i++) {
		printk("===SMI Comm[%d]: "
			"Ena = %x, "
			"Clr = %x, "
			"Type = %x, "
			"Ctrl = %x\n",
			i,
			MET_SMI_Comm_GetEna(i),
			MET_SMI_Comm_GetClr(i),
			MET_SMI_Comm_GetType(i),
			MET_SMI_Comm_GetCon(i));
	}
#endif

	for (i=0; i<SMI_COMM_NUMBER; i++) {
		MET_SMI_Comm_Disable(i);
	}

#ifdef MET_SMI_DEBUG
	for (i=0; i<SMI_COMM_NUMBER; i++) {
		printk("SMI Comm[%d]: "
			"Ena = %x, "
			"Clr = %x, "
			"Type = %x, "
			"Ctrl = %x\n",
			i,
			MET_SMI_Comm_GetEna(i),
			MET_SMI_Comm_GetClr(i),
			MET_SMI_Comm_GetType(i),
			MET_SMI_Comm_GetCon(i));
		printk("SMI Comm[%d]: "
			"mode = %lx, "
			"master = %lx, "
			"port = %lx, "
			"rwtype = %lx, "
			"desttype = %lx, "
			"bustype = %lx\n",
			i,
			smi_comm[i].mode,
			smi_comm[i].master,
			smi_comm[i].port,
			smi_comm[i].rwtype,
			smi_comm[i].desttype,
			smi_comm[i].bustype);
	}
#endif

	if (parallel_mode == 0) {
		// Legacy mode
		ret += smi_lm_get_cnt(smi_value, size);
	} else {
		// Parallel mode
		ret += smi_pm_get_cnt(smi_value, size);
	}

	smi_stop();
	smi_start();

	return ret;
}

// TODO: need to re-check
static unsigned int smi_dump_polling(unsigned int *smi_value)
{
	int ret=0;
	int master;

	if (allport[toggle_idx].master < SMI_LARB_NUMBER) {
		master = allport[toggle_idx].master;
		MET_SMI_Pause(master);
		// Reset
		smi_value[ret] = 0;
		// Set Header
		smi_value[ret++] =
			(parallel_mode << MET_SMI_BIT_PM) |
			(master << MET_SMI_BIT_MASTER) |
			(allport[toggle_idx].port << MET_SMI_BIT_PORT) |
			(allport[toggle_idx].rwtype << MET_SMI_BIT_RW) |
			(allport[toggle_idx].desttype << MET_SMI_BIT_DST) |
			(larb_req_type << MET_SMI_BIT_REQ);
		// Data
		// ACT/REQ/BEAT/BYTE
		smi_value[ret++] = MET_SMI_GetActiveCnt(master);
		smi_value[ret++] = MET_SMI_GetRequestCnt(master);
		smi_value[ret++] = MET_SMI_GetBeatCnt(master);
		smi_value[ret++] = MET_SMI_GetByteCnt(master);

		// CP/DP/OSTD/CPMAX/OSTDMAX
		smi_value[ret++] = MET_SMI_GetCPCnt(master);
		smi_value[ret++] = MET_SMI_GetDPCnt(master);
		smi_value[ret++] = MET_SMI_GetOSTDCnt(master);
		smi_value[ret++] = MET_SMI_GetCP_MAX(master);
		smi_value[ret++] = MET_SMI_GetOSTD_MAX(master);
	} else {
		master = allport[toggle_idx].master - SMI_LARB_NUMBER;
		MET_SMI_Comm_Disable(master);
		// Reset
		smi_value[ret] = 0;
		// Set Header
		smi_value[ret++] =
			(parallel_mode << MET_SMI_BIT_PM) |
			(master << MET_SMI_BIT_MASTER) |
			(allport[toggle_idx].port << MET_SMI_BIT_PORT) |
			(allport[toggle_idx].rwtype << MET_SMI_BIT_RW) |
			// don't care
			(0x0 << MET_SMI_BIT_DST) |
			(comm_req_type << MET_SMI_BIT_REQ);
		// Data
		// ACT/REQ/BEAT/BYTE
		smi_value[ret++] = MET_SMI_Comm_GetActiveCnt(master);
		smi_value[ret++] = MET_SMI_Comm_GetRequestCnt(master);
		smi_value[ret++] = MET_SMI_Comm_GetBeatCnt(master);
		smi_value[ret++] = MET_SMI_Comm_GetByteCnt(master);

		// CP/DP/OSTD/CPMAX/OSTDMAX
		smi_value[ret++] = MET_SMI_Comm_GetCPCnt(master);
		smi_value[ret++] = MET_SMI_Comm_GetDPCnt(master);
		smi_value[ret++] = MET_SMI_Comm_GetOSTDCnt(master);
		smi_value[ret++] = MET_SMI_Comm_GetCP_MAX(master);
		smi_value[ret++] = MET_SMI_Comm_GetOSTD_MAX(master);
	}
	smi_stop_master(master);
	smi_start_master(master);

	return ret;
}

static void smi_uninit(void)
{
	MET_SMI_PowerOff();
}

static int met_smi_create(struct kobject *parent)
{
	int ret = 0;

	/* Init. */
	int i, j;

	smi_init_value();

	larb_req_type = SMI_REQ_ALL;
	comm_req_type = SMI_REQ_ALL;
	parallel_mode = 0;

	for (i=0; i<SMI_LARB_NUMBER; i++) {
		for (j=0; j<NPORT_IN_PM; j++) {
			larb_pm_port[i][j] = -1;
		}
	}

	for (i=0; i<SMI_COMM_NUMBER; i++) {
		for (j=0; j<NPORT_IN_PM; j++) {
			comm_pm_port[i][j] = -1;
			comm_pm_rw_type[i][j] = SMI_READ_ONLY;
		}
	}

	kobj_smi = parent;

	ret = sysfs_create_file(kobj_smi, &toggle_cnt_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create toggle_cnt in sysfs\n");
		return ret;
	}

	ret = sysfs_create_file(kobj_smi, &toggle_master_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create toggle_master in sysfs\n");
		return ret;
	}

	ret = sysfs_create_file(kobj_smi, &count_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create count in sysfs\n");
		return ret;
	}

	ret = sysfs_create_file(kobj_smi, &portnum_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create portnum in sysfs\n");
		return ret;
	}

	ret = sysfs_create_file(kobj_smi, &err_msg_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create err_msg in sysfs\n");
		return ret;
	}

	ret = sysfs_create_file(kobj_smi, &larb_req_type_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create larb_req_type in sysfs\n");
		return ret;
	}

	ret = sysfs_create_file(kobj_smi, &comm_req_type_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create comm_req_type in sysfs\n");
		return ret;
	}

	ret = sysfs_create_file(kobj_smi, &parallel_mode_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create enable_parallel_mode in sysfs\n");
		return ret;
	}

	ret = sysfs_create_file(kobj_smi, &pm_rwtype1_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create pm_rwytpe1 in sysfs\n");
		return ret;
	}

	ret = sysfs_create_file(kobj_smi, &pm_rwtype2_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create pm_rwytpe2 in sysfs\n");
		return ret;
	}

	ret = sysfs_create_file(kobj_smi, &pm_rwtype3_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create pm_rwytpe3 in sysfs\n");
		return ret;
	}

	ret = sysfs_create_file(kobj_smi, &pm_rwtype4_attr.attr);
	if (ret != 0) {
		pr_err("Failed to create pm_rwytpe4 in sysfs\n");
		return ret;
	}

	return ret;
}

void met_smi_delete(void)
{
	if (kobj_smi != NULL) {
		sysfs_remove_file(kobj_smi, &pm_rwtype4_attr.attr);
		sysfs_remove_file(kobj_smi, &pm_rwtype3_attr.attr);
		sysfs_remove_file(kobj_smi, &pm_rwtype2_attr.attr);
		sysfs_remove_file(kobj_smi, &pm_rwtype1_attr.attr);
		sysfs_remove_file(kobj_smi, &parallel_mode_attr.attr);
		sysfs_remove_file(kobj_smi, &comm_req_type_attr.attr);
		sysfs_remove_file(kobj_smi, &larb_req_type_attr.attr);
		sysfs_remove_file(kobj_smi, &err_msg_attr.attr);
		sysfs_remove_file(kobj_smi, &toggle_cnt_attr.attr);
		sysfs_remove_file(kobj_smi, &toggle_master_attr.attr);
		sysfs_remove_file(kobj_smi, &count_attr.attr);
		sysfs_remove_file(kobj_smi, &portnum_attr.attr);
		kobj_smi = NULL;
	}
}

static void met_smi_start(void)
{
	/* HW setting */
	if (do_smi()) {
		smi_init();
		smi_stop();
		smi_start();
	}
}

static void met_smi_stop(void)
{
	/* HW setting */
	if (do_smi()) {
		smi_stop();
		smi_uninit();
	}
}

static void met_smi_polling(unsigned long long stamp, int cpu)
{
	unsigned char count=0;
	unsigned int smi_value[NARRAY];  //TODO: need re-check
	static int times=0;
	static int toggle_stop=0;

	if (do_smi() == 1) { //single port polling
		count = smi_polling(smi_value, NARRAY);
		//printk("===smi_polling result count=%d\n",count);
		if (count) {
			ms_smi(stamp, count, smi_value);
		}
	} else if (toggle_stop == 0) {
		if (do_smi() == 2) { //all-toggling
			//count = smi_toggle_polling(smi_value);
			count = smi_dump_polling(smi_value);
			if (count) {
				//printk("smi_polling result count=%d\n",count);
				ms_smit(stamp, count, smi_value);
			}
			if (times == toggle_cnt) {//switch port
				toggle_idx = (toggle_idx + 1) % (SMI_ALLPORT_COUNT*4);
					toggle_port(toggle_idx);
				times = 0;
			} else {
				times++;
			}
		} else if (do_smi() == 3) { //per-master toggling
			//count = smi_toggle_polling(smi_value);
			count = smi_dump_polling(smi_value);
			if (count) {
				//printk("smi_polling result count=%d\n",count);
				ms_smit(stamp, count, smi_value);
			}
			if (times == toggle_cnt) {//switch port
				toggle_idx = toggle_idx + 1;
				if (toggle_idx > toggle_master_max) {
					toggle_idx = toggle_master_min;
				}
				toggle_port(toggle_idx);
				times = 0;
			} else {
				times++;
			}
		} else if (do_smi() == 4) { //toggle all and stop
			count = smi_dump_polling(smi_value);
			if (count) {
				ms_smit(stamp, count, smi_value);
			}
			if (times == toggle_cnt) {//switch port
				//toggle_idx = (toggle_idx + 1) % (SMI_ALLPORT_COUNT*4);
				toggle_idx = toggle_idx + 1;
				if (toggle_idx < SMI_ALLPORT_COUNT*4) {
					toggle_port(toggle_idx);
				} else {
					toggle_stop = 1;	//stop smi polling
					return;
				}
				times = 0;
			} else {
				times++;
			}
		}
	}
}

static char help[] =
"  --smi=toggle                          "
"monitor all SMI port banwidth\n"
"  --smi=toggle:master                   "
"monitor one master's SMI port banwidth\n"
"  --smi=master:port:rw:dest:bus         "
"monitor specified SMI banwidth\n"
//TODO
"support parallel mode\n";
static char header_legacy_mode[] =
"# ms_smi: timestamp,metadata,active,request,beat,byte,"
"CP,DP,OSTD,CPMAX,OSTDMAX\n"
"met-info [000] 0.0: met_smi_header: timestamp,metadata,"
"active,request,beat,byte,"
"CP,DP,OSTD,CPMAX,OSTDMAX\n";
static char header_parallel_mode[] =
"# ms_smi: timestamp,metadata,byte\n"
"met-info [000] 0.0: met_smi_header: timestamp,metadata,byte\n";
static char header_toggle_mode[] =
"# ms_smit: timestamp,metadata,active,request,beat,byte,"
"CP,DP,OSTD,CPMAX,OSTDMAX\n"
"met-info [000] 0.0: met_smi_header: timestamp,metadata,"
"active,request,beat,byte,"
"CP,DP,OSTD,CPMAX,OSTDMAX\n";

static int smi_print_help(char *buf, int len)
{
	return snprintf(buf, PAGE_SIZE, help);
}

static int smi_print_header(char *buf, int len)
{
	int i, j;

	int ret;

	if (met_smi.mode == 1)
		if (parallel_mode == 0)
			ret = snprintf(buf, PAGE_SIZE, header_legacy_mode);
		else
			ret = snprintf(buf, PAGE_SIZE, header_parallel_mode);
	else
		ret = snprintf(buf, PAGE_SIZE, header_toggle_mode);

	/* Reset */
	smi_init_value();

	larb_req_type = SMI_REQ_ALL;
	comm_req_type = SMI_REQ_ALL;
	parallel_mode = 0;

	for (i=0; i<SMI_LARB_NUMBER; i++) {
		for (j=0; j<NPORT_IN_PM; j++) {
			larb_pm_port[i][j] = -1;
		}
	}

	for (i=0; i<SMI_COMM_NUMBER; i++) {
		for (j=0; j<NPORT_IN_PM; j++) {
			comm_pm_port[i][j] = -1;
			comm_pm_rw_type[i][j] = SMI_READ_ONLY;
		}
	}

	met_smi.mode = 0;

	return ret;
}

static int get_num(const char *dc, int *pValue)
{
	int i = 0;
	int value = 0;
	int digit;

	while (((*dc) >= '0') && ((*dc) <= '9')) {
		digit = *dc - '0';
		value = 10*value + digit;
		dc++;
		i++;
	}

	if (i == 0)
		return 0;

	*pValue = value;
	return i;
}

static int check_master_vaild(int master)
{
	if ((master < 0) || (master >= (SMI_LARB_NUMBER + SMI_COMM_NUMBER)))
		return MET_SMI_FAIL;
	return MET_SMI_SUCCESS;
}

static int check_port_vaild(int master, int port)
{
	if (port < smi_map[master].count)
		return MET_SMI_SUCCESS;
	else
		return MET_SMI_FAIL;
}

static int assign_port(int master, int port)
{
	int i;

	// Legacy mode
	if (parallel_mode == 0) {
		if ((master >= 0) && (master < SMI_LARB_NUMBER)) {
			smi_larb[master].port = port;
			return MET_SMI_SUCCESS;
		} else if ((master >= SMI_LARB_NUMBER) &&
				(master < (SMI_LARB_NUMBER +
					SMI_COMM_NUMBER))) {
			smi_comm[master-SMI_LARB_NUMBER].port = port;
			return MET_SMI_SUCCESS;
		} else {
			return MET_SMI_FAIL;
		}
	// Parallel mode
	} else {
		if ((master >= 0) && (master < SMI_LARB_NUMBER)) {
			for (i=0; i<NPORT_IN_PM; i++) {
				if (larb_pm_port[master][i] == -1) {
					larb_pm_port[master][i] = port;
#ifdef MET_SMI_DEBUG
					printk("===SMI in PM: "
						"Master[%d], "
						"Port = %d, "
						"i = %d\n",
						master,
						port,
						i);
#endif
					break;
				}
			}
			return MET_SMI_SUCCESS;
		} else if ((master >= SMI_LARB_NUMBER) &&
				(master < (SMI_LARB_NUMBER +
					SMI_COMM_NUMBER))) {
			for (i=0; i<NPORT_IN_PM; i++) {
				if (comm_pm_port[master -
						SMI_LARB_NUMBER][i] == -1) {
					comm_pm_port[master -
						SMI_LARB_NUMBER][i] = port;
					break;
				}
			}
			return MET_SMI_SUCCESS;
		} else {
			return MET_SMI_FAIL;
		}
	}
}

static int get_port(const char *arg, int master, int *port)
{
	int ret;

	ret = get_num(arg, port);
	// get port
	if (ret == 0) {
		snprintf(err_msg,
			MET_SMI_BUF_SIZE,
			"Normal: can't get number [%s]\n",
			arg);
		return MET_SMI_FAIL;
	}
	// check port
	if (check_port_vaild(master, *port) != MET_SMI_SUCCESS) {
		snprintf(err_msg,
			MET_SMI_BUF_SIZE,
			"Normal: check port failed [%s]\n",
			arg);
		return MET_SMI_FAIL;
	}
	// assign port
	if (assign_port(master, *port) != MET_SMI_SUCCESS) {
		snprintf(err_msg,
			MET_SMI_BUF_SIZE,
			"Normal: assign port failed [%s]\n",
			arg);
		return MET_SMI_FAIL;
	}

	return ret;
}

static int check_rwtype_vaild(int master, int port, int rwtype)
{
	if (SMI_RW_ALL == smi_map[master].desc[port].rwtype) {
		if ((SMI_RW_ALL == rwtype) ||
			(SMI_READ_ONLY == rwtype) ||
			(SMI_WRITE_ONLY == rwtype))
			return MET_SMI_SUCCESS;
		else
			return MET_SMI_FAIL;
	} else if (SMI_RW_RESPECTIVE == smi_map[master].desc[port].rwtype) {
		if ((SMI_READ_ONLY == rwtype) ||
			(SMI_WRITE_ONLY == rwtype))
			return MET_SMI_SUCCESS;
		else
			return MET_SMI_FAIL;
	} else {
		return MET_SMI_FAIL;
	}
}

static int assign_rwtype(int master, int rwtype)
{
	if ((master >= 0) && (master < SMI_LARB_NUMBER)) {
		smi_larb[master].rwtype = rwtype;
		return MET_SMI_SUCCESS;
	} else if ((master >= SMI_LARB_NUMBER) &&
			(master < (SMI_LARB_NUMBER + SMI_COMM_NUMBER))) {
		smi_comm[master-SMI_LARB_NUMBER].rwtype = rwtype;
		return MET_SMI_SUCCESS;
	} else {
		return MET_SMI_FAIL;
	}

}

static int check_desttype_valid(int master, int port, int desttype)
{
	if ((SMI_DEST_NONE == smi_map[master].desc[port].desttype) ||
		(desttype == smi_map[master].desc[port].desttype))
		return MET_SMI_SUCCESS;
	else if (SMI_DEST_ALL == smi_map[master].desc[port].desttype) {
		if ((SMI_DEST_ALL == desttype) ||
			(SMI_DEST_EMI == desttype) ||
			(SMI_DEST_INTERNAL == desttype))
			return MET_SMI_SUCCESS;
		else
			return MET_SMI_FAIL;
	} else {
		return MET_SMI_FAIL;
	}
}

static int assign_desttype(int master, int desttype)
{
	if ((master >= 0) && (master < SMI_LARB_NUMBER)) {
		smi_larb[master].desttype = desttype;
		return MET_SMI_SUCCESS;
	} else if ((master >= SMI_LARB_NUMBER) &&
			(master < (SMI_LARB_NUMBER + SMI_COMM_NUMBER))) {
		smi_comm[master-SMI_LARB_NUMBER].desttype = desttype;
		return MET_SMI_SUCCESS;
	} else {
		return MET_SMI_FAIL;
	}
}

static int check_bustype_valid(int master, int port, int bustype)
{
	if ((SMI_BUS_NONE == smi_map[master].desc[port].bustype) ||
		(bustype == smi_map[master].desc[port].bustype))
		return MET_SMI_SUCCESS;
	else
		return MET_SMI_FAIL;
}

static int assign_bustype(int master, int bustype)
{
	if ((master >= 0) && (master < SMI_LARB_NUMBER)) {
		smi_larb[master].bustype = bustype;
		return MET_SMI_SUCCESS;
	} else if ((master >= SMI_LARB_NUMBER) &&
			(master < (SMI_LARB_NUMBER + SMI_COMM_NUMBER))) {
		smi_comm[master-SMI_LARB_NUMBER].bustype = bustype;
		return MET_SMI_SUCCESS;
	} else {
		return MET_SMI_FAIL;
	}
}

static int assign_mode(int master, int mode)
{
	if ((master >= 0) && (master < SMI_LARB_NUMBER)) {
		smi_larb[master].mode = mode;
		return MET_SMI_SUCCESS;
	} else if ((master >= SMI_LARB_NUMBER) &&
			(master < (SMI_LARB_NUMBER + SMI_COMM_NUMBER))) {
		smi_comm[master-SMI_LARB_NUMBER].mode = mode;
		return MET_SMI_SUCCESS;
	} else {
		return MET_SMI_FAIL;
	}
}

/*
 * There are serveal cases as follows:
 *
 * 1. "met-cmd --start --smi=toggle"
 *
 * 2. "met-cmd --start --smi=toggle:master"
 *
 * 3. "met-cmd --start --smi=master:port:rwtype:desttype:bustype"
 *
 * 4. "met-cmd --start --smi=dump"
 *
 * 5. "met-cmd --start --smi=master:port[:port1][:port2][:port3]"
 *
 */
static int smi_process_argument(const char *arg, int len)
{
	int master, port;
	int rwtype, desttype, bustype;
	int ret;
	int idx;

	if (len < 6)
		return -1;

	memset(err_msg, 0, MET_SMI_BUF_SIZE);

	/* --smi=toggle */
	if ((strncmp(arg, "toggle", 6) == 0) && (len == 6)) {
		if (met_smi.mode != 0)
			return -1;
		/* Set mode */
		met_smi.mode = 2;
	/* --smi=toggle:master */
	} else if ((strncmp(arg, "toggle", 6) == 0) &&
		arg[6] == ':' &&
		len > 7) {
		if (met_smi.mode != 0)
			return -1;
		ret = get_num(&(arg[7]), &toggle_master);
		if (ret == 0) {
			snprintf(err_msg,
				MET_SMI_BUF_SIZE,
				"Toggle master: can't get number [%s]\n",
				arg);
			return -1;
		}
		if (check_master_vaild(toggle_master) != MET_SMI_SUCCESS) {
			snprintf(err_msg,
				MET_SMI_BUF_SIZE,
				"Toggle master: check master failed [%s]\n",
				arg);
			return -1;
		}
		/* Set mode */
		met_smi.mode = 3;
	/* --smi=master:port:rwtype:desttype:bustype */
	} else if (len >= 3) {
		if ((met_smi.mode != 0) &&
			(met_smi.mode != 1))
			return -1;
		if (parallel_mode == 0) {
			/* Initial variables */
			master = 0;
			port = 0;
			rwtype = 0;
			desttype = 0;
			bustype = 0;
			/* Get master */
			idx = 0;
			ret = get_num(&(arg[idx]), &master);
			if (ret == 0) {
				snprintf(err_msg,
					MET_SMI_BUF_SIZE,
					"Normal: can't get number [%s]\n",
					arg);
				return -1;
			}
			// Check master
			if (check_master_vaild(master) != MET_SMI_SUCCESS) {
				snprintf(err_msg,
					MET_SMI_BUF_SIZE,
					"Normal: check master failed [%s]\n",
					arg);
				return -1;
			}
			/* Get port */
			idx += ret + 1;
			ret = get_port(&(arg[idx]), master, &port);
			if (ret == MET_SMI_FAIL) {
				return -1;
			}
			/* Get rwtype */
			idx += ret + 1;
			ret = get_num(&(arg[idx]), &rwtype);
			if (ret == 0) {
				snprintf(err_msg,
					MET_SMI_BUF_SIZE,
					"Normal: can't get number [%s]\n",
					arg);
				return -1;
			}
			// check rwtype
			if (check_rwtype_vaild(master, port, rwtype) !=
						MET_SMI_SUCCESS) {
				snprintf(err_msg,
					MET_SMI_BUF_SIZE,
					"Normal: check rwtype failed [%s]\n",
					arg);
				return -1;
			}
			// assign rwtype
			if (assign_rwtype(master, rwtype) != MET_SMI_SUCCESS) {
				snprintf(err_msg,
					MET_SMI_BUF_SIZE,
					"Normal: assign rwtype failed [%s]\n",
					arg);
				return -1;
			}
			/* Get desttype */
			idx += ret + 1;
			ret = get_num(&(arg[idx]), &desttype);
			if (ret == 0) {
				snprintf(err_msg,
					MET_SMI_BUF_SIZE,
					"Normal: can't get number [%s]\n",
					arg);
				return -1;
			}
			// check desttype
			if (check_desttype_valid(master, port, desttype) !=
						MET_SMI_SUCCESS) {
				snprintf(err_msg,
					MET_SMI_BUF_SIZE,
					"Normal: check desttype failed [%s]\n",
					arg);
				return -1;
			}
			// assign desttype
			if (assign_desttype(master, desttype) !=
						MET_SMI_SUCCESS) {
				snprintf(err_msg,
					MET_SMI_BUF_SIZE,
					"Normal: assign desttype failed [%s]\n",
					arg);
				return -1;
			}
			/* Get bustype */
			idx += ret + 1;
			ret = get_num(&(arg[idx]), &bustype);
			if (ret == 0) {
				snprintf(err_msg,
					MET_SMI_BUF_SIZE,
					"Normal: can't get number [%s]\n",
					arg);
				return -1;
			}
			// check bustype
			if (check_bustype_valid(master, port, bustype) !=
						MET_SMI_SUCCESS) {
				snprintf(err_msg,
					MET_SMI_BUF_SIZE,
					"Normal: check bustype failed [%s]\n",
					arg);
				return -1;
			}
			// assign bustype
			if (assign_bustype(master, bustype) !=
				MET_SMI_SUCCESS) {
				snprintf(err_msg,
					MET_SMI_BUF_SIZE,
					"Normal: assign bustype failed [%s]\n",
					arg);
				return -1;
			}
			// assign mode for each master TODO: need to re-check
			if (assign_mode(master, 1) != MET_SMI_SUCCESS) {
				snprintf(err_msg,
					MET_SMI_BUF_SIZE,
					"Normal: assign mode failed [%s]\n",
					arg);
				return -1;
			}
#ifdef MET_SMI_DEBUG
			if (master < SMI_LARB_NUMBER) {
			printk("===Setting Master[%d]: "
				"mode = %lx, "
				"master = %lx, "
				"port = %lx, "
				"rwtype = %lx, "
				"desttype = %lx, "
				"bustype = %lx\n",
				master,
				smi_larb[master].mode,
				smi_larb[master].master,
				smi_larb[master].port,
				smi_larb[master].rwtype,
				smi_larb[master].desttype,
				smi_larb[master].bustype);
			} else {
			printk("===Setting Master[%d]: "
				"mode = %lx, "
				"master = %lx, "
				"port = %lx, "
				"rwtype = %lx, "
				"desttype = %lx, "
				"bustype = %lx\n",
				master,
				smi_comm[master-SMI_LARB_NUMBER].mode,
				smi_comm[master-SMI_LARB_NUMBER].master,
				smi_comm[master-SMI_LARB_NUMBER].port,
				smi_comm[master-SMI_LARB_NUMBER].rwtype,
				smi_comm[master-SMI_LARB_NUMBER].desttype,
				smi_comm[master-SMI_LARB_NUMBER].bustype);
			}
#endif
		/* --smi=master:port[:port1][:port2][:port3] */
		} else {
			/* Initial variables */
			master = 0;
			port = 0;
			/* Get master */
			idx = 0;
			ret = get_num(&(arg[idx]), &master);
			if (ret == 0) {
				snprintf(err_msg,
					MET_SMI_BUF_SIZE,
					"Normal: can't get number [%s]\n",
					arg);
				return -1;
			}
			// Check master
			if (check_master_vaild(master) != MET_SMI_SUCCESS) {
				snprintf(err_msg,
					MET_SMI_BUF_SIZE,
					"Normal: check master failed [%s]\n",
					arg);
				return -1;
			}
			/* Get port */
			do {
				idx += ret + 1;
				ret = get_port(&(arg[idx]), master, &port);
				if (ret == MET_SMI_FAIL) {
					return -1;
				}
			} while (isalnum(arg[idx+ret+1]));
		}
		/* Set mode */
		met_smi.mode = 1;
	/* --smi=dump */
	} else if (strncmp(arg, "dump", 4) == 0) {
		if (met_smi.mode != 0)
			return -1;
		/* Set mode */
		met_smi.mode = 4;
	} else {
		return -1;
	}

	return 0;
}

struct metdevice met_smi = {
	.name = "smi",
	.owner = THIS_MODULE,
	.type = MET_TYPE_BUS,
	.create_subfs = met_smi_create,
	.delete_subfs = met_smi_delete,
	.cpu_related = 0,
	.start = met_smi_start,
	.stop = met_smi_stop,
	.polling_interval = 0,
	.timed_polling = met_smi_polling,
	.print_help = smi_print_help,
	.print_header = smi_print_header,
	.process_argument = smi_process_argument,
};

