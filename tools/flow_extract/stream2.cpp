/*
  Copyright(c) 2010-2017 Intel Corporation.
  Copyright(c) 2016-2017 Viosoft Corporation.
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

#include <iomanip>
#include <arpa/inet.h>
#include <sstream>

#include "stream.hpp"
#include "stream2.hpp"

int Stream2::fromFile(ifstream *f)
{
	m_actions.clear();
	if (streamHdr.fromFile(f))
		return -1;
	if (actionsFromFile(f, streamHdr.actionCount))
		return -1;
	if (setReferences(f))
		return -1;

	return 0;
}

int Stream2::actionsFromFile(ifstream *f, size_t actionCount)
{
	m_actions.resize(actionCount);
	for (size_t i = 0; i < actionCount; ++i)
		f->read((char *)&m_actions[i], sizeof(Stream::ActionEntry));

	return 0;
}

int Stream2::setReferences(ifstream *f)
{
	size_t toRead = streamHdr.clientHdrLen +
		streamHdr.serverHdrLen +
		streamHdr.clientContentLen +
		streamHdr.serverContentLen;

	delete [] clientServerHdrContent;
	clientServerHdrContent = new uint8_t[toRead];
	f->read((char *)clientServerHdrContent, toRead);
	return 0;
}

void Stream2::calcOffsets(ofstream *out)
{
	size_t curPos = out->tellp();

	clientHdrBeg = curPos;
	serverHdrBeg = clientHdrBeg + streamHdr.clientHdrLen;
	clientContentBeg = serverHdrBeg + streamHdr.serverHdrLen;
	serverContentBeg = clientContentBeg + streamHdr.clientContentLen;
}

void Stream2::toFile(ofstream *out) const
{
	size_t len = streamHdr.clientHdrLen +
		streamHdr.serverHdrLen +
		streamHdr.clientContentLen +
		streamHdr.serverContentLen;

	out->write((const char *)clientServerHdrContent, len);
}

static string ipToString(const uint32_t ip)
{
	uint32_t ip_ne = htonl(ip);
	stringstream ss;

	ss << ((ip_ne >> 24) & 0xff) << "."
	   << ((ip_ne >> 16) & 0xff) << "."
	   << ((ip_ne >> 8) & 0xff) << "."
	   << (ip_ne & 0xff);

	return ss.str();
}

static string spaces(uint32_t count)
{
	stringstream ss;

	while (count--)
		ss << " ";
	return ss.str();
}

NetSocket Stream2::getServerNetSocket() const
{
	return NetSocket(streamHdr.serverIP, ntohs(streamHdr.serverPort));
}

NetSocket Stream2::getClientNetSocket() const
{
	return NetSocket(streamHdr.clientIP, ntohs(streamHdr.clientPort));
}
void Stream2::setServerNetSocket(const NetSocket& netSocket)
{
	streamHdr.serverPort = htons(netSocket.port);
	streamHdr.serverIP = netSocket.host;
}

void Stream2::setClientNetSocket(const NetSocket& netSocket)
{
	streamHdr.clientPort = htons(netSocket.port);
	streamHdr.clientIP = netSocket.host;
}
void Stream2::toLua(ofstream *f, const string& binFileName, const string& streamTableName) const

{
	(*f) << std::fixed;

	(*f) << streamTableName << "[" << streamHdr.streamId << "] = {" << endl
	     << spaces(3) << "client_data = {" << endl
	     << spaces(6) << "header = bin_read(" << binFileName << "," << clientHdrBeg << "," << streamHdr.clientHdrLen << "), " << endl
	     << spaces(6) << "content = bin_read(" << binFileName << "," << clientContentBeg << "," << streamHdr.clientContentLen << "), " << endl
	     << spaces(3) << "}," << endl
	     << spaces(3) << "server_data = {" << endl
	     << spaces(6) << "header = bin_read(" << binFileName << "," << serverHdrBeg << "," << streamHdr.serverHdrLen << "), " << endl
	     << spaces(6) << "content = bin_read(" << binFileName << "," << serverContentBeg << "," << streamHdr.serverContentLen << "), " << endl
	     << spaces(3) << "}," << endl
	     << spaces(3) << "actions = {" << endl;

	for (size_t i = 0; i < m_actions.size(); ++i) {
		const char *peer_str = m_actions[i].peer == 0? "client" : "server";

		(*f) << spaces(6) <<  peer_str << "_content(" << m_actions[i].beg << "," << m_actions[i].len << ")," << endl;
	}

	(*f) << spaces(3) << "}," << endl
	     << spaces(3) << "clients = {ip = ip(\"" << ipToString(streamHdr.clientIP) << "\"), port = " << ntohs(streamHdr.clientPort) << "}," << endl
	     << spaces(3) << "servers = {ip = ip(\"" << ipToString(streamHdr.serverIP) << "\"), port = " << ntohs(streamHdr.serverPort) << "}," << endl
	     << spaces(3) << "l4_proto = \"" << (streamHdr.protocol == 0x06? "tcp" : "udp") << "\"," << endl
	     << spaces(3) << "up_bps = " << setprecision(4) << streamHdr.upRate << "," << endl
	     << spaces(3) << "dn_bps = " << setprecision(4) << streamHdr.dnRate << "," << endl;

	(*f) << "}" << endl;
}
