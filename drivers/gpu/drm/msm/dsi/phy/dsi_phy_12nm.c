// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 * Copyright (c) 2025, Guilherme Castro <guilherme.castro@pm.me>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/iopoll.h>

#include "dsi_phy.h"
#include "dsi.xml.h"
#include "dsi_phy_12nm.xml.h"

/*
 * DSI PLL 12nm - clock diagram (eg: DSI0):
 *
 *                     +----------+
 *            +--------|  vco_clk |--------+
 *            |        +----------+        |
 *            |                            |
 *      +-----v------+              +-----v------+
 *      |/1 /2 /4    |              |/1 /2 /4    |
 *      |   /8 /16/32|              |   /8 /16/32|
 *      +-----+------+              +-----+------+
 *            |                            |
 *     +------v------+             +------v------+
 *     |post_div_mux |             | gp_div_mux  |
 *     +------+------+             +------+------+
 *            |                            |
 *       +----v----+                  +----v----+
 *       |  DIV-4  |                  | DIV+1   |
 *       +----+----+                  | (1..128)|
 *            |                       +----+----+
 *            v                            |
 *      dsi0pllbyte                        v
 *                                     dsi0pll
 */

#define VCO_REF_CLK_RATE	19200000UL
#define VCO_MIN_RATE		1000000000UL
#define VCO_MAX_RATE		2000000000UL

#define PLL_POLL_MAX_READS	15
#define PLL_POLL_TIMEOUT_US	1000

struct pll_12nm_cached_state {
	unsigned long vco_rate;
	u8 post_div_mux;
	u8 gp_div_mux;
	u8 pixel_divhf;
};

struct dsi_pll_12nm {
	struct clk_hw clk_hw;
	struct msm_dsi_phy *phy;
	struct pll_12nm_cached_state cached_state;
};

#define to_pll_12nm(x)	container_of(x, struct dsi_pll_12nm, clk_hw)

/*
 * Post-divider mux encoding for the 12nm PLL.
 *
 * The post_div_mux selects a division ratio (1/2/4/8/16/32) from the VCO.
 * The encoding is split across two registers:
 *   VCO_CTRL[5:4]           + CHAR_PUMP_BIAS_CTRL[6]
 *
 * sel  div   VCO_CTRL[5:4]  CPBIAS[6]
 *  0    1       0x00           0
 *  1    2       0x30           1
 *  2    4       0x10           0
 *  3    8       0x20           0
 *  4   16       0x30           0
 *  5   32       0x00           1
 */
static const u8 post_div_table[] = { 1, 2, 4, 8, 16, 32 };

static void pll_12nm_encode_post_div(u8 sel, u8 *vco_bits, u8 *cpbias_bit)
{
	switch (sel) {
	case 0: *vco_bits = 0x00; *cpbias_bit = 0; break; /* /1  */
	case 1: *vco_bits = 0x30; *cpbias_bit = 1; break; /* /2  */
	case 2: *vco_bits = 0x10; *cpbias_bit = 0; break; /* /4  */
	case 3: *vco_bits = 0x20; *cpbias_bit = 0; break; /* /8  */
	case 4: *vco_bits = 0x30; *cpbias_bit = 0; break; /* /16 */
	case 5: *vco_bits = 0x00; *cpbias_bit = 1; break; /* /32 */
	default: *vco_bits = 0x00; *cpbias_bit = 0; break;
	}
}

static u8 pll_12nm_decode_post_div(void __iomem *pll_base)
{
	u32 vco_cntrl, cpbias_cntrl;

	vco_cntrl = readl(pll_base + REG_DSI_12nm_PHY_PLL_VCO_CTRL) & 0x30;
	cpbias_cntrl = (readl(pll_base + REG_DSI_12nm_PHY_PLL_CHAR_PUMP_BIAS_CTRL) >> 6) & 0x1;

	if (cpbias_cntrl == 0) {
		if (vco_cntrl == 0x00)
			return 0; /* /1  */
		else if (vco_cntrl == 0x10)
			return 2; /* /4  */
		else if (vco_cntrl == 0x20)
			return 3; /* /8  */
		else if (vco_cntrl == 0x30)
			return 4; /* /16 */
	} else if (cpbias_cntrl == 1) {
		if (vco_cntrl == 0x30)
			return 1; /* /2  */
		else if (vco_cntrl == 0x00)
			return 5; /* /32 */
	}

	return 0; /* default to /1 */
}

/*
 * Compute the hsfreqrange value from target frequency (after post_div).
 * target_freq is in Hz, bitclk = target_freq * 2.
 */
