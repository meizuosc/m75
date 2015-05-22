#include <linux/kernel.h>
#include <asm/io.h>
#include <mach/sync_write.h>
#include <mach/mt_typedefs.h>
//#include <mach/mt_emi_bm.h>
//#include <mach/mt_reg_base.h>
#include "mt_emi_bm.h"

#define MASK_MASTER 0xFF
#define MASK_TRANS_TYPE 0xFF

static unsigned char g_cBWL;

void MET_BM_Init(void)
{
    g_cBWL = 0;

    /*
    * make sure BW limiter counts consumed Soft-mode bandwidth of each master
    */
    //if (readl(IOMEM(EMI_ARBA)) & 0x00008000) {
    //    g_cBWL |= 1 << 0;
    //    mt65xx_reg_sync_writel(readl(IOMEM(EMI_ARBA)) & ~0x00008000, EMI_ARBA);
    //}

    //if (readl(IOMEM(EMI_ARBB)) & 0x00008000) {
    //    g_cBWL |= 1 << 1;
    //    mt65xx_reg_sync_writel(readl(IOMEM(EMI_ARBB)) & ~0x00008000, EMI_ARBB);
    //}

    //if (readl(IOMEM(EMI_ARBC)) & 0x00008000) {
    //    g_cBWL |= 1 << 2;
    //    mt65xx_reg_sync_writel(readl(IOMEM(EMI_ARBC)) & ~0x00008000, EMI_ARBC);
    //}

    //if (readl(IOMEM(EMI_ARBD)) & 0x00008000) {
    //    g_cBWL |= 1 << 3;
    //    mt65xx_reg_sync_writel(readl(IOMEM(EMI_ARBD)) & ~0x00008000, EMI_ARBD);
    //}

    //if (readl(IOMEM(EMI_ARBE)) & 0x00008000) {
    //    g_cBWL |= 1 << 4;
    //    mt65xx_reg_sync_writel(readl(IOMEM(EMI_ARBE)) & ~0x00008000, EMI_ARBE);
    //}

}

void MET_BM_DeInit(void)
{
    //if (g_cBWL & (1 << 0)) {
    //    g_cBWL &= ~(1 << 0);
    //    mt65xx_reg_sync_writel(readl(IOMEM(EMI_ARBA)) | 0x00008000, EMI_ARBA);
    //}

    //if (g_cBWL & (1 << 1)) {
    //    g_cBWL &= ~(1 << 1);
    //    mt65xx_reg_sync_writel(readl(IOMEM(EMI_ARBB)) | 0x00008000, EMI_ARBB);
    //}

    //if (g_cBWL & (1 << 2)) {
    //    g_cBWL &= ~(1 << 2);
    //    mt65xx_reg_sync_writel(readl(IOMEM(EMI_ARBC)) | 0x00008000, EMI_ARBC);
    //}

    //if (g_cBWL & (1 << 3)) {
    //    g_cBWL &= ~(1 << 3);
    //    mt65xx_reg_sync_writel(readl(IOMEM(EMI_ARBD)) | 0x00008000, EMI_ARBD);
    //}

    //if (g_cBWL & (1 << 4)) {
    //    g_cBWL &= ~(1 << 4);
    //    mt65xx_reg_sync_writel(readl(IOMEM(EMI_ARBE)) | 0x00008000, EMI_ARBE);
    //}
}

void MET_BM_Enable(const unsigned int enable)
{
    const unsigned int value = readl(IOMEM(EMI_BMEN));

    mt65xx_reg_sync_writel((value & ~(BUS_MON_PAUSE | BUS_MON_EN)) |
    		(enable ? BUS_MON_EN : 0), EMI_BMEN);
}

/*
void BM_Disable(void)
{
    const unsigned int value = readl(EMI_BMEN);

    mt65xx_reg_sync_writel(value & (~BUS_MON_EN), EMI_BMEN);
}
*/

void MET_BM_Pause(void)
{
    const unsigned int value = readl(IOMEM(EMI_BMEN));

    mt65xx_reg_sync_writel(value | BUS_MON_PAUSE, EMI_BMEN);
}

