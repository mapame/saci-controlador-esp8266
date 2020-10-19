typedef struct dashboard_item_type_s {
	char name[16];
	char parameter_types[5];
} dashboard_item_type_t;

typedef struct dashboard_item_s {
	int line;
	int width[3];
	char type_name[16];
	char dname[64];
	void * parameters[4];
} dashboard_item_t;

extern const int dashboard_item_type_qty;
extern const dashboard_item_type_t dashboard_item_types[];

extern const int dashboard_line_title_qty;
extern const char dashboard_line_titles[][64];

extern const int dashboard_item_qty;
extern const dashboard_item_t dashboard_itens[];


int dashboard_type_get_index(const char *type_name);