static u32 pll_12nm_get_hsfreqrange(u64 target_freq)
{
	u64 bitclk_mhz = div_u64(target_freq * 2, 1000000);

	/*
	 * Table from downstream mdss-dsi-pll-12nm-util.c
	 * Maps bit clock rate (MHz) to hsfreqrange register value.
	 */
	static const struct {
		u32 min_mhz;
		u32 max_mhz;
		u32 val;
	} tbl[] = {
		{   80,   90, 0x00 }, {   90,  100, 0x10 },
		{  100,  110, 0x20 }, {  110,  120, 0x30 },
		{  120,  130, 0x01 }, {  130,  140, 0x11 },
		{  140,  150, 0x21 }, {  150,  160, 0x31 },
		{  160,  170, 0x02 }, {  170,  180, 0x12 },
		{  180,  190, 0x22 }, {  190,  205, 0x32 },
		{  205,  220, 0x03 }, {  220,  235, 0x13 },
		{  235,  250, 0x23 }, {  250,  275, 0x33 },
		{  275,  300, 0x04 }, {  300,  325, 0x14 },
		{  325,  350, 0x25 }, {  350,  400, 0x35 },
		{  400,  450, 0x05 }, {  450,  500, 0x16 },
		{  500,  550, 0x26 }, {  550,  600, 0x37 },
		{  600,  650, 0x07 }, {  650,  700, 0x18 },
		{  700,  750, 0x28 }, {  750,  800, 0x39 },
		{  800,  850, 0x09 }, {  850,  900, 0x19 },
		{  900,  950, 0x29 }, {  950, 1000, 0x3a },
		{ 1000, 1050, 0x0a }, { 1050, 1100, 0x1a },
		{ 1100, 1150, 0x2a }, { 1150, 1200, 0x3b },
		{ 1200, 1250, 0x0b }, { 1250, 1300, 0x1b },
		{ 1300, 1350, 0x2b }, { 1350, 1400, 0x3c },
		{ 1400, 1450, 0x0c }, { 1450, 1500, 0x1c },
		{ 1500, 1550, 0x2c }, { 1550, 1600, 0x3d },
		{ 1600, 1650, 0x0d }, { 1650, 1700, 0x1d },
		{ 1700, 1750, 0x2e }, { 1750, 1800, 0x3e },
		{ 1800, 1850, 0x0e }, { 1850, 1900, 0x1e },
		{ 1900, 1950, 0x2f }, { 1950, 2000, 0x3f },
		{ 2000, 2050, 0x0f }, { 2050, 2100, 0x40 },
		{ 2100, 2150, 0x41 }, { 2150, 2200, 0x42 },
		{ 2200, 2250, 0x43 }, { 2250, 2300, 0x44 },
		{ 2300, 2350, 0x45 }, { 2350, 2400, 0x46 },
		{ 2400, 2450, 0x47 }, { 2450, 2500, 0x48 },
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(tbl); i++)
		if (bitclk_mhz >= tbl[i].min_mhz && bitclk_mhz < tbl[i].max_mhz)
			return tbl[i].val;

	return 0x49; /* >= 2500 MHz */
}

/*
 * Compute VCO control and charge pump bias control from target frequency
 * and post_div_mux selection.
 */
static void pll_12nm_get_vco_cntrl(u64 target_freq, u8 post_div_mux,
				    u32 *vco_cntrl, u32 *cpbias_cntrl)
{
	u64 target_freq_mhz = div_u64(target_freq, 1000000);
	u8 vco_bits, cpbias_bit;

	/* Start with the post_div encoding */
	pll_12nm_encode_post_div(post_div_mux, &vco_bits, &cpbias_bit);

	*cpbias_cntrl = cpbias_bit;
	*vco_cntrl = vco_bits;

	/* Add VCO band selection in bits [1:0] */
	if (target_freq_mhz <= 1250 && target_freq_mhz >= 1092)
		*vco_cntrl |= 2;
	else if (target_freq_mhz < 1092 && target_freq_mhz >= 950)
		*vco_cntrl |= 3;
	else if (target_freq_mhz < 950 && target_freq_mhz >= 712)
		*vco_cntrl |= 1;
	else if (target_freq_mhz < 712 && target_freq_mhz >= 546)
		*vco_cntrl |= 2;
	else if (target_freq_mhz < 546 && target_freq_mhz >= 475)
		*vco_cntrl |= 3;
	else if (target_freq_mhz < 475 && target_freq_mhz >= 356)
		*vco_cntrl |= 1;
	else if (target_freq_mhz < 356 && target_freq_mhz >= 273)
		*vco_cntrl |= 2;
	else if (target_freq_mhz < 273 && target_freq_mhz >= 237)
		*vco_cntrl |= 3;
	else if (target_freq_mhz < 237 && target_freq_mhz >= 178)
		*vco_cntrl |= 1;
	else if (target_freq_mhz < 178 && target_freq_mhz >= 136)
		*vco_cntrl |= 2;
	else if (target_freq_mhz < 136 && target_freq_mhz >= 118)
		*vco_cntrl |= 3;
	else if (target_freq_mhz < 118 && target_freq_mhz >= 89)
		*vco_cntrl |= 1;
	else if (target_freq_mhz < 89 && target_freq_mhz >= 68)
		*vco_cntrl |= 2;
	else if (target_freq_mhz < 68 && target_freq_mhz >= 57)
		*vco_cntrl |= 3;
	else if (target_freq_mhz < 57 && target_freq_mhz >= 44)
		*vco_cntrl |= 1;
	else
		*vco_cntrl |= 2;
}

