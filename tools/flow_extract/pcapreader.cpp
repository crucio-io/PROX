/*
  Copyright(c) 2010-2015 Intel Corporation.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <pcap.h>
#include <cstring>
#include <linux/in.h>

#include "pcapreader.hpp"

int PcapReader::open(const string& file_path)
{
	char err_str[PCAP_ERRBUF_SIZE];

	if (m_handle) {
		m_error = "Pcap file already open";
		return -1;
	}

	m_handle = pcap_open_offline_with_tstamp_precision(file_path.c_str(),
							   PCAP_TSTAMP_PRECISION_NANO,
							   err_str);

	if (!m_handle) {
		m_error = "Failed to open pcap file";
		return -1;
	}

	m_file_beg = ftell(pcap_file(m_handle));
	fseek(pcap_file(m_handle), 0, SEEK_END);
	m_file_end = ftell(pcap_file(m_handle));
	fseek(pcap_file(m_handle), m_file_beg, SEEK_SET);

	return 0;
}

int PcapReader::readOnce(PcapPkt *pkt, uint64_t pos)
{
	return -1;
}

int PcapReader::read(PcapPkt *pkt)
{
	if (!m_handle) {
		m_error = "No pcap file opened";
	}

	const uint8_t *buf = pcap_next(m_handle, &pkt->header);

	if (buf) {
		memcpy(pkt->buf, buf, pkt->header.len);
		pktReadCount++;
	}

	return !!buf;
}

void PcapReader::close()
{
	if (m_handle)
		pcap_close(m_handle);

	m_handle = NULL;
}
