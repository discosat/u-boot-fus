/*
 * sdp.h - Serial Download Protocol
 *
 * Copyright (C) 2017 Toradex
 * Author: Stefan Agner <stefan.agner@toradex.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __SDP_H_
#define __SDP_H_

/**
 * struct sdp_stream_ops - Call back functions for SDP in stream mode
 * @new_file:	indicate the beginning of a new file download
 * @rx_data:	handle the next data chunk of a file
 */
struct sdp_stream_ops {
	/* A new file with size should be downloaded to dnl_address */
	void (*new_file)(u32 dnl_address, u32 size);

	/* The next data_len bytes have been received in data_buf */
	void (*rx_data)(u8 *data_buf, int data_len);
};

int sdp_init(int controller_index);
void sdp_handle(int controller_index,
		const struct sdp_stream_ops *ops, bool single);

int spl_sdp_stream_image(const struct sdp_stream_ops *ops, bool single);
int spl_sdp_stream_continue(const struct sdp_stream_ops *ops, bool single);

#endif /* __SDP_H_ */