static u32 pll_12nm_get_osc_freq_target(u64 target_freq)
{
	u64 target_freq_mhz = div_u64(target_freq, 1000000);

	if (target_freq_mhz <= 1000)
		return 1315;
	else if (target_freq_mhz <= 1500)
		return 1839;
	else
		return 0;
}

static u32 pll_12nm_get_fsm_ovr_ctrl(u64 target_freq)
{
	u64 bitclk_mhz = div_u64(target_freq * 2, 1000000);

	if (bitclk_mhz > 1500 && bitclk_mhz <= 2500)
		return 0;
	else
		return BIT(6);
}

/*
 * Program PLL registers for a given VCO rate and post_div_mux selection.
 */
static void pll_12nm_commit(struct dsi_pll_12nm *pll_12nm, u8 post_div_mux)
{
	struct msm_dsi_phy *phy = pll_12nm->phy;
	void __iomem *pll_base = phy->pll_base;
	u64 target_freq;
	unsigned long vco_rate = pll_12nm->cached_state.vco_rate;
	u32 m_div, hsfreqrange, vco_cntrl, cpbias_cntrl;
	u32 osc_freq_target, fsm_ovr_ctrl;
	u8 data;

	target_freq = div_u64(vco_rate, post_div_table[post_div_mux]);

	hsfreqrange = pll_12nm_get_hsfreqrange(target_freq);
	pll_12nm_get_vco_cntrl(target_freq, post_div_mux,
			       &vco_cntrl, &cpbias_cntrl);
	osc_freq_target = pll_12nm_get_osc_freq_target(target_freq);
	m_div = (u32)div_u64(vco_rate * 4, VCO_REF_CLK_RATE);
	fsm_ovr_ctrl = pll_12nm_get_fsm_ovr_ctrl(target_freq);

	writel(0x01, pll_base + REG_DSI_12nm_PHY_PLL_CTRL0);
	writel(0x05, pll_base + REG_DSI_12nm_PHY_PLL_PLL_CTRL);
	writel(0x01, pll_base + REG_DSI_12nm_PHY_PLL_SLEWRATE_DDL_LOOP_CTRL);

	data = (hsfreqrange & 0x7f) | BIT(7);
	writel(data, pll_base + REG_DSI_12nm_PHY_PLL_HS_FREQ_RAN_SEL);

	data = (vco_cntrl & 0x3f) | BIT(6);
	writel(data, pll_base + REG_DSI_12nm_PHY_PLL_VCO_CTRL);

	data = osc_freq_target & 0x7f;
	writel(data, pll_base + REG_DSI_12nm_PHY_PLL_SLEWRATE_DDL_CYC_FRQ_ADJ_0);

	data = (osc_freq_target >> 7) & 0x1f;
	writel(data, pll_base + REG_DSI_12nm_PHY_PLL_SLEWRATE_DDL_CYC_FRQ_ADJ_1);
	writel(0x30, pll_base + REG_DSI_12nm_PHY_PLL_INPUT_LOOP_DIV_RAT_CTRL);

	data = m_div & 0x3f;
	writel(data, pll_base + REG_DSI_12nm_PHY_PLL_LOOP_DIV_RATIO_0);

	data = (m_div >> 6) & 0x3f;
	writel(data, pll_base + REG_DSI_12nm_PHY_PLL_LOOP_DIV_RATIO_1);
	writel(0x60, pll_base + REG_DSI_12nm_PHY_PLL_INPUT_DIV_PLL_OVR);

	writel(0x05, pll_base + REG_DSI_12nm_PHY_PLL_PROP_CHRG_PUMP_CTRL);
	writel(0x00, pll_base + REG_DSI_12nm_PHY_PLL_INTEG_CHRG_PUMP_CTRL);
	writel(0x10, pll_base + REG_DSI_12nm_PHY_PLL_GMP_CTRL_DIG_TST);

	data = ((cpbias_cntrl & 0x1) << 6) | BIT(4);
	writel(data, pll_base + REG_DSI_12nm_PHY_PLL_CHAR_PUMP_BIAS_CTRL);

	data = (pll_12nm->cached_state.gp_div_mux << 5) | 0x05;
	writel(data, pll_base + REG_DSI_12nm_PHY_PLL_PLL_CTRL);

	data = pll_12nm->cached_state.pixel_divhf & 0x7f;
	writel(data, pll_base + REG_DSI_12nm_PHY_PLL_SSC9);

	writel(0x03, pll_base + REG_DSI_12nm_PHY_PLL_ANA_PROG_CTRL);
	writel(0x50, pll_base + REG_DSI_12nm_PHY_PLL_ANA_TST_LOCK_ST_OVR_CTRL);
	writel(fsm_ovr_ctrl, pll_base + REG_DSI_12nm_PHY_PLL_SLEWRATE_FSM_OVR_CTRL);
	writel(0x01, pll_base + REG_DSI_12nm_PHY_PLL_PHA_ERR_CTRL_0);
	writel(0x00, pll_base + REG_DSI_12nm_PHY_PLL_PHA_ERR_CTRL_1);
	writel(0xff, pll_base + REG_DSI_12nm_PHY_PLL_LOCK_FILTER);
	writel(0x03, pll_base + REG_DSI_12nm_PHY_PLL_UNLOCK_FILTER);
	writel(0x0c, pll_base + REG_DSI_12nm_PHY_PLL_PRO_DLY_RELOCK);
	writel(0x02, pll_base + REG_DSI_12nm_PHY_PLL_LOCK_DET_MODE_SEL);

	/* Ensure all writes complete before enabling */
	wmb();
}

