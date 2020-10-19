#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dashboard.h"

const dashboard_item_type_t dashboard_item_types[] = {
	{"vertical_gauge", "FTNN"}, /* percentage, label */
	{"text", "TTNN"}, /* text, text_color */
	{"button", "ITNN"} /* enabled, command */
};

const int dashboard_item_type_qty = sizeof(dashboard_item_types) / sizeof(dashboard_item_type_t);

int dashboard_type_get_index(const char *type_name) {
	if(type_name == NULL)
		return -1;
	
	for(int i = 0; i < dashboard_item_type_qty; i++)
		if(!strcmp(type_name, dashboard_item_types[i].name))
			return i;
	
	return -2;
}