void MET_BM_Continue(void)
{
    const unsigned int value = readl(IOMEM(EMI_BMEN));

    mt65xx_reg_sync_writel(value & (~BUS_MON_PAUSE), EMI_BMEN);
}

unsigned int MET_BM_IsOverrun(void)
{
    /*
    * return 0 if EMI_BCNT(bus cycle counts) or
    * EMI_WACT(total word counts) is overrun,
    * otherwise return an !0 value
    */
    const unsigned int value = readl(IOMEM(EMI_BMEN));

    return (value & BC_OVERRUN);
}

void MET_BM_SetReadWriteType(const unsigned int ReadWriteType)
{
    const unsigned int value = readl(IOMEM(EMI_BMEN));

    /*
    * ReadWriteType: 00/11 --> both R/W
    *                   01 --> only R
    *                   10 --> only W
    */
    mt65xx_reg_sync_writel((value & 0xFFFFFFCF) | (ReadWriteType << 4), EMI_BMEN);
}

int MET_BM_GetBusCycCount(void)
{
    return MET_BM_IsOverrun() ? BM_ERR_OVERRUN : readl(IOMEM(EMI_BCNT));
}

unsigned int MET_BM_GetTransAllCount(void)
{
    return readl(IOMEM(EMI_TACT));
}

int MET_BM_GetTransCount(const unsigned int counter_num)
{
    unsigned int iCount;

    switch (counter_num) {
    case 1:
        iCount = readl(IOMEM(EMI_TSCT));
        break;

    case 2:
        iCount = readl(IOMEM(EMI_TSCT2));
        break;

    case 3:
        iCount = readl(IOMEM(EMI_TSCT3));
        break;

    default:
        return BM_ERR_WRONG_REQ;
    }

    return iCount;
}

int MET_BM_GetWordAllCount(void)
{
    return MET_BM_IsOverrun() ? BM_ERR_OVERRUN : readl(IOMEM(EMI_WACT));
}

int MET_BM_GetWordCount(const unsigned int counter_num)
{
    unsigned int iCount;

    switch (counter_num) {
    case 1:
        iCount = readl(IOMEM(EMI_WSCT));
        break;

    case 2:
        iCount = readl(IOMEM(EMI_WSCT2));
        break;

    case 3:
        iCount = readl(IOMEM(EMI_WSCT3));
        break;

    case 4:
        iCount = readl(IOMEM(EMI_WSCT4));
        break;

    default:
        return BM_ERR_WRONG_REQ;
    }

    return iCount;
}

unsigned int MET_BM_GetBandwidthWordCount(void)
{
    return readl(IOMEM(EMI_BACT));
}

unsigned int MET_BM_GetOverheadWordCount(void)
{
    return readl(IOMEM(EMI_BSCT));
}

int MET_BM_GetTransTypeCount(const unsigned int counter_num)
{
    return (counter_num < 1 || counter_num > BM_COUNTER_MAX) ? BM_ERR_WRONG_REQ : readl(IOMEM(EMI_TTYPE1 + (counter_num - 1) * 8));
}

int MET_BM_SetMonitorCounter(const unsigned int counter_num,
			const unsigned int master,
			const unsigned int trans_type)
{
    unsigned int value, addr;
    const unsigned int iMask = (MASK_TRANS_TYPE << 8) | MASK_MASTER;

    if (counter_num < 1 || counter_num > BM_COUNTER_MAX) {
        return BM_ERR_WRONG_REQ;
    }

    if (counter_num == 1) {
        addr = EMI_BMEN;
        value = (readl(IOMEM(addr)) & ~(iMask << 16)) |
		((trans_type & MASK_TRANS_TYPE) << 24) |
		((master & MASK_MASTER) << 16);
    } else {
        addr = (counter_num <= 3) ?
		EMI_MSEL : (EMI_MSEL2 + (counter_num / 2 - 2) * 8);

        // clear master and transaction type fields
        value = readl(IOMEM(addr)) &
			~(iMask << ((counter_num % 2) * 16));

        // set master and transaction type fields
        value |= (((trans_type & MASK_TRANS_TYPE) << 8) |
			(master & MASK_MASTER)) <<
			((counter_num % 2) * 16);
    }

    mt65xx_reg_sync_writel(value, addr);

    return BM_REQ_OK;
}

