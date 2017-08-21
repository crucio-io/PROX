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

seven_tuple = function(svlan, cvlan, ip_proto, src, dst, sport, dport, action)
   return {
      svlan_id = svlan,
      cvlan_id = cvlan,
      ip_proto = ip_proto,
      src_cidr = src,
      dst_cidr = dst,
      sport    = sport,
      dport    = dport,
      action   = action,
   }
end

rules2={};
sports={0,2,4,6,8,10,12,14};
src_t={{s="192.168.0.0/18", svlan1=0, svlan2=1},
       {s="192.168.16.0/18", svlan1=16, svlan2=17},
       {s="192.168.32.0/18", svlan1=32, svlan2=33},
       {s="192.168.48.0/18", svlan1=48, svlan2=49},
      };

for srck,srcv in pairs(src_t) do
 for cvlan_mask = 0,255 do
  for spk,spv in pairs(sports) do
    table.insert(rules2,seven_tuple(val_mask(srcv.svlan1,0x0fff), val_mask(cvlan_mask,0x0fff), val_mask(17,0xff), cidr(srcv.s), cidr("10.0.0.0/7"), val_range(spv,spv), val_range(0,511), "allow"));
    table.insert(rules2,seven_tuple(val_mask(srcv.svlan1,0x0fff), val_mask(cvlan_mask,0x0fff), val_mask(17,0xff), cidr(srcv.s), cidr("74.0.0.0/7"), val_range(spv,spv), val_range(0,511), "allow"));
    table.insert(rules2,seven_tuple(val_mask(srcv.svlan2,0x0fff), val_mask(cvlan_mask,0x0fff), val_mask(17,0xff), cidr(srcv.s), cidr("10.0.0.0/7"), val_range(spv,spv), val_range(0,511), "allow"));
    table.insert(rules2,seven_tuple(val_mask(srcv.svlan2,0x0fff), val_mask(cvlan_mask,0x0fff), val_mask(17,0xff), cidr(srcv.s), cidr("74.0.0.0/7"), val_range(spv,spv), val_range(0,511), "allow"));
   table.insert(rules2,rules4);
  end
 end
end

return rules2

