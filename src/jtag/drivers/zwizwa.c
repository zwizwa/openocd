/***************************************************************************
 *   Copyright (C) 2021 by Tom Schouten, tom@zwizwa.be                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <jtag/interface.h>
#include <jtag/swd.h>

//#include "zwizwa.h"

static const struct command_registration zwizwa_command_handlers[] = {
	COMMAND_REGISTRATION_DONE
};

static const char * const zwizwa_transports[] = { "swd", NULL };

static int zwizwa_init(void)
{
	LOG_INFO("ZWIZWA driver");
	return ERROR_OK;
}

static int zwizwa_quit(void)
{
	return ERROR_OK;
}

/* (1) assert or (0) deassert reset lines */
static int zwizwa_reset(int trst, int srst)
{
	return ERROR_OK;
}

static int zwizwa_speed(int speed)
{
	return ERROR_OK;
}

static int zwizwa_khz(int khz, int *jtag_speed)
{
	*jtag_speed = 0;
	return ERROR_OK;
}

static int zwizwa_speed_div(int speed, int *khz)
{
	*khz = 0;
	return ERROR_OK;
}

static int zwizwa_swd_init(void)
{
	LOG_DEBUG("zwizwa_swd_init");
	return ERROR_OK;
}

int zwizwa_swd_switch_seq(enum swd_special_seq seq)
{
	LOG_DEBUG("zwizwa_swd_switch_seq");

	switch (seq) {
	case LINE_RESET:
		LOG_DEBUG("SWD line reset");
		break;
	case JTAG_TO_SWD:
		LOG_DEBUG("JTAG-to-SWD");
		break;
	case SWD_TO_JTAG:
		LOG_DEBUG("SWD-to-JTAG");
		break;
	default:
		LOG_ERROR("Sequence %d not supported", seq);
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

static void zwizwa_swd_read_reg(uint8_t cmd, uint32_t *value, uint32_t ap_delay_clk)
{
	LOG_DEBUG("zwizwa_swd_read_reg");
}

static void zwizwa_swd_write_reg(uint8_t cmd, uint32_t value, uint32_t ap_delay_clk)
{
	LOG_DEBUG("zwizwa_swd_write_reg");
}

static void zwizwa_exchange(bool rnw, uint8_t buf[], unsigned int offset, unsigned int bit_cnt)
{
	LOG_DEBUG("zwizwa_exchange");
}

static int zwizwa_swd_run_queue(void)
{
	static int queued_retval; // ???
	LOG_DEBUG("zwizwa_swd_run_queue");
	/* A transaction must be followed by another transaction or at least 8 idle cycles to
	 * ensure that data is clocked through the AP. */
	zwizwa_exchange(true, NULL, 0, 8);

	int retval = queued_retval;
	queued_retval = ERROR_OK;
	LOG_DEBUG("SWD queue return value: %02x", retval);
	return retval;
}


const struct swd_driver zwizwa_swd = {
	.init = zwizwa_swd_init,
	.switch_seq = zwizwa_swd_switch_seq,
	.read_reg = zwizwa_swd_read_reg,
	.write_reg = zwizwa_swd_write_reg,
	.run = zwizwa_swd_run_queue,
};


struct adapter_driver zwizwa_adapter_driver = {
	.name = "zwizwa",
	.transports = zwizwa_transports,
	.commands = zwizwa_command_handlers,

	.init = zwizwa_init,
	.quit = zwizwa_quit,
	.reset = zwizwa_reset,
	.speed = zwizwa_speed,
	.khz = zwizwa_khz,
	.speed_div = zwizwa_speed_div,

	.swd_ops = &zwizwa_swd,
};