int MET_BM_SetMaster(const unsigned int counter_num, const unsigned int master)
{
    unsigned int value, addr;
    const unsigned int iMask = 0x7F;

    if (counter_num < 1 || counter_num > BM_COUNTER_MAX) {
        return BM_ERR_WRONG_REQ;
    }

    if (counter_num == 1) {
        addr = EMI_BMEN;
        value = (readl(IOMEM(addr)) & ~(iMask << 16)) | ((master & iMask) << 16);
    }else {
        addr = (counter_num <= 3) ? EMI_MSEL : (EMI_MSEL2 + (counter_num / 2 - 2) * 8);

        // clear master and transaction type fields
        value = readl(IOMEM(addr)) & ~(iMask << ((counter_num % 2) * 16));

        // set master and transaction type fields
        value |= ((master & iMask) << ((counter_num % 2) * 16));
    }

    mt65xx_reg_sync_writel(value, addr);

    return BM_REQ_OK;
}

int MET_BM_SetIDSelect(const unsigned int counter_num, const unsigned int id, const unsigned int enable)
{
    unsigned int value, addr, shift_num;

    if ((counter_num < 1 || counter_num > BM_COUNTER_MAX)
        || (id > 0x1FFF)
        || (enable > 1)) {
        return BM_ERR_WRONG_REQ;
    }

    addr = EMI_BMID0 + (counter_num - 1) / 2 * 4;

    // field's offset in the target EMI_BMIDx register
    shift_num = ((counter_num - 1) % 2) * 16;

    // clear SELx_ID field
    value = readl(IOMEM(addr)) & ~(0x1FFF << shift_num);

    // set SELx_ID field
    value |= id << shift_num;

    mt65xx_reg_sync_writel(value, addr);

    value = (readl(IOMEM(EMI_BMEN2)) & ~(1 << (counter_num - 1))) | (enable << (counter_num - 1));

    mt65xx_reg_sync_writel(value, EMI_BMEN2);

    return BM_REQ_OK;
}

int MET_BM_SetUltraHighFilter(const unsigned int counter_num, const unsigned int enable)
{
    unsigned int value;

    if ((counter_num < 1 || counter_num > BM_COUNTER_MAX)
        || (enable > 1)) {
        return BM_ERR_WRONG_REQ;
    }

    value = (readl(IOMEM(EMI_BMEN1)) & ~(1 << (counter_num - 1))) | (enable << (counter_num - 1));

    mt65xx_reg_sync_writel(value, EMI_BMEN1);

    return BM_REQ_OK;
}

int MET_BM_SetLatencyCounter(void)
{
	unsigned int value;

	value = readl(IOMEM(EMI_BMEN2)) & ~(0b11 << 24);
	//emi_ttype1 -- emi_ttype8 change as total latencies
	//for m0 -- m7,
	//and emi_ttype9 -- emi_ttype16 change as total transaction counts
	//for m0 -- m7
	value |= (0b10 << 24);
	mt65xx_reg_sync_writel(value, EMI_BMEN2);

	return BM_REQ_OK;
}