static bool pll_12nm_poll_for_ready(struct dsi_pll_12nm *pll_12nm)
{
	u32 val;
	int ret;

	ret = readl_poll_timeout_atomic(
		pll_12nm->phy->pll_base + REG_DSI_12nm_PHY_PLL_STAT0,
		val, val & DSI_12nm_PHY_PLL_STAT0_PLL_RDY,
		PLL_POLL_TIMEOUT_US / PLL_POLL_MAX_READS,
		PLL_POLL_TIMEOUT_US);

	return ret == 0;
}

static int pll_12nm_enable_seq(struct dsi_pll_12nm *pll_12nm)
{
	void __iomem *pll_base = pll_12nm->phy->pll_base;
	u32 data;

	/* Power up the PLL before attempting lock */
	data = readl(pll_base + REG_DSI_12nm_PHY_PLL_POWERUP_CTRL);
	data &= ~BIT(1); /* clear ONPLL_OVR_EN */
	data |= BIT(0);  /* set ONPLL_OVR */
	writel(data, pll_base + REG_DSI_12nm_PHY_PLL_POWERUP_CTRL);
	ndelay(500);

	writel(0x49, pll_base + REG_DSI_12nm_PHY_PLL_SYS_CTRL);
	wmb(); /* ensure committed before delay */
	udelay(5);
	writel(0xc9, pll_base + REG_DSI_12nm_PHY_PLL_SYS_CTRL);
	wmb(); /* ensure committed before polling */
	udelay(50);

	if (!pll_12nm_poll_for_ready(pll_12nm)) {
		DRM_DEV_ERROR(&pll_12nm->phy->pdev->dev,
			      "DSI PLL 12nm lock failed\n");
		return -EINVAL;
	}

	/* Post-lock: set CLK_SEL to route PLL output */
	ndelay(50);
	data = readl(pll_base + REG_DSI_12nm_PHY_PLL_PLL_CTRL);
	data |= 0x01;
	writel(data, pll_base + REG_DSI_12nm_PHY_PLL_PLL_CTRL);
	ndelay(500);
	wmb(); /* ensure committed */

	DBG("DSI PLL 12nm locked");
	return 0;
}

/*
 * VCO Clock Callbacks
 */

static int dsi_pll_12nm_clk_set_rate(struct clk_hw *hw, unsigned long rate,
				      unsigned long parent_rate)
{
	struct dsi_pll_12nm *pll_12nm = to_pll_12nm(hw);

	pll_12nm->cached_state.vco_rate = rate;

	return 0;
}

static int dsi_pll_12nm_clk_is_enabled(struct clk_hw *hw)
{
	struct dsi_pll_12nm *pll_12nm = to_pll_12nm(hw);
	u32 status;

	status = readl(pll_12nm->phy->pll_base + REG_DSI_12nm_PHY_PLL_STAT0);

	return !!(status & DSI_12nm_PHY_PLL_STAT0_PLL_RDY);
}

static unsigned long dsi_pll_12nm_clk_recalc_rate(struct clk_hw *hw,
						    unsigned long parent_rate)
{
	struct dsi_pll_12nm *pll_12nm = to_pll_12nm(hw);

	/*
	 * Prefer cached rate (set by clk_set_rate) over HW readback,
	 * because the PLL registers may contain stale values from the
	 * bootloader before we've programmed them.
	 */
	if (pll_12nm->cached_state.vco_rate)
		return pll_12nm->cached_state.vco_rate;

	return VCO_MIN_RATE;
}

