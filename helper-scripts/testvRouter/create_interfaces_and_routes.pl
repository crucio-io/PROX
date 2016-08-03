#!/bin/env perl
#
# Copyright(c) 2010-2016 Intel Corporation.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#   * Neither the name of Intel Corporation nor the names of its
#     contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
##

# This script creates four sets of files: 2 sets for use case 0 and 1
# (which use the same configuration) and 2 for use case 2.
# Each use case is defined by 2 sets of configuration files.
# interface.txt contains the IP addresses of the DPDK fast path interfaces.
# route.x.y.txt contains the routing table for different configurations
# with x being number of routes and y number of next_hops.
# Those interface.txt and route.x.y.txt files should then be converted
# to fit the syntax of vRouter configuration files.

use strict;
my $max_nb_routes = 8192;
my $max_nb_next_hops = 1024;
my $max_nb_interfaces = 4;
my $nb_next_hops = 1;
my ($interface, $a1, $a2, $a3, $a4, $fh, $output_route);

# Create interface configuration for use case 0 and 1
my $interface_config = "interface.txt";
open($fh, '>', $interface_config) or die "Could not open file '$interface_config' $!";
print $fh "# interface IP address/prefix\n";
for ($interface = 0; $interface < $max_nb_interfaces; $interface++) {
	print $fh ($interface+64).".0.0.240/24\n";	
}
close $fh;

# Create interface configuration for use case 2
my $interface_config = "interface_use_case_2.txt";
open($fh, '>', $interface_config) or die "Could not open file '$interface_config' $!";
print $fh "# interface IP address/prefix\n";
for ($interface = 0; $interface < $max_nb_interfaces; $interface++) {
	print $fh ($interface * 8 + 1).".0.0.240/5\n";	
}
close $fh;

# Create routes configuration for use case 0 and 1
while ($nb_next_hops <= $max_nb_next_hops) {
	my $nb_routes_per_interface = $nb_next_hops;
	while ($nb_routes_per_interface <= $max_nb_routes) {
		$output_route = "route.".$nb_routes_per_interface.".".$nb_next_hops.".txt";
		open($fh, '>', $output_route) or die "Could not open file '$output_route' $!";
		print $fh "# destination/prefix;nex-hop\n";

		for (my $route_nb = 0; $route_nb < $nb_routes_per_interface; $route_nb++) {
			for ($interface = 0; $interface < $max_nb_interfaces; $interface++) {
				$a1 = $interface * 8 + 1 + (($route_nb & 1) << 2) + ($route_nb & 2);
				$a2 = (($route_nb & 4) << 5) + (($route_nb & 8) << 1) + (($route_nb & 0x10) >> 1) + (($route_nb & 0x20) >> 4) + (($route_nb & 0x40) >> 6);
				$a3 = (($route_nb & 0x80)) + (($route_nb & 0x100) >> 2) + (($route_nb & 0x200) >> 5) + (($route_nb & 0x400) >> 7) + (($route_nb & 0x800) >> 10) + (($route_nb & 0x1000) >> 12);
				$a4 = 0;
				print $fh $a1.".".$a2.".".$a3.".".$a4."/24;";
				print $fh ($interface+64).".0.".(($route_nb % $nb_next_hops) >> 7).".".(1 + (($route_nb % $nb_next_hops) & 0x7f)) ."\n";
			}
		}
		$nb_routes_per_interface = $nb_routes_per_interface * 2;
	}
	$nb_next_hops = $nb_next_hops * 2;		
}
close $fh;

# Create routes configuration for use case 2
$output_route = "route.1.1.use_case_2.txt";
open($fh, '>', $output_route) or die "Could not open file '$output_route' $!";
print $fh "# destination/prefix;nex-hop\n";

for ($interface = 0; $interface < $max_nb_interfaces; $interface++) {
	$a1 = $interface + 64 ;
	$a2 = 0;
	$a3 = 0;
	$a4 = 0;
	print $fh $a1.".".$a2.".".$a3.".".$a4."/24;";
	print $fh ($interface * 8 + 1).".0.0.1\n";
}
close $fh;