int MET_BM_GetLatencyCycle(const unsigned int counter_num)
{
	unsigned int cycle_count;

	switch(counter_num)
	{
	case 1:
		cycle_count = readl(IOMEM(EMI_TTYPE1));
		break;
	case 2:
		cycle_count = readl(IOMEM(EMI_TTYPE2));
		break;
	case 3:
		cycle_count = readl(IOMEM(EMI_TTYPE3));
		break;
	case 4:
		cycle_count = readl(IOMEM(EMI_TTYPE4));
		break;
	case 5:
		cycle_count = readl(IOMEM(EMI_TTYPE5));
		break;
	case 6:
		cycle_count = readl(IOMEM(EMI_TTYPE6));
		break;
	case 7:
		cycle_count = readl(IOMEM(EMI_TTYPE7));
		break;
	case 8:
		cycle_count = readl(IOMEM(EMI_TTYPE8));
		break;
	case 9:
		cycle_count = readl(IOMEM(EMI_TTYPE9));
		break;
	case 10:
		cycle_count = readl(IOMEM(EMI_TTYPE10));
		break;
	case 11:
		cycle_count = readl(IOMEM(EMI_TTYPE11));
		break;
	case 12:
		cycle_count = readl(IOMEM(EMI_TTYPE12));
		break;
	case 13:
		cycle_count = readl(IOMEM(EMI_TTYPE13));
		break;
	case 14:
		cycle_count = readl(IOMEM(EMI_TTYPE14));
		break;
	case 15:
		cycle_count = readl(IOMEM(EMI_TTYPE15));
		break;
	case 16:
		cycle_count = readl(IOMEM(EMI_TTYPE16));
		break;
	case 17:
		cycle_count = readl(IOMEM(EMI_TTYPE17));
		break;
	case 18:
		cycle_count = readl(IOMEM(EMI_TTYPE18));
		break;
	case 19:
		cycle_count = readl(IOMEM(EMI_TTYPE19));
		break;
	case 20:
		cycle_count = readl(IOMEM(EMI_TTYPE20));
		break;
	case 21:
		cycle_count = readl(IOMEM(EMI_TTYPE21));
		break;
	default:
		return BM_ERR_WRONG_REQ;
	}

	return cycle_count;
}

int MET_BM_GetEmiDcm(void)
{
    return ((readl(IOMEM(EMI_CONM)) >> 24) ? 1 : 0);
}

int MET_BM_SetEmiDcm(const unsigned int setting)
{
    unsigned int value;

    value = readl(IOMEM(EMI_CONM));
    mt65xx_reg_sync_writel( (value & 0x00FFFFFF) | (setting << 24), EMI_CONM);

    return BM_REQ_OK;
}

// DRAMC Channel A
unsigned int MET_DRAMC_GetPageHitCount(DRAMC_Cnt_Type CountType)
{
    unsigned int iCount;

    switch (CountType) {
    case DRAMC_R2R:
        iCount = readl(IOMEM(DRAMC_R2R_PAGE_HIT));
        break;

    case DRAMC_R2W:
        iCount = readl(IOMEM(DRAMC_R2W_PAGE_HIT));
        break;

    case DRAMC_W2R:
        iCount = readl(IOMEM(DRAMC_W2R_PAGE_HIT));
        break;

    case DRAMC_W2W:
        iCount = readl(IOMEM(DRAMC_W2W_PAGE_HIT));
        break;
    case DRAMC_ALL:
        iCount = readl(IOMEM(DRAMC_R2R_PAGE_HIT)) +
			readl(IOMEM(DRAMC_R2W_PAGE_HIT)) +
                 	readl(IOMEM(DRAMC_W2R_PAGE_HIT)) +
			readl(IOMEM(DRAMC_W2W_PAGE_HIT));
        break;
    default:
        return BM_ERR_WRONG_REQ;
    }

    return iCount;
}

unsigned int MET_DRAMC_GetPageMissCount(DRAMC_Cnt_Type CountType)
{
    unsigned int iCount;

    switch (CountType) {
    case DRAMC_R2R:
        iCount = readl(IOMEM(DRAMC_R2R_PAGE_MISS));
        break;

    case DRAMC_R2W:
        iCount = readl(IOMEM(DRAMC_R2W_PAGE_MISS));
        break;

    case DRAMC_W2R:
        iCount = readl(IOMEM(DRAMC_W2R_PAGE_MISS));
        break;

    case DRAMC_W2W:
        iCount = readl(IOMEM(DRAMC_W2W_PAGE_MISS));
        break;
    case DRAMC_ALL:
        iCount = readl(IOMEM(DRAMC_R2R_PAGE_MISS)) +
			readl(IOMEM(DRAMC_R2W_PAGE_MISS)) +
			readl(IOMEM(DRAMC_W2R_PAGE_MISS)) +
			readl(IOMEM(DRAMC_W2W_PAGE_MISS));
        break;
    default:
        return BM_ERR_WRONG_REQ;
    }

    return iCount;
}

