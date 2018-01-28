--
-- Copyright(c) 2010-2015 Intel Corporation.
-- Copyright(c) 2016-2018 Viosoft Corporation.
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

t2={}
svlan_id=0
ip1=2^24*192 + 2^16*168 + 2^8*0 + 0;
for id1=0,3,1 do
  for cvlan_id=0,255,1 do
    mac_s=string.format("00:00:01:00:00:%02x", cvlan_id);
    table.insert(t2,{dest_id=id1, gre_id=0, svlan_id=svlan_id, cvlan_id=cvlan_id, cidr = {ip=ip(ip1),depth=29}, mac = mac(mac_s), user_id=cvlan_id});
    ip1=ip1+8
    table.insert(t2,{dest_id=id1, gre_id=0, svlan_id=svlan_id+1, cvlan_id=cvlan_id, cidr = {ip=ip(ip1),depth=29}, mac = mac(mac_s), user_id=cvlan_id});
    ip1=ip1+8
  end
  svlan_id=svlan_id+16
end

return t;

