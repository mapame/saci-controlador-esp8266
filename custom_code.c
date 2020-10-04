#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "configuration.h"


char config_nivel_cisterna_acp[CONFIG_STR_SIZE];
char config_nivel_caixa_inf_acp[CONFIG_STR_SIZE];
char config_nivel_caixa_sup_acp[CONFIG_STR_SIZE];
char config_fluxo_rua_acp[CONFIG_STR_SIZE];

float config_fator_fluxo_rua;


const int extended_config_table_qty = 5;

const config_info_t extended_config_table[] = {
	{"nivel_cisterna_acp",	"Nível Cisterna: Endereço/Canal/Portas",		'T', "",		0,  63, 0, (void*) &config_nivel_cisterna_acp},
	{"nivel_caixa_inf_acp",	"Nível Caixa Inferior: Endereço/Canal/Portas",	'T', "",		0,  63, 0, (void*) &config_nivel_caixa_inf_acp},
	{"nivel_caixa_sup_acp",	"Nível Caixa Superior: Endereço/Canal/Portas",	'T', "",		0,  63, 0, (void*) &config_nivel_caixa_sup_acp},
	{"fluxo_rua_acp",		"Sensor de Fluxo da Rua: Endereço/Canal/Portas",'T', "",		0,  63, 0, (void*) &config_fluxo_rua_acp},
	{"fator_fluxo",			"Sensor de Fluxo da Rua: Fator de Conversão",	'F', "1.0",		1, 100, 0, (void*) &config_fator_fluxo_rua},
};