static int dsi_pll_12nm_vco_prepare(struct clk_hw *hw)
{
	struct dsi_pll_12nm *pll_12nm = to_pll_12nm(hw);
	void __iomem *pll_base = pll_12nm->phy->pll_base;
	u32 data;
	int ret;

	if (!pll_12nm->cached_state.vco_rate) {
		/*
		 * VCO rate not yet configured — this happens during clock
		 * framework init before the first modeset. Just return
		 * success; the proper set_rate + prepare will be called
		 * when the display pipeline is enabled.
		 */
		return 0;
	}

	DBG("DSI PLL 12nm: vco_prepare rate=%lu post_div=%d gp_div=%d pix_divhf=%d",
	    pll_12nm->cached_state.vco_rate,
	    pll_12nm->cached_state.post_div_mux,
	    pll_12nm->cached_state.gp_div_mux,
	    pll_12nm->cached_state.pixel_divhf);

	/*
	 * Always do full PLL programming. We cannot trust bootloader
	 * state since the VCO rate and divider settings may differ.
	 */
	pll_12nm_commit(pll_12nm, pll_12nm->cached_state.post_div_mux);

	ret = pll_12nm_enable_seq(pll_12nm);
	if (ret)
		return ret;

	/* Enable GP_CLK_EN for pixel clock path */
	data = readl(pll_base + REG_DSI_12nm_PHY_PLL_SSC0);
	data |= BIT(6);
	writel(data, pll_base + REG_DSI_12nm_PHY_PLL_SSC0);
	wmb(); /* ensure committed */

	return 0;
}

static void dsi_pll_12nm_vco_unprepare(struct clk_hw *hw)
{
	struct dsi_pll_12nm *pll_12nm = to_pll_12nm(hw);
	void __iomem *pll_base = pll_12nm->phy->pll_base;
	u32 data;

	/* Disable GP_CLK_EN */
	data = readl(pll_base + REG_DSI_12nm_PHY_PLL_SSC0);
	data &= ~BIT(6);
	writel(data, pll_base + REG_DSI_12nm_PHY_PLL_SSC0);
	ndelay(500);

	/* Clear CLK_SEL bits */
	data = readl(pll_base + REG_DSI_12nm_PHY_PLL_PLL_CTRL);
	data &= ~0x03;
	writel(data, pll_base + REG_DSI_12nm_PHY_PLL_PLL_CTRL);
	ndelay(500);

	/* Power down PLL: clear ONPLL_OVR, set ONPLL_OVR_EN */
	data = readl(pll_base + REG_DSI_12nm_PHY_PLL_POWERUP_CTRL);
	data &= ~BIT(0);
	data |= BIT(1);
	writel(data, pll_base + REG_DSI_12nm_PHY_PLL_POWERUP_CTRL);
	ndelay(500);
	wmb(); /* ensure committed */
}

static long dsi_pll_12nm_clk_round_rate(struct clk_hw *hw, unsigned long rate,
					 unsigned long *parent_rate)
{
	struct dsi_pll_12nm *pll_12nm = to_pll_12nm(hw);

	if (rate < pll_12nm->phy->cfg->min_pll_rate)
		return pll_12nm->phy->cfg->min_pll_rate;
	else if (rate > pll_12nm->phy->cfg->max_pll_rate)
		return pll_12nm->phy->cfg->max_pll_rate;
	else
		return rate;
}

static const struct clk_ops clk_ops_dsi_pll_12nm_vco = {
	.round_rate = dsi_pll_12nm_clk_round_rate,
	.set_rate = dsi_pll_12nm_clk_set_rate,
	.recalc_rate = dsi_pll_12nm_clk_recalc_rate,
	.prepare = dsi_pll_12nm_vco_prepare,
	.unprepare = dsi_pll_12nm_vco_unprepare,
	.is_enabled = dsi_pll_12nm_clk_is_enabled,
};

/*
 * PLL Callbacks (save/restore)
 */

static void dsi_12nm_pll_save_state(struct msm_dsi_phy *phy)
{
	struct dsi_pll_12nm *pll_12nm = to_pll_12nm(phy->vco_hw);
	void __iomem *pll_base = phy->pll_base;

	pll_12nm->cached_state.post_div_mux =
		pll_12nm_decode_post_div(pll_base);
	pll_12nm->cached_state.gp_div_mux =
		(readl(pll_base + REG_DSI_12nm_PHY_PLL_PLL_CTRL) >> 5) & 0x7;
	pll_12nm->cached_state.pixel_divhf =
		readl(pll_base + REG_DSI_12nm_PHY_PLL_SSC9) & 0x7f;
}

static int dsi_12nm_pll_restore_state(struct msm_dsi_phy *phy)
{
	struct dsi_pll_12nm *pll_12nm = to_pll_12nm(phy->vco_hw);
	u32 data;
	int ret;

	pll_12nm_commit(pll_12nm, pll_12nm->cached_state.post_div_mux);

	ret = pll_12nm_enable_seq(pll_12nm);
	if (ret) {
		DRM_DEV_ERROR(&phy->pdev->dev,
			      "12nm PLL restore failed: %d\n", ret);
		return ret;
	}

	/* Re-enable GP_CLK_EN */
	data = readl(phy->pll_base + REG_DSI_12nm_PHY_PLL_SSC0);
	data |= BIT(6);
	writel(data, phy->pll_base + REG_DSI_12nm_PHY_PLL_SSC0);
	wmb(); /* ensure committed */

	return 0;
}

/*
 * Byte clock
 *
 * byte_clk = VCO / post_div_table[sel] / 4
 *
 * The post_div_mux uses a non-standard register encoding split across
 * two registers that cannot be modeled with clk_divider.  Instead we
 * cache the selection index and program HW in pll_12nm_commit().
 */
