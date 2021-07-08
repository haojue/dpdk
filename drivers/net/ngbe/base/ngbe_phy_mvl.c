/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 */

#include "ngbe_phy_mvl.h"

#define MVL_PHY_RST_WAIT_PERIOD  5

s32 ngbe_read_phy_reg_mvl(struct ngbe_hw *hw,
		u32 reg_addr, u32 device_type, u16 *phy_data)
{
	mdi_reg_t reg;
	mdi_reg_22_t reg22;

	reg.device_type = device_type;
	reg.addr = reg_addr;

	if (hw->phy.media_type == ngbe_media_type_fiber)
		ngbe_write_phy_reg_mdi(hw, MVL_PAGE_SEL, 0, 1);
	else
		ngbe_write_phy_reg_mdi(hw, MVL_PAGE_SEL, 0, 0);

	ngbe_mdi_map_register(&reg, &reg22);

	ngbe_read_phy_reg_mdi(hw, reg22.addr, reg22.device_type, phy_data);

	return 0;
}

s32 ngbe_write_phy_reg_mvl(struct ngbe_hw *hw,
		u32 reg_addr, u32 device_type, u16 phy_data)
{
	mdi_reg_t reg;
	mdi_reg_22_t reg22;

	reg.device_type = device_type;
	reg.addr = reg_addr;

	if (hw->phy.media_type == ngbe_media_type_fiber)
		ngbe_write_phy_reg_mdi(hw, MVL_PAGE_SEL, 0, 1);
	else
		ngbe_write_phy_reg_mdi(hw, MVL_PAGE_SEL, 0, 0);

	ngbe_mdi_map_register(&reg, &reg22);

	ngbe_write_phy_reg_mdi(hw, reg22.addr, reg22.device_type, phy_data);

	return 0;
}

s32 ngbe_setup_phy_link_mvl(struct ngbe_hw *hw, u32 speed,
				bool autoneg_wait_to_complete)
{
	u16 value_r4 = 0;
	u16 value_r9 = 0;
	u16 value;

	DEBUGFUNC("ngbe_setup_phy_link_mvl");
	UNREFERENCED_PARAMETER(autoneg_wait_to_complete);

	hw->phy.autoneg_advertised = 0;

	if (hw->phy.type == ngbe_phy_mvl) {
		if (speed & NGBE_LINK_SPEED_1GB_FULL) {
			value_r9 |= MVL_PHY_1000BASET_FULL;
			hw->phy.autoneg_advertised |= NGBE_LINK_SPEED_1GB_FULL;
		}

		if (speed & NGBE_LINK_SPEED_100M_FULL) {
			value_r4 |= MVL_PHY_100BASET_FULL;
			hw->phy.autoneg_advertised |= NGBE_LINK_SPEED_100M_FULL;
		}

		if (speed & NGBE_LINK_SPEED_10M_FULL) {
			value_r4 |= MVL_PHY_10BASET_FULL;
			hw->phy.autoneg_advertised |= NGBE_LINK_SPEED_10M_FULL;
		}

		hw->phy.read_reg(hw, MVL_ANA, 0, &value);
		value &= ~(MVL_PHY_100BASET_FULL |
			   MVL_PHY_100BASET_HALF |
			   MVL_PHY_10BASET_FULL |
			   MVL_PHY_10BASET_HALF);
		value_r4 |= value;
		hw->phy.write_reg(hw, MVL_ANA, 0, value_r4);

		hw->phy.read_reg(hw, MVL_PHY_1000BASET, 0, &value);
		value &= ~(MVL_PHY_1000BASET_FULL |
			   MVL_PHY_1000BASET_HALF);
		value_r9 |= value;
		hw->phy.write_reg(hw, MVL_PHY_1000BASET, 0, value_r9);
	} else {
		hw->phy.autoneg_advertised = 1;

		hw->phy.read_reg(hw, MVL_ANA, 0, &value);
		value &= ~(MVL_PHY_1000BASEX_HALF | MVL_PHY_1000BASEX_FULL);
		value |= MVL_PHY_1000BASEX_FULL;
		hw->phy.write_reg(hw, MVL_ANA, 0, value);
	}

	value = MVL_CTRL_RESTART_AN | MVL_CTRL_ANE;
	ngbe_write_phy_reg_mdi(hw, MVL_CTRL, 0, value);

	hw->phy.read_reg(hw, MVL_INTR, 0, &value);

	return 0;
}

s32 ngbe_reset_phy_mvl(struct ngbe_hw *hw)
{
	u32 i;
	u16 ctrl = 0;
	s32 status = 0;

	DEBUGFUNC("ngbe_reset_phy_mvl");

	if (hw->phy.type != ngbe_phy_mvl && hw->phy.type != ngbe_phy_mvl_sfi)
		return NGBE_ERR_PHY_TYPE;

	/* select page 18 reg 20 */
	status = ngbe_write_phy_reg_mdi(hw, MVL_PAGE_SEL, 0, 18);

	/* mode select to RGMII-to-copper or RGMII-to-sfi*/
	if (hw->phy.type == ngbe_phy_mvl)
		ctrl = MVL_GEN_CTL_MODE_COPPER;
	else
		ctrl = MVL_GEN_CTL_MODE_FIBER;
	status = ngbe_write_phy_reg_mdi(hw, MVL_GEN_CTL, 0, ctrl);
	/* mode reset */
	ctrl |= MVL_GEN_CTL_RESET;
	status = ngbe_write_phy_reg_mdi(hw, MVL_GEN_CTL, 0, ctrl);

	for (i = 0; i < MVL_PHY_RST_WAIT_PERIOD; i++) {
		status = ngbe_read_phy_reg_mdi(hw, MVL_GEN_CTL, 0, &ctrl);
		if (!(ctrl & MVL_GEN_CTL_RESET))
			break;
		msleep(1);
	}

	if (i == MVL_PHY_RST_WAIT_PERIOD) {
		DEBUGOUT("PHY reset polling failed to complete.\n");
		return NGBE_ERR_RESET_FAILED;
	}

	return status;
}

s32 ngbe_check_phy_link_mvl(struct ngbe_hw *hw,
		u32 *speed, bool *link_up)
{
	s32 status = 0;
	u16 phy_link = 0;
	u16 phy_speed = 0;
	u16 phy_data = 0;
	u16 insr = 0;

	DEBUGFUNC("ngbe_check_phy_link_mvl");

	/* Initialize speed and link to default case */
	*link_up = false;
	*speed = NGBE_LINK_SPEED_UNKNOWN;

	hw->phy.read_reg(hw, MVL_INTR, 0, &insr);

	/*
	 * Check current speed and link status of the PHY register.
	 * This is a vendor specific register and may have to
	 * be changed for other copper PHYs.
	 */
	status = hw->phy.read_reg(hw, MVL_PHYSR, 0, &phy_data);
	phy_link = phy_data & MVL_PHYSR_LINK;
	phy_speed = phy_data & MVL_PHYSR_SPEED_MASK;

	if (phy_link == MVL_PHYSR_LINK) {
		*link_up = true;

		if (phy_speed == MVL_PHYSR_SPEED_1000M)
			*speed = NGBE_LINK_SPEED_1GB_FULL;
		else if (phy_speed == MVL_PHYSR_SPEED_100M)
			*speed = NGBE_LINK_SPEED_100M_FULL;
		else if (phy_speed == MVL_PHYSR_SPEED_10M)
			*speed = NGBE_LINK_SPEED_10M_FULL;
	}

	return status;
}
