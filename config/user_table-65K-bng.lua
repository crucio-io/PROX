--
-- Copyright(c) 2010-2015 Intel Corporation.
-- Copyright(c) 2016-2017 Viosoft Corporation.
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
--

-- This script generates a user table containing 65536 users. It is
-- meant to be used in a BNG with 2 CPE facing ports. Each of the CPE
-- facing ports has 32768 users behind it. Each user has a unique
-- svlan/cvlan combination. The only difference between the two sets
-- of users is the svlan id. Note that any arbitrary configuration is
-- possible.

local user_table = {}

for i = 1,2^15 do
   idx = i - 1
   user_table[i] = {
      gre_id   = idx,
      -- svlan_id is 000000000XXXXXXX at the bit level
      -- cvlan_id is 0000XXXX00XX00XX at the bit level
      svlan_id = mask(idx, 0x7f00) / 2^8,
      cvlan_id = mask(idx, 0xf0) * 2^4 + mask(idx, 0xc) * 2^2 + mask(idx, 0x3),
      user_id  = idx,
   }
end

for i = 1,2^15 do
   idx = i - 1
   user_table[2^15 + i] = {
      gre_id   = 2^15 + idx,
      -- svlan_id is 000000001XXXXXXX at the bit level
      -- cvlan_id is 0000XXXX00XX00XX at the bit level
      svlan_id = mask(idx, 0x7f00) / 2^8 + 0x80,
      cvlan_id = mask(idx, 0xf0) * 2^4 + mask(idx, 0xc) * 2^2 + mask(idx, 0x3),
      user_id  = idx,
   }
end

return user_table