struct dsi_pll_12nm_byteclk {
	struct clk_hw hw;
	struct dsi_pll_12nm *pll;
};

#define to_byteclk(x) container_of(x, struct dsi_pll_12nm_byteclk, hw)

static unsigned long dsi_12nm_byteclk_recalc_rate(struct clk_hw *hw,
						   unsigned long parent_rate)
{
	struct dsi_pll_12nm_byteclk *bc = to_byteclk(hw);
	u8 sel = bc->pll->cached_state.post_div_mux;

	return parent_rate / post_div_table[sel] / 4;
}

static long dsi_12nm_byteclk_round_rate(struct clk_hw *hw, unsigned long rate,
					 unsigned long *parent_rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(post_div_table); i++) {
		unsigned long vco = rate * post_div_table[i] * 4;

		if (vco >= VCO_MIN_RATE && vco <= VCO_MAX_RATE) {
			*parent_rate = vco;
			return vco / post_div_table[i] / 4;
		}
	}

	return -EINVAL;
}

static int dsi_12nm_byteclk_set_rate(struct clk_hw *hw, unsigned long rate,
				      unsigned long parent_rate)
{
	struct dsi_pll_12nm_byteclk *bc = to_byteclk(hw);
	int i;

	for (i = 0; i < ARRAY_SIZE(post_div_table); i++) {
		unsigned long vco = rate * post_div_table[i] * 4;

		if (vco >= VCO_MIN_RATE && vco <= VCO_MAX_RATE) {
			bc->pll->cached_state.post_div_mux = i;
			return 0;
		}
	}

	return -EINVAL;
}

static const struct clk_ops clk_ops_dsi_12nm_byteclk = {
	.round_rate = dsi_12nm_byteclk_round_rate,
	.set_rate = dsi_12nm_byteclk_set_rate,
	.recalc_rate = dsi_12nm_byteclk_recalc_rate,
};

/*
 * Pixel clock
 *
 * pixel_clk = VCO / (1 << gp_div_mux) / (pixel_divhf + 1)
 *
 * gp_div_mux selects power-of-2 division (/1, /2, /4, /8, /16, /32).
 * pixel_divhf selects an integer divider from 1 to 128.
 * Both use non-standard encodings programmed in pll_12nm_commit().
 */
struct dsi_pll_12nm_pixelclk {
	struct clk_hw hw;
	struct dsi_pll_12nm *pll;
};

#define to_pixelclk(x) container_of(x, struct dsi_pll_12nm_pixelclk, hw)

static unsigned long dsi_12nm_pixelclk_recalc_rate(struct clk_hw *hw,
						    unsigned long parent_rate)
{
	struct dsi_pll_12nm_pixelclk *pc = to_pixelclk(hw);
	unsigned long gp_div = 1 << pc->pll->cached_state.gp_div_mux;
	unsigned long pixel_div = pc->pll->cached_state.pixel_divhf + 1;

	return parent_rate / gp_div / pixel_div;
}

static int dsi_12nm_pixelclk_find_divs(unsigned long rate,
					unsigned long parent_rate,
					u8 *gp_div_mux_out,
					u8 *pixel_divhf_out)
{
	int gp_sel;
	unsigned long best_err = ULONG_MAX;
	u8 best_gp = 0, best_pix = 0;
	bool found = false;

	for (gp_sel = 0; gp_sel <= 5; gp_sel++) {
		unsigned long gp_div = 1 << gp_sel;
		unsigned long after_gp = parent_rate / gp_div;
		unsigned long pixel_div, actual_rate, err;

		if (after_gp < rate)
			continue;

		pixel_div = DIV_ROUND_CLOSEST(after_gp, rate);
		if (pixel_div < 1)
			pixel_div = 1;
		if (pixel_div > 128)
			continue;

		actual_rate = after_gp / pixel_div;
		err = (actual_rate > rate) ? actual_rate - rate : rate - actual_rate;

		if (err < best_err) {
			best_err = err;
			best_gp = gp_sel;
			best_pix = pixel_div - 1;
			found = true;
		}
		if (err == 0)
			break;
	}

	if (!found)
		return -EINVAL;

	*gp_div_mux_out = best_gp;
	*pixel_divhf_out = best_pix;
	return 0;
}

static long dsi_12nm_pixelclk_round_rate(struct clk_hw *hw, unsigned long rate,
					   unsigned long *parent_rate)
{
	u8 gp_div_mux, pixel_divhf;
	int ret;

	ret = dsi_12nm_pixelclk_find_divs(rate, *parent_rate,
					   &gp_div_mux, &pixel_divhf);
	if (ret)
		return ret;

	return *parent_rate / (1 << gp_div_mux) / (pixel_divhf + 1);
}

