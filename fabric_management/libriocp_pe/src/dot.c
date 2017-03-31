/*
 * Copyright (c) 2014, Prodrive Technologies
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file dot.c
 * Processing element dot graphing
 */
#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>

#include "inc/riocp_pe.h"
#include "inc/riocp_pe_internal.h"

#include "pe.h"
#include "llist.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Print dot graph node from PE to file stream
 * @param file Opened output filestream
 * @param pe   Target PE
 */
static int riocp_pe_dot_print_node(FILE *file, struct riocp_pe *pe)
{
	did_val_t did_val;
	did_t did;
	pe_rt_val route_port;
	int ret = 0;

	fprintf(file, "\t\"0x%08x\" ", pe->comptag);
	fprintf(file, "[label=\"%s\\nct: 0x%08x", riocp_pe_get_device_name(pe), pe->comptag);

	if (!RIOCP_PE_IS_SWITCH(pe->cap))
		fprintf(file, "\\nid:0x%02x", pe->did_reg_val);

	fprintf(file, "\" URL=\"javascript:parent.nodeselect(%08x)\" tooltip=\"", pe->comptag);
	fprintf(file, "%s %s (0x%08x)&#10;", riocp_pe_get_vendor_name(pe),
		riocp_pe_get_device_name(pe), pe->cap.dev_id);

	/* Put the switch routes in the tooltip */
	if (RIOCP_PE_IS_SWITCH(pe->cap)) {
		for (did_val = 0; did_val < 256; did_val++) {
			if (did_get(&did, did_val)) {
				continue;
			}

			ret = riocp_sw_get_route_entry(pe, 0xff, did, &route_port);
			if (ret) {
				RIOCP_ERROR("Could not get route for pe 0x%08x\n", pe->comptag);
				return ret;
			}

			if (route_port != 0xff)
				fprintf(file, "%02x:%u&#10;", did_val, route_port);
		}
	}
	fprintf(file, "\"];\n");

	return ret;
}

/**
 * Dump DOT language of connected peers
 * @param file Filestream to dump output to
 * @param list List of already seen PEs
 * @param pe   Target PE to dump
 */
static int riocp_pe_dot_dump_foreach(FILE *file, struct riocp_pe_llist_item *list,
	struct riocp_pe *pe)
{
	unsigned int n = 0;
	struct riocp_pe *peer;
	int ret = 0;

	/* Check if we already seen PE */
	if (riocp_pe_llist_find(list, pe) == NULL)
		riocp_pe_llist_add(list, pe);
	else
		return 0;

	/* Print current node */
	ret = riocp_pe_dot_print_node(file, pe);
	if (ret)
		return ret;

	/* Print links */
	for (n = 0; n < RIOCP_PE_PORT_COUNT(pe->cap); n++) {
		peer = pe->peers[n].peer;
		if (peer == NULL)
			continue;

		/* Crude hack to not print links double, only print them from the lowest comptag */
		if (pe->comptag < peer->comptag) {
			fprintf(file, "\t\t\"0x%08x\" -- \"0x%08x\" [taillabel=%u headlabel=%u ",
				pe->comptag, peer->comptag, n, pe->peers[n].remote_port);
			fprintf(file, "URL=\"javascript:parent.edgeselect(%08x:%u)\"",
				pe->comptag, n);
			fprintf(file, "tooltip=\"\"];\n");
		}
	}

	/* Recursively do the same for peer */
	for (n = 0; n < RIOCP_PE_PORT_COUNT(pe->cap); n++) {
		peer = pe->peers[n].peer;
		if (peer == NULL)
			continue;

		riocp_pe_dot_dump_foreach(file, list, peer);
	}

	return ret;
}

/**
 * Dump DOT graph from RapidIO master port
 * @param filename File to dump dot graph to
 * @param mport    RapidIO master port (root node of graph)
 * @retval -EINVAL Invalid root node
 * @retval -EPERM  Unable to open filename for writing
 * @retval -EIO    Error with RapidIO maintenance access
 */
int RIOCP_SO_ATTR riocp_pe_dot_dump(const char *filename, riocp_pe_handle mport)
{
	FILE *file;
	struct riocp_pe_llist_item *seen;
	int ret = 0;

	if (mport == NULL)
		return -EINVAL;

	file = fopen(filename, "w");
	if (file == NULL)
		return -EPERM;

	seen = (struct riocp_pe_llist_item *)calloc(1, sizeof(*seen));

	fprintf(file, "# Autogenerated DOT graph by libriocp_pe\n");
	fprintf(file, "graph network {\n");
	fprintf(file, "overlap=false;\n");
	fprintf(file, "splines=line;\n");
	fprintf(file, "pad=\"0.2,0.0\";\n");
	fprintf(file, "ranksep=1.1;\n");

	ret = riocp_pe_dot_dump_foreach(file, seen, mport);
	if (ret) {
		RIOCP_ERROR("Could not dump dot graph\n");
		goto out;
	}
	fprintf(file, "}\n");

out:
	fclose(file);
	riocp_pe_llist_free(seen);

	return ret;
}

#ifdef __cplusplus
}
#endif