unsigned int MET_DRAMC_GetInterbankCount(DRAMC_Cnt_Type CountType)
{
    unsigned int iCount;

    switch (CountType) {
    case DRAMC_R2R:
        iCount = readl(IOMEM(DRAMC_R2R_INTERBANK));
        break;
    case DRAMC_R2W:
        iCount = readl(IOMEM(DRAMC_R2W_INTERBANK));
        break;
    case DRAMC_W2R:
        iCount = readl(IOMEM(DRAMC_W2R_INTERBANK));
        break;
    case DRAMC_W2W:
        iCount = readl(IOMEM(DRAMC_W2W_INTERBANK));
        break;
    case DRAMC_ALL:
        iCount = readl(IOMEM(DRAMC_R2R_INTERBANK)) +
			readl(IOMEM(DRAMC_R2W_INTERBANK)) +
			readl(IOMEM(DRAMC_W2R_INTERBANK)) +
			readl(IOMEM(DRAMC_W2W_INTERBANK));
        break;
    default:
        return BM_ERR_WRONG_REQ;
    }

    return iCount;
}

unsigned int MET_DRAMC_GetIdleCount(void)
{
    return readl(IOMEM(DRAMC_IDLE_COUNT));
}

// DRAMC Channel B
unsigned int MET_DRAMC_GetPageHitCount1(DRAMC_Cnt_Type CountType)
{
    unsigned int iCount;

    switch (CountType) {
    case DRAMC_R2R:
        iCount = readl(IOMEM(DRAMC_R2R_PAGE_HIT1));
        break;
    case DRAMC_R2W:
        iCount = readl(IOMEM(DRAMC_R2W_PAGE_HIT1));
        break;
    case DRAMC_W2R:
        iCount = readl(IOMEM(DRAMC_W2R_PAGE_HIT1));
        break;
    case DRAMC_W2W:
        iCount = readl(IOMEM(DRAMC_W2W_PAGE_HIT1));
        break;
    case DRAMC_ALL:
        iCount = readl(IOMEM(DRAMC_R2R_PAGE_HIT1)) +
			readl(IOMEM(DRAMC_R2W_PAGE_HIT1)) +
                 	readl(IOMEM(DRAMC_W2R_PAGE_HIT1)) +
			readl(IOMEM(DRAMC_W2W_PAGE_HIT1));
        break;
    default:
        return BM_ERR_WRONG_REQ;
    }

    return iCount;
}

unsigned int MET_DRAMC_GetPageMissCount1(DRAMC_Cnt_Type CountType)
{
    unsigned int iCount;

    switch (CountType) {
    case DRAMC_R2R:
        iCount = readl(IOMEM(DRAMC_R2R_PAGE_MISS1));
        break;

    case DRAMC_R2W:
        iCount = readl(IOMEM(DRAMC_R2W_PAGE_MISS1));
        break;

    case DRAMC_W2R:
        iCount = readl(IOMEM(DRAMC_W2R_PAGE_MISS1));
        break;

    case DRAMC_W2W:
        iCount = readl(IOMEM(DRAMC_W2W_PAGE_MISS1));
        break;
    case DRAMC_ALL:
        iCount = readl(IOMEM(DRAMC_R2R_PAGE_MISS1)) +
			readl(IOMEM(DRAMC_R2W_PAGE_MISS1)) +
			readl(IOMEM(DRAMC_W2R_PAGE_MISS1)) +
			readl(IOMEM(DRAMC_W2W_PAGE_MISS1));
        break;
    default:
        return BM_ERR_WRONG_REQ;
    }

    return iCount;
}

unsigned int MET_DRAMC_GetInterbankCount1(DRAMC_Cnt_Type CountType)
{
    unsigned int iCount;

    switch (CountType) {
    case DRAMC_R2R:
        iCount = readl(IOMEM(DRAMC_R2R_INTERBANK1));
        break;

    case DRAMC_R2W:
        iCount = readl(IOMEM(DRAMC_R2W_INTERBANK1));
        break;

    case DRAMC_W2R:
        iCount = readl(IOMEM(DRAMC_W2R_INTERBANK1));
        break;

    case DRAMC_W2W:
        iCount = readl(IOMEM(DRAMC_W2W_INTERBANK1));
        break;
    case DRAMC_ALL:
        iCount = readl(IOMEM(DRAMC_R2R_INTERBANK1)) +
			readl(IOMEM(DRAMC_R2W_INTERBANK1)) +
			readl(IOMEM(DRAMC_W2R_INTERBANK1)) +
			readl(IOMEM(DRAMC_W2W_INTERBANK1));
        break;
    default:
        return BM_ERR_WRONG_REQ;
    }

    return iCount;
}