static int dsi_12nm_pixelclk_set_rate(struct clk_hw *hw, unsigned long rate,
					unsigned long parent_rate)
{
	struct dsi_pll_12nm_pixelclk *pc = to_pixelclk(hw);
	u8 gp_div_mux, pixel_divhf;
	int ret;

	ret = dsi_12nm_pixelclk_find_divs(rate, parent_rate,
					   &gp_div_mux, &pixel_divhf);
	if (ret)
		return ret;

	pc->pll->cached_state.gp_div_mux = gp_div_mux;
	pc->pll->cached_state.pixel_divhf = pixel_divhf;

	return 0;
}

static const struct clk_ops clk_ops_dsi_12nm_pixelclk = {
	.round_rate = dsi_12nm_pixelclk_round_rate,
	.set_rate = dsi_12nm_pixelclk_set_rate,
	.recalc_rate = dsi_12nm_pixelclk_recalc_rate,
};

/*
 * PLL Clock Registration
 */

static int pll_12nm_register(struct dsi_pll_12nm *pll_12nm,
			     struct clk_hw **provided_clocks)
{
	char clk_name[32];
	struct clk_init_data vco_init = {
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "ref", .name = "xo",
		},
		.num_parents = 1,
		.name = clk_name,
		.flags = CLK_IGNORE_UNUSED,
		.ops = &clk_ops_dsi_pll_12nm_vco,
	};
	struct clk_init_data byte_init = {
		.num_parents = 1,
		.name = clk_name,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_ops_dsi_12nm_byteclk,
	};
	struct clk_init_data pixel_init = {
		.num_parents = 1,
		.name = clk_name,
		.ops = &clk_ops_dsi_12nm_pixelclk,
	};
	struct device *dev = &pll_12nm->phy->pdev->dev;
	struct dsi_pll_12nm_byteclk *byteclk;
	struct dsi_pll_12nm_pixelclk *pixelclk;
	int ret;

	DBG("id=%d", pll_12nm->phy->id);

	/* Register VCO clock */
	snprintf(clk_name, sizeof(clk_name), "dsi%dvco_clk", pll_12nm->phy->id);
	pll_12nm->clk_hw.init = &vco_init;
	ret = devm_clk_hw_register(dev, &pll_12nm->clk_hw);
	if (ret)
		return ret;

	/* Byte clock: VCO -> post_div -> /4 */
	byteclk = devm_kzalloc(dev, sizeof(*byteclk), GFP_KERNEL);
	if (!byteclk)
		return -ENOMEM;
	byteclk->pll = pll_12nm;
	snprintf(clk_name, sizeof(clk_name), "dsi%dpllbyte", pll_12nm->phy->id);
	byte_init.parent_hws = (const struct clk_hw *[]) { &pll_12nm->clk_hw };
	byteclk->hw.init = &byte_init;
	ret = devm_clk_hw_register(dev, &byteclk->hw);
	if (ret)
		return ret;
	provided_clocks[DSI_BYTE_PLL_CLK] = &byteclk->hw;

	/* Pixel clock: VCO -> gp_div -> pixel_div */
	pixelclk = devm_kzalloc(dev, sizeof(*pixelclk), GFP_KERNEL);
	if (!pixelclk)
		return -ENOMEM;
	pixelclk->pll = pll_12nm;
	snprintf(clk_name, sizeof(clk_name), "dsi%dpll", pll_12nm->phy->id);
	pixel_init.parent_hws = (const struct clk_hw *[]) { &pll_12nm->clk_hw };
	pixelclk->hw.init = &pixel_init;
	ret = devm_clk_hw_register(dev, &pixelclk->hw);
	if (ret)
		return ret;
	provided_clocks[DSI_PIXEL_PLL_CLK] = &pixelclk->hw;

	return 0;
}

static int dsi_pll_12nm_init(struct msm_dsi_phy *phy)
{
	struct platform_device *pdev = phy->pdev;
	struct dsi_pll_12nm *pll_12nm;
	int ret;

	if (!pdev)
		return -ENODEV;

	pll_12nm = devm_kzalloc(&pdev->dev, sizeof(*pll_12nm), GFP_KERNEL);
	if (!pll_12nm)
		return -ENOMEM;

	pll_12nm->phy = phy;

	ret = pll_12nm_register(pll_12nm, phy->provided_clocks->hws);
	if (ret) {
		DRM_DEV_ERROR(&pdev->dev, "failed to register 12nm PLL: %d\n", ret);
		return ret;
	}

	phy->vco_hw = &pll_12nm->clk_hw;

	return 0;
}

/*
 * PHY Operations
 */

static int dsi_12nm_phy_enable(struct msm_dsi_phy *phy,
			       struct msm_dsi_phy_clk_request *clk_req)
{
	struct msm_dsi_dphy_timing *timing = &phy->timing;
	void __iomem *base = phy->base;
	int ret;

	ret = msm_dsi_dphy_timing_calc(timing, clk_req);
	if (ret) {
		DRM_DEV_ERROR(&phy->pdev->dev,
			      "12nm D-PHY timing calculation failed: %d\n", ret);
		return ret;
	}

