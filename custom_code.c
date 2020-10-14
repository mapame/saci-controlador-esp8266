#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "configuration.h"


int config_nivel_cisterna_addr;
int_list_t config_nivel_cisterna_ports;

int config_nivel_caixainf_addr;
int_list_t config_nivel_caixainf_ports;

int config_nivel_caixasup_addr;
int_list_t config_nivel_caixasup_ports;

int config_fluxo_rua_addr;
float config_fator_fluxo_rua;

const config_info_t extended_config_table[] = {
	{"nivel_cisterna_addr",		"Sensor de Nível - Cisterna (Endereço)",		'I', "",		0,	32,		1, (void*) &config_nivel_cisterna_addr},
	{"nivel_cisterna_ports",	"Sensor de Nível - Cisterna (Portas)",			'L', "",		0,	255,	1, (void*) &config_nivel_cisterna_ports},
	{"nivel_caixasup_addr",		"Sensor de Nível - Caixa Inf. (Endereço)",		'I', "",		0,	32,		1, (void*) &config_nivel_caixainf_addr},
	{"nivel_caixasup_ports",	"Sensor de Nível - Caixa Inf. (Portas)",		'L', "",		0,	255,	1, (void*) &config_nivel_caixainf_ports},
	{"nivel_caixainf_addr",		"Sensor de Nível - Caixa Sup. (Endereço)",		'I', "",		0,	32,		1, (void*) &config_nivel_caixasup_addr},
	{"nivel_caixainf_ports",	"Sensor de Nível - Caixa Sup. (Portas)",		'L', "",		0,	255,	1, (void*) &config_nivel_caixasup_ports},
	{"fluxo_rua_addr",			"Sensor de Fluxo (Endereço)",					'I', "",		0,	32,		1, (void*) &config_fluxo_rua_addr},
	{"fator_fluxo_rua",			"Sensor de Fluxo: Fator de Conversão",			'F', "1.0",		1,	100,	0, (void*) &config_fator_fluxo_rua},
};
const int extended_config_table_qty = sizeof(extended_config_table) / sizeof(config_info_t);
