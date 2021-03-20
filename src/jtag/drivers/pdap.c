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
		if (EOF == c) return ERROR_FAIL;
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

static uint32_t **queue;
static int32_t queue_size = 0;
static int32_t queue_next;
static uint32_t nb_transactions;

void queue_write(uint32_t *dst) {
	if (queue_next >= queue_size) {
		queue_size *= 2;
		LOG_INFO("growing queue to %d", queue_size);
		int32_t nb_bytes = sizeof(uint32_t*) * queue_size;
		queue = realloc(queue, nb_bytes);
		if (!queue) exit(1);
	}
	queue[queue_next++] = dst;
}

int pdap_init(void)
{
	/* Called both by swd.init (first), then adapter.init, so just
	   make it idempotent. */
	if (dev) return ERROR_OK;
	queue_size = 1024 * 16;
	queue = malloc(sizeof(uint32_t*)*queue_size);
	if (!queue) exit(1);

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

	// http://www.cs.uleth.ca/~holzmann/C/system/ttyraw.c
	tio.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	tio.c_oflag &= ~(OPOST);
	tio.c_cflag |= (CS8);
	tio.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;

	if (0 != ioctl(devfd, TCSETS2, &tio)) return ERROR_FAIL;

	PDAP(" "); // discards any half command
	PDAP("0 echo");
	PDAP("hex");
	PDAP("%x sync", nb_transactions++);
	char buf[100];
	for(;;) {
		if (ERROR_FAIL == pdap_resp(buf, sizeof(buf))) return ERROR_FAIL;
		if (!strncmp("sync ", buf, 5)) break;
	}
	LOG_INFO("PDAP sync ok");

	return ERROR_OK;
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

//#define LOG_ LOG_DEBUG
#define LOG_ LOG_INFO


void pdap_swd_read_reg(uint8_t cmd, uint32_t *value, uint32_t ap_delay_clk)
{
	if (value) {
		PDAP("%x rd p", cmd);
		queue_write(value);
	}
	else {
		PDAP("%x rd drop", cmd);
	}
	if (ap_delay_clk) {
		PDAP("%x ap_delay_clk", ap_delay_clk);
	}
}

void pdap_swd_write_reg(uint8_t cmd, uint32_t value, uint32_t ap_delay_clk)
{
	PDAP("%x %x wr", value, cmd);
	if (ap_delay_clk) {
		PDAP("%x ap_delay_clk", ap_delay_clk);
	}
}



int pdap_swd_run_queue(void)
{
	int rv = ERROR_FAIL;
	LOG_INFO("run %d\n", queue_next);
	PDAP("%x sync", nb_transactions);
	/* There will be one line per read/write command, and a sync at the end. */
	char buf[100] = {};
	for (int32_t i = 0; i < queue_next; i++) {
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
		*(queue[i]) = val;
	}
	if (ERROR_FAIL == pdap_resp(buf, sizeof(buf))) {
		LOG_ERROR("sync read fail: '%s'", buf);
		goto fail;
	}
	if (strncmp("sync ", buf, 5)) {
		LOG_ERROR("bad sync: '%s'", buf);
		goto fail;
	}
	rv = ERROR_OK;
fail:
	nb_transactions++;
	queue_next = 0;
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