	/* CTRL0: enable CFG_CLK_EN */
	writel(BIT(0), base + REG_DSI_12nm_PHY_CTRL0);

	/* Clock lane timings */
	writel(timing->clk_zero | BIT(7),
	       base + REG_DSI_12nm_PHY_HSTX_CLKLANE_HS0STATE_TIM_CTRL);
	writel(timing->clk_trail | BIT(6),
	       base + REG_DSI_12nm_PHY_HSTX_CLKLANE_TRALSTATE_TIM_CTRL);
	writel(timing->clk_prepare | BIT(6),
	       base + REG_DSI_12nm_PHY_HSTX_CLKLANE_CLKPOSTSTATE_TIM_CTRL);
	writel(timing->hs_rqst,
	       base + REG_DSI_12nm_PHY_HSTX_CLKLANE_REQSTATE_TIM_CTRL);
	writel(timing->hs_exit | BIT(6) | BIT(7),
	       base + REG_DSI_12nm_PHY_HSTX_CLKLANE_EXITSTATE_TIM_CTRL);

	/* Data lane timings */
	writel(timing->hs_zero | BIT(7),
	       base + REG_DSI_12nm_PHY_HSTX_DATALANE_HS0STATE_TIM_CTRL);
	writel(timing->hs_trail | BIT(6),
	       base + REG_DSI_12nm_PHY_HSTX_DATALANE_TRAILSTATE_TIM_CTRL);
	writel(timing->hs_rqst,
	       base + REG_DSI_12nm_PHY_HSTX_DATALANE_REQSTATE_TIM_CTRL);
	writel(timing->hs_exit | BIT(6) | BIT(7),
	       base + REG_DSI_12nm_PHY_HSTX_DATALANE_EXITSTATE_TIM_CTRL);

	/* TA timings */
	writel(0x03, base + REG_DSI_12nm_PHY_T_TA_GO_TIM_COUNT);
	writel(0x01, base + REG_DSI_12nm_PHY_T_TA_SURE_TIM_COUNT);
	writel(0x85, base + REG_DSI_12nm_PHY_REQ_DLY);

	/* Lane data reversal control */
	writel(0x00, base + REG_DSI_12nm_PHY_HSTX_READY_DLY_DATA_REV_CTRL_LANE0);
	writel(0x00, base + REG_DSI_12nm_PHY_HSTX_READY_DLY_DATA_REV_CTRL_LANE1);
	writel(0x00, base + REG_DSI_12nm_PHY_HSTX_READY_DLY_DATA_REV_CTRL_LANE2);
	writel(0x00, base + REG_DSI_12nm_PHY_HSTX_READY_DLY_DATA_REV_CTRL_LANE3);
	writel(0x00, base + REG_DSI_12nm_PHY_HSTX_DATAREV_CTRL_CLKLANE);

	/* HSTX drive strength */
	writel(BIT(2) | BIT(3), base + REG_DSI_12nm_PHY_HSTX_DRIV_INDATA_CTRL_CLKLANE);
	writel(BIT(2) | BIT(3), base + REG_DSI_12nm_PHY_HSTX_DRIV_INDATA_CTRL_LANE0);
	writel(BIT(2) | BIT(3), base + REG_DSI_12nm_PHY_HSTX_DRIV_INDATA_CTRL_LANE1);
	writel(BIT(2) | BIT(3), base + REG_DSI_12nm_PHY_HSTX_DRIV_INDATA_CTRL_LANE2);
	writel(BIT(2) | BIT(3), base + REG_DSI_12nm_PHY_HSTX_DRIV_INDATA_CTRL_LANE3);
	wmb(); /* ensure PHY registers are committed */

	return 0;
}

static void dsi_12nm_phy_disable(struct msm_dsi_phy *phy)
{
	/* SYS_CTRL: assert PHY reset */
	writel(BIT(0) | BIT(3), phy->base + REG_DSI_12nm_PHY_SYS_CTRL);
	wmb(); /* ensure committed */
}

/*
 * Configuration
 */

static const struct regulator_bulk_data dsi_phy_12nm_regulators[] = {
	{ .supply = "vddio", .init_load_uA = 100000 },
};

const struct msm_dsi_phy_cfg dsi_phy_12nm_cfgs = {
	.has_phy_regulator = true,
	.regulator_data = dsi_phy_12nm_regulators,
	.num_regulators = ARRAY_SIZE(dsi_phy_12nm_regulators),
	.ops = {
		.enable = dsi_12nm_phy_enable,
		.disable = dsi_12nm_phy_disable,
		.pll_init = dsi_pll_12nm_init,
		.save_pll_state = dsi_12nm_pll_save_state,
		.restore_pll_state = dsi_12nm_pll_restore_state,
	},
	.min_pll_rate = VCO_MIN_RATE,
	.max_pll_rate = VCO_MAX_RATE,
	.io_start = { 0x1a94400, 0x1a96400 },
	.num_dsi_phy = 2,
};
