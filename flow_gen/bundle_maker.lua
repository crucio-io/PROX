-- Copyright(c) 2010-2015 Intel Corporation.
-- All rights reserved.
--
-- Redistribution and use in source and binary forms, with or without
-- modification, are permitted provided that the following conditions
-- are met:
--
--   * Redistributions of source code must retain the above copyright
--     notice, this list of conditions and the following disclaimer.
--   * Redistributions in binary form must reproduce the above copyright
--     notice, this list of conditions and the following disclaimer in
--     the documentation and/or other materials provided with the
--     distribution.
--   * Neither the name of Intel Corporation nor the names of its
--     contributors may be used to endorse or promote products derived
--     from this software without specific prior written permission.
--
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
-- "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
-- LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
-- A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
-- OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
-- SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
-- LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
-- DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
-- THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
-- (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
-- OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

function get_client_bundles(bundles)
   local client_bundles = {};

   for i,b in ipairs(bundles) do
      client_bundles[i] = {bundle = b, imix_fraction = 1}
   end

   return client_bundles;
end

function get_server_streams(bundles)
   local server_streams = {}
   n_listen = 0
   for i, bundle in ipairs(bundles) do
      for j, stream in ipairs(bundle) do
	 n_listen = n_listen + 1
	 server_streams[n_listen] = stream
      end
   end
   return server_streams;
end

function setup_bundles(first_ip_byte, speed_scaling)
   bundles = dofile("cfg.lua")

   local client_bundles = get_client_bundles(bundles);
   local server_streams = get_server_streams(bundles);

   for i,e in ipairs(client_bundles) do
      for j,stream in ipairs(e.bundle) do
	 stream.clients.ip[1] = first_ip_byte
	 stream.clients.port_mask = 0xffff
      end
   end

   for i,stream in ipairs(server_streams) do
      stream.servers.ip[1] = first_ip_byte
   end

   local highest_bps = 0;
   for i,e in ipairs(client_bundles) do
      for j,s in ipairs(e.bundle) do
	 if (s.up_bps ~= 1250000000 and s.dn_bps ~= 1250000000) then
	    if (highest_bps < s.up_bps) then
	       highest_bps = s.up_bps
	    end
	    if (highest_bps < s.dn_bps) then
	       highest_bps = s.dn_bps
	    end
	 end
      end
   end

   if (highest_bps == 0) then
      highest_bps = 1250000000
   end
   max_ss = 1250000000/highest_bps

   if (max_ss_and_quit == not nil and max_ss_and_quit == true) then
      print("max ss=" .. max_ss .. "")
      os.exit(0);
   end

   if (speed_scaling > max_ss) then
      error("Scaling too high (maximum scaling is " .. max_ss .. ")")
   end

   for i,e in ipairs(client_bundles) do
      for j,s in ipairs(e.bundle) do
	 if (s.up_bps ~= 1250000000 and s.dn_bps ~= 1250000000) then
	    s.up_bps = s.up_bps * speed_scaling;
	    s.dn_bps = s.dn_bps * speed_scaling;
	 end
      end
   end

   return client_bundles, server_streams
end
