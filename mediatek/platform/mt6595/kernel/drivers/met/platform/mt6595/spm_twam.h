#ifndef _SPM_TWAM_H_
#define _SPM_TWAM_H_

struct sig_desc_t {
	unsigned long sig;
	char name[40];
};

struct sig_desc_t twam_sig_list[] = {
	{ 0,	"Infra_aux_idle"},
	{ 1,	"CCIF_IDLE"},
	{ 2,	"DRAMC_IDLE"},
	{ 3,	"DISP_IDLE"},
	{ 4,	"ISP_IDLE"},
	{ 5,	"MFG_IDLE"},
	{ 6,	"VENC_IDLE"},
	{ 7,	"VDEC_IDLE"},
	{ 8,	"EMI_IDLE"},
	{ 9,	"AXI_IDLE"},
	{ 10, "PERI_IDLE"},
	{ 11, "DISP_REQ"},
	{ 12, "MFG_REQ"},
	{ 13, "Standbvwfi[0]"},
	{ 14, "Standbvwfi[1]"},
	{ 15, "Standbvwfi[2]"},
	{ 16, "Standbvwfi[3]"},
	{ 17, "Standbvwfi[4]"},
	{ 18, "Standbvwfi[5]"},
	{ 19, "Standbvwfi[6]"},
	{ 20, "Standbvwfi[7]"},
	{ 21, "MCUSYS_IDLE"},
	{ 22, "CA7_CPUTOP_IDLE"},
	{ 23, "CA15_CPUTOP_IDLE"},
	{ 24, "EMI_CLK_OFF_ACK"},
	{ 25, "MD32_SRCCLKEN"},
	{ 26, "MD32_APSRCREQ"},
	{ 27, "MD1_SRCCLKEN"},
	{ 28, "MD_APSRCREQ_MUX"},
	{ 29, "MD2_SRCCLKEN"},
	{ 30, "SRCCLKENI_0"},
	{ 31, "SRCCLKENI_1"},
};

#endif // _SPM_TWAM_H_
