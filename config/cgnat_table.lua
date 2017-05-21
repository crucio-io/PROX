-- Copyright(c) 2010-2017 Intel Corporation.
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

local cgnat = {}
cgnat.dynamic = {
   {public_ip_range_start = ip("20.0.1.0"),public_ip_range_stop = ip("20.0.1.15"), public_port = val_range(0,65535)},
   {public_ip_range_start = ip("20.0.1.16"),public_ip_range_stop = ip("20.0.1.31"), public_port = val_range(0,65535)},
}
cgnat.static_ip_port = {
   {src_ip = ip("192.168.2.1"), src_port = 68, dst_ip = ip("20.0.2.1"), dst_port = 68},
   {src_ip = ip("192.168.2.1"), src_port = 168, dst_ip = ip("20.0.2.1"), dst_port = 5000},
   {src_ip = ip("192.168.2.1"), src_port = 268, dst_ip = ip("20.0.2.1"), dst_port = 5001},
   {src_ip = ip("192.168.2.1"), src_port = 368, dst_ip = ip("20.0.2.1"), dst_port = 5002},
}
cgnat.static_ip = {
   {src_ip = ip("192.168.3.1"), dst_ip = ip("20.0.3.1")},
   {src_ip = ip("192.168.3.2"), dst_ip = ip("20.0.3.2")},
   {src_ip = ip("192.168.3.3"), dst_ip = ip("20.0.3.3")},
   {src_ip = ip("192.168.3.4"), dst_ip = ip("20.0.3.4")},
   {src_ip = ip("192.168.3.5"), dst_ip = ip("20.0.3.5")},
   {src_ip = ip("192.168.3.6"), dst_ip = ip("20.0.3.6")},
   {src_ip = ip("192.168.3.7"), dst_ip = ip("20.0.3.7")},
   {src_ip = ip("192.168.3.8"), dst_ip = ip("20.0.3.8")},
}
return cgnat
