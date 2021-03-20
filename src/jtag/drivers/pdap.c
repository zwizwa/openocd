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
#include <stdio.h>
#include <asm-generic/termbits.h>
#include <asm-generic/ioctls.h>
#include <sys/ioctl.h>

const struct command_registration pdap_command_handlers[] = {
	COMMAND_REGISTRATION_DONE
};

const char * const pdap_transports[] = { "swd", NULL };


static FILE *dev;

static FILE *dbg;
#define DBG(...) if (dbg) {				\
		fprintf(dbg, __VA_ARGS__);		\
		fprintf(dbg, "\n");			\
	}

#define PDAP(...) {				\
		/* LOG_DEBUG("req: " __VA_ARGS__); */	\
		DBG("  "__VA_ARGS__);		\
		fprintf(dev, __VA_ARGS__);		\
		fprintf(dev, "\n");			\
	}



int pdap_resp(char *buf, int len)
{
	buf[0] = 0;
	int i = 0;
	for (;;) {
		if (i >= (len-1)) return ERROR_FAIL;
		int c = fgetc(dev);
		if (EOF == c) {
			/* This means the device disappeared. */
			LOG_ERROR("PDAP EOF");
			exit(1);
			//return ERROR_FAIL;
		}
		if ('\r' == c) continue;
		if ('\n' == c) {
			buf[i] = 0;
			if ((len > 0) && buf[0] == '#') {
				// Pass on firmware diagnostics.
				DBG("< %s", buf);
				LOG_INFO("%s", buf);
				i = 0;
				buf[0] = 0;
				continue;
			}
			else {
				DBG(". %s", buf);
				return i;
			}
		}
		buf[i++] = c;
	}
}

static uint32_t nb_transactions;
static int last_error;

static int pdap_sync(int discard) {
	int rv = ERROR_OK;

	/* Just perform synchronization here to make sure we're still
	 * in lock step. */
	PDAP("%x sync", nb_transactions);

	char buf[100] = {};

next_line:
	if (ERROR_FAIL == pdap_resp(buf, sizeof(buf))) {
		LOG_ERROR("sync read fail: '%s'", buf);
		rv = ERROR_FAIL;
		goto done;
	}
	if (strncmp("sync ", buf, 5)) {
		if (discard) goto next_line;

		LOG_ERROR("bad sync: '%s'", buf);
		rv = ERROR_FAIL;
		goto done;
	}

	uint32_t sync = strtol(buf + 5, NULL, 16);
	if (sync != nb_transactions) {
		LOG_ERROR("bad sync: %d != %d", sync, nb_transactions);
		rv = ERROR_FAIL;
		goto done;
	}
done:
	nb_transactions++;
	return rv;

}

int pdap_init(void)
{
	/* Called both by swd.init (first), then adapter.init, so just
	   make it idempotent. */
	if (dev) return ERROR_OK;

	last_error = ERROR_OK;


	const char *dbgname = getenv("PDAP_LOGFILE");
	if (dbgname) {
		dbg = fopen(dbgname, "w");
	}
	const char *devname = getenv("PDAP_TTY");
	if (!devname) {
		devname = "/dev/ttyACM0";
	}

	dev = fopen(devname, "r+");
	if (!dev) {
	    LOG_ERROR("Can't open %s", devname);
	    return ERROR_FAIL;
	}
	LOG_INFO("PDAP device %s", devname);
	int devfd = fileno(dev);

	struct termios2 tio;
	if (0 != ioctl(devfd, TCGETS2, &tio)) return ERROR_FAIL;

	tio.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	tio.c_oflag &= ~(OPOST);
	tio.c_cflag |= (CS8);
	tio.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;

	if (0 != ioctl(devfd, TCSETS2, &tio)) return ERROR_FAIL;

	/* Send empty command to discard any junk that might be on the
	   command line.  Disable echo for machine interaction
	   (default is on), and switch to hex mode (default is
	   decimal.  Perform sync, discarding any non-sync lines to
	   skip the controller startup messages. */
	PDAP(" ");
	PDAP("0 echo");
	PDAP("hex");
	return pdap_sync(1);
}

int pdap_quit(void)
{
	return ERROR_OK;
}

/* (1) assert or (0) deassert reset lines */
int pdap_reset(int trst, int srst)
{
	// needs: reset_config srst_only
	PDAP("%x srst", srst);
	return ERROR_OK;
}

int pdap_swd_switch_seq(enum swd_special_seq seq)
{
	switch (seq) {
	case LINE_RESET:
		PDAP("line_reset");
		break;
	case JTAG_TO_SWD:
		PDAP("jtag_to_swd");
		break;
	case SWD_TO_JTAG:
		PDAP("swd_to_jtag");
		break;
	default:
		LOG_ERROR("Sequence %d not supported", seq);
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

void pdap_swd_read_reg(uint8_t cmd, uint32_t *pval, uint32_t ap_delay_clk)
{
	if (last_error != ERROR_OK) {
		/* Don't continue if anything went wrong in the
		   current queue run. */
		return;
	}
	if (pval) {
		/* 'rd' pushes to stack, 'p' prints hex number. */
		PDAP("%x rd p", cmd);
		char buf[100] = {};
		if (ERROR_FAIL == pdap_resp(buf, sizeof(buf))) {
			LOG_ERROR("value read fail: '%s'", buf);
			goto fail;
		}
		if (!strncmp("error ack ", buf, 10)) {
			uint32_t ack = strtol(buf + 10, NULL, 16);
			LOG_ERROR("ack = %d", ack);
			goto fail;
		}
		uint32_t val = strtol(buf, NULL, 16);
		LOG_DEBUG("val = 0x%x", val);
		*pval = val;
	}
	else {
		/* 'rd' pushes to stack, 'drop' discards result
		   without printing. */
		PDAP("%x rd drop", cmd);
	}
	if (ap_delay_clk) {
		PDAP("%x idle", ap_delay_clk);
	}
	last_error = ERROR_OK;
	return;
fail:
	last_error = ERROR_FAIL;
}

void pdap_swd_write_reg(uint8_t cmd, uint32_t value, uint32_t ap_delay_clk)
{
	PDAP("%x %x wr", value, cmd);
	if (ap_delay_clk) {
		PDAP("%x idle", ap_delay_clk);
	}
}

int pdap_swd_run_queue(void)
{
	int rv;

	/* Make sure we're still in lock step. */
	if (ERROR_OK == (rv = pdap_sync(0))) {
		rv = last_error;
	}

	/* Set things up for the next queue run. */
	last_error = ERROR_OK;
	return rv;
}


const struct swd_driver pdap_swd = {
	.init = pdap_init,
	.switch_seq = pdap_swd_switch_seq,
	.read_reg = pdap_swd_read_reg,
	.write_reg = pdap_swd_write_reg,
	.run = pdap_swd_run_queue,
};


struct adapter_driver pdap_adapter_driver = {
	.name = "pdap",
	.transports = pdap_transports,
	.commands = pdap_command_handlers,

	.init = pdap_init,
	.quit = pdap_quit,
	.reset = pdap_reset,

	.swd_ops = &pdap_swd,
};
