/*
  Copyright(c) 2016-2018 Viosoft Corporation.
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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "ipv4_range_parser.h"

#define ADDRESS_MAX     40 // {254-255}.{254-255}.{254-255}.{254-255}

static bool is_valid_ipv4_octet(const char *octet, int *value)
{
    size_t i, len = strlen(octet);
    if (len > 3) return false;
    for (i = 0; i < len; i++)
        if (octet[i] < '0' || octet[i] > '9')
            return false;

    *value = atoi(octet);
    if (*value > 255)
        return false;

    return true;
}

static bool is_valid_ipv4_range(const char *octet, int *range_start, int *range_end)
{
    if (octet[0] != '{' || octet[strlen(octet) - 1] != '}')
        return false;

    char octet_w[ADDRESS_MAX];
    strcpy(octet_w, octet);
    octet_w[0] = ' ';
    octet_w[strlen(octet) - 1] = '\0';

    char *rest = octet_w;
    char *token;

    size_t octet_index = 0;
    if (!(token = strtok_r(rest, "-", &rest)))
        return false;
    *range_start = atoi(token);

    if (!(token = strtok_r(rest, "-", &rest)))
        return false;
    *range_end = atoi(token);

    return (*range_start < *range_end);
}

bool parse_ipv4_address_range(char *address, struct ipv4_address_range *address_range)
{
    if (strlen(address) > ADDRESS_MAX)
        return false;

    size_t i, dots_count = 0;
    for (i = 0; i < strlen(address); i++)
    {
        if (address[i] == '.') dots_count++;
        if ((address[i] < '0' || address[i] > '9') && address[i] != '}' && address[i] != '{' && address[i] != '-' && address[i] != '.')
            return false;
    }

    if (dots_count != 3)
        return false;

    char address_w[ADDRESS_MAX];
    strcpy(address_w, address);
    char *rest = address_w;
    char *token;

    memset(address_range, 0, sizeof(struct ipv4_address_range));

    size_t octet_index = 0;
    while ((token = strtok_r(rest, ".", &rest)))
    {
        if (is_valid_ipv4_octet(token, &address_range->octet[octet_index]))
        {
            octet_index++;
            continue;
        }

        if (is_valid_ipv4_range(token, &address_range->octet_start[octet_index], &address_range->octet_end[octet_index]))
        {
            address_range->octet[octet_index] = address_range->octet_start[octet_index];
            octet_index++;
            continue;
        }
        return false;
    }
    return true;
}

void get_next_ipv4_address(struct ipv4_address_range *range, uint32_t *ipv4)
{
    unsigned int octets[4];
    int i;
    bool can_increment = true;

    for (i = 4; i >= 0; i--)
    {
        octets[i] = range->octet[i];
        if (range->octet_start[i] > 0 && range->octet_end[i] > 0 && can_increment)
        {
            range->octet[i] = (range->octet[i] + 1) % (range->octet_end[i] + 1);
            if (range->octet[i] == 0)
                range->octet[i] = range->octet_start[i];
            else
                can_increment = false;
        }
    }

    *ipv4 = (octets[3] & 0xFF);
    *ipv4 = ((*ipv4 << 8) | (octets[2] & 0xFF));
    *ipv4 = ((*ipv4 << 8) | (octets[1] & 0xFF));
    *ipv4 = ((*ipv4 << 8) | (octets[0] & 0xFF));

    return;
}