void MET_APDMA_Enable(const unsigned int enable)
{
    const unsigned int value = readl(IOMEM(MET_APDMA_EN));

    mt65xx_reg_sync_writel((value & ~(0x1)) |
    		(enable ? 0x1 : 0), MET_APDMA_EN);
}

unsigned int MET_APDMA_GetEna(void)
{
    return readl(IOMEM(MET_APDMA_EN));
}

void MET_APDMA_SetSrcAddr(const unsigned int src)
{
    mt65xx_reg_sync_writel(src, MET_APDMA_SRC);
}

void MET_APDMA_SetDstAddr(const unsigned int dst)
{
    mt65xx_reg_sync_writel(dst, MET_APDMA_DST);
}

void MET_APDMA_SetLen(const unsigned int len)
{
    mt65xx_reg_sync_writel(len, MET_APDMA_LEN);
}

unsigned int MET_DRAMC_GetIdleCount1(void)
{
    return readl(IOMEM(DRAMC_IDLE_COUNT1));
}

unsigned int MET_EMI_GetConA(void)
{
	return readl(IOMEM(EMI_CONA));
}

unsigned int MET_EMI_GetBMEN(void)
{
	return readl(IOMEM(EMI_BMEN));
}

unsigned int MET_EMI_GetBMEN2(void)
{
	return readl(IOMEM(EMI_BMEN2));
}

unsigned int MET_EMI_GetMSEL(void)
{
	return readl(IOMEM(EMI_MSEL));
}

unsigned int MET_EMI_GetMSEL2(void)
{
	return readl(IOMEM(EMI_MSEL2));
}

unsigned int MET_EMI_GetMSEL3(void)
{
	return readl(IOMEM(EMI_MSEL3));
}

unsigned int MET_EMI_GetMSEL4(void)
{
	return readl(IOMEM(EMI_MSEL4));
}

unsigned int MET_EMI_GetMSEL5(void)
{
	return readl(IOMEM(EMI_MSEL5));
}

unsigned int MET_EMI_GetMSEL6(void)
{
	return readl(IOMEM(EMI_MSEL6));
}

unsigned int MET_EMI_GetMSEL7(void)
{
	return readl(IOMEM(EMI_MSEL7));
}

unsigned int MET_EMI_GetMSEL8(void)
{
	return readl(IOMEM(EMI_MSEL8));
}

unsigned int MET_EMI_GetMSEL9(void)
{
	return readl(IOMEM(EMI_MSEL9));
}

unsigned int MET_EMI_GetMSEL10(void)
{
	return readl(IOMEM(EMI_MSEL10));
}

unsigned int MET_EMI_GetBMID0(void)
{
	return readl(IOMEM(EMI_BMID0));
}

unsigned int MET_EMI_GetBMID1(void)
{
	return readl(IOMEM(EMI_BMID1));
}

unsigned int MET_EMI_GetBMID2(void)
{
	return readl(IOMEM(EMI_BMID2));
}

unsigned int MET_EMI_GetBMID3(void)
{
	return readl(IOMEM(EMI_BMID3));
}

unsigned int MET_EMI_GetBMID4(void)
{
	return readl(IOMEM(EMI_BMID4));
}

unsigned int MET_EMI_GetBMID5(void)
{
	return readl(IOMEM(EMI_BMID5));
}

unsigned int MET_EMI_GetBMID6(void)
{
	return readl(IOMEM(EMI_BMID6));
}

unsigned int MET_EMI_GetBMID7(void)
{
	return readl(IOMEM(EMI_BMID7));
}

unsigned int MET_EMI_GetBMID8(void)
{
	return readl(IOMEM(EMI_BMID8));
}

unsigned int MET_EMI_GetBMID9(void)
{
	return readl(IOMEM(EMI_BMID9));
}

unsigned int MET_EMI_GetBMID10(void)
{
	return readl(IOMEM(EMI_BMID10));
}
