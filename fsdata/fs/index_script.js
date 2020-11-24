var ws;
var receiveTimeoutInterval;
var logged = false;
var configPageDone = false;
var configPageShown = false;
var timeAlertShown = false;

window.onload = function() {
	if(localStorage.getItem("access_key") === null) {
		document.getElementById("navbar-login").style.display = "";
	}
	
	createModuleCards();
	
	wsOpen();
}

function wsOpen() {
	if (typeof ws === "undefined" || ws.readyState != WebSocket.CONNECTING) {
		if(location.host.length < 8) {
			return;
		}
		
		ws = new WebSocket("ws://" + location.host);
		
		secondaryNavbarChangeType("light");
		
		ws.onopen = function(evt) {
			testAccessKey();
			setInterval(testAccessKey, 5000);
			
			receiveTimeoutInterval = setTimeout(function(){ secondaryNavbarChangeType("warning"); }, 4000);
		};
		
		ws.onerror = function(evt) {
			console.error("WebSocket error:", evt);
			ws.close();
		};
		
		ws.onclose = function(evt) {
			clearTimeout(receiveTimeoutInterval);
			
			document.getElementById("navbar-login").style.display = "none";
			document.getElementById("navbar-logged").style.display = "none";
			
			if(configPageShown === true) {
				toggleConfigPage();
			}
			
			secondaryNavbarChangeType("error");
			
			addPageAlert("error", "Conexão perdida. Recarregue a página para tentar conectar novamente.")
			
			window.scrollTo(0, 0);
			
			setDashboardButtonsEnabledState(false);
		};
		
		ws.onmessage = function(evt) {
			var received = JSON.parse(evt.data);
			
			clearTimeout(receiveTimeoutInterval);
			secondaryNavbarChangeType("success");
			
			if(typeof received.new_key === "string") {
				logged = true;
				
				document.getElementById("navbar-login").style.display = "none";
				document.getElementById("navbar-logged").style.display = "";
				
				localStorage.setItem("access_key", received.new_key);
				
				setDashboardButtonsEnabledState(true);
			}
			
			if(typeof received.debug_message === "string") {
				console.log(received.debug_message);
			}
			
			if(typeof received.server_notification === "string") {
				handleServerNotification(received.server_notification, received.details);
			}
			
			if(typeof received.system_info === "object") {
				showSystemInfo(received.system_info);
			}
			
			if(typeof received.temperature === "number") {
				document.getElementById('system-temperature').innerHTML = Math.trunc(received.temperature) + ' °C';
			}
			
			if(typeof received.time === "number") {
				updateReceivedTime(received.time);
			}
			
			if(typeof received.module_info === "object") {
				populateModuleCard(received.module_info);
			}
			
			if(typeof received.module_data === "object") {
				updateModuleValues(received.module_data);
			}
			
			if(typeof received.config_forms === "object") {
				addConfigurationForms(received.config_forms.qty, received.config_forms.titles)
			}
			
			if(typeof received.config_info === "object") {
				addConfiguration(received.config_info);
			}
			
			if(typeof received.config_data === "object") {
				updateConfigurationValue(received.config_data.name, received.config_data.value);
			}
			
			if(typeof received.dashboard_lines === "object") {
				addDashboardLines(received.dashboard_lines.qty, received.dashboard_lines.titles);
			}
			
			if(typeof received.dashboard_info === "object") {
				addDashboardItem(received.dashboard_info);
			}
			
			if(typeof received.dashboard_parameter === "object") {
				dashboardItemUpdateParameters(received.dashboard_parameter.number, received.dashboard_parameter.parameters);
			}
			
			receiveTimeoutInterval = setTimeout(function(){ secondaryNavbarChangeType("warning"); }, 5000);
		};
	}
}

function addPageAlert(type, text) {
	let newalert = document.createElement("div");
	
	newalert.innerHTML = text;
	
	newalert.className = "siimple-alert siimple-alert--" + type;
	
	newalert.addEventListener("click", function() {this.remove()}, false);
	
	document.getElementById('alert-inner-container').appendChild(newalert);
}

function secondaryNavbarChangeType(type) {
	let navbarElement = document.getElementById('secondary-navbar');
	
	navbarElement.classList.remove("siimple-navbar--primary", "siimple-navbar--success", "siimple-navbar--warning", "siimple-navbar--error", "siimple-navbar--light");
	navbarElement.classList.add("siimple-navbar--" + type);
}

function toggleConfigPage() {
	
	if(configPageShown === true) {
		configPageShown = false;
		document.getElementById("configuration-page").style.display = "none";
		document.getElementById("dashboard-page").style.display = "";
	} else {
		configPageShown = true;
		document.getElementById("configuration-page").style.display = "";
		document.getElementById("dashboard-page").style.display = "none";
		
		if(configPageDone === false) {
			showLoadingModal(-1, "Carregando configurações...");
			requestConfigInfo();
		}
	}
}

function inputCallOnReturn(event, fn) {
	if (event.keyCode == 13) {
		fn();
	}
}

function login() {
	var password_field = document.getElementById("login-input");
	
	if(ws.readyState != WebSocket.OPEN) {
		return;
	}
	
	ws.send(JSON.stringify({"op":"login","password":password_field.value}));
}

function logout() {
	var key = localStorage.getItem("access_key");
	
	if(ws.readyState != WebSocket.OPEN) {
		return;
	}
	
	ws.send(JSON.stringify({"op":"logout", "key":key}));
}

function showLoadingModal(showForSecs, text) {
	if(typeof text === "string") {
		document.getElementById("loading-modal-text").innerText = text;
	}
	
	document.getElementById("loading-modal").style.display = "";
	
	if(typeof showForSecs === "number" && showForSecs > 0) {
		setTimeout(hideLoadingModal, showForSecs * 1000);
	}
}

function hideLoadingModal() {
	document.getElementById("loading-modal").style.display = "none";
}

function testAccessKey() {
	var key = localStorage.getItem("access_key");
	
	if(key === null) {
		return;
	}
	
	if(typeof ws === "undefined" || ws.readyState != WebSocket.OPEN) {
		return;
	}
	
	ws.send(JSON.stringify({"op":"test-key","key":key}));
}

function handleServerNotification(notification, details) {
	switch(notification) {
		case "diagnostic_mode":
			document.getElementById("regular-dashboard").style.display = "none";
			document.getElementById("diagnostic-dashboard").style.display = "";
			break;
			
		case "loading_done":
			hideLoadingModal();
			break;
			
		case "config_info_done":
			configPageDone = true;
			hideLoadingModal();
			break;
			
		case "key_ok":
			if(logged === false) {
				logged = true;
				
				document.getElementById("navbar-login").style.display = "none";
				document.getElementById("navbar-logged").style.display = "";
				
				setDashboardButtonsEnabledState(true);
			}
			
			break;
			
		case "invalid_key":
			logged = false;
			
			localStorage.removeItem("access_key");
			
			document.getElementById("navbar-login").style.display = "";
			document.getElementById("navbar-logged").style.display = "none";
			
			if(configPageShown === true) {
				toggleConfigPage();
			}
			
			setDashboardButtonsEnabledState(false);
			
			break;
			
		case "wrong_password":
			document.getElementById("login-input").value = "";
			addPageAlert("warning", "Senha incorreta!");
			break;
			
		case "ota_failed":
			addPageAlert("error", "A atualização falhou. Erro: " + details);
			hideLoadingModal();
			break;
			
		case "ota_done":
			addPageAlert("success", "Atualização concluída com sucesso!");
			hideLoadingModal();
			break;
			
		case "restart":
			showLoadingModal(-1, "Reiniciando o sistema...");
			setTimeout(document.location.reload.bind(window.location, false), 6000);
			break;
			
		default:
			console.log("Received unkown server notification: " + notification);
			
			break;
	}
}

function updateReceivedTime(received_time) {
	var moduleDate = new Date(received_time * 1000);
	var localDate = new Date();
	var timeElement = document.getElementById('system-time');
	
	if(received_time == 0) {
		timeElement.innerHTML = "--/--/---- --:--:--";
		timeElement.style.color = "red";
		timeElement.title = "O relógio do sistema está parado.";
		
		if(timeAlertShown == false) {
			addPageAlert("error", "O relógio interno parou, pode ser necessário fazer a substituição da bateria (CR2032) no módulo central. Se a bateria já foi substituída, faça login e então atualize o relógio.");
			
			timeAlertShown = true;
		}
	} else {
		timeElement.innerHTML = moduleDate.getDate().toString().padStart(2, "0") + "/" + (moduleDate.getMonth() + 1).toString().padStart(2, "0") + "/" + moduleDate.getFullYear() + " " + moduleDate.getHours().toString().padStart(2, "0") + ":" + moduleDate.getMinutes().toString().padStart(2, "0") + ":" + moduleDate.getSeconds().toString().padStart(2, "0");
		
		if(Math.abs(localDate.getTime() - moduleDate.getTime()) > 30 * 1000) {
			timeElement.style.color = "orange";
			timeElement.title = "O relógio do sistema pode estar incorreto.";
		} else {
			timeElement.style.color = "";
		}
	}
}

function addConfigurationForms(qty, titles) {
	var config_page = document.getElementById("configuration-page");
	
	if(typeof qty !== "number") {
		return;
	}
	
	for(let form_n = 0; form_n < qty; form_n++) {
		let form_element = document.getElementById("configuration-page-form" + form_n);
		
		if(form_element === null) {
			form_element = document.createElement("div");
			
			form_element.id = "configuration-page-form" + form_n;
			form_element.className = "siimple-form";
			
			config_page.appendChild(form_element);
		}
		
		if(typeof titles[form_n] !== "undefined" && titles[form_n] !== "") {
			let new_form_title = document.createElement("div");
			
			new_form_title.className = "siimple-form-title";
			
			new_form_title.innerText = titles[form_n];
			
			form_element.appendChild(new_form_title);
		}
	}
}

function addConfiguration(config_info) {
	var input_element = document.getElementById("configuration-input-" + config_info.name);
	
	if(input_element === null) {
		let form_element = document.getElementById("configuration-page-form" + config_info.formn);
		
		let new_label = document.createElement("div");
		let new_field = document.createElement("div");
		
		input_element = document.createElement("input");
		
		input_element.id = "configuration-input-" + config_info.name;
		
		new_field.className = "siimple-field";
		new_label.className = "siimple-field-label";
		
		new_label.innerText = config_info.dname;
		
		new_field.appendChild(new_label);
		
		input_element.className = "configuration-input";
		
		if(config_info.type === "B") {
			let switch_div = document.createElement("div");
			let switch_label = document.createElement("label");
			
			input_element.type = "checkbox";
			
			input_element.onchange = function() { this.value = (this.checked ? 1 : 0); }
			
			switch_label.htmlFor = input_element.id;
			
			switch_div.className = "siimple-switch";
			
			switch_div.appendChild(input_element);
			switch_div.appendChild(switch_label);
			
			new_field.appendChild(switch_div);
		} else {
			let new_helper = document.createElement("div");
			
			input_element.type = "text";
			
			input_element.classList.add("siimple-input");
			input_element.classList.add("siimple-input--fluid");
			
			new_helper.classList.add("siimple-field-helper");
			
			switch(config_info.type) {
				case "L":
					new_helper.innerText = "Lista de valores inteiros entre " + Math.trunc(config_info.min) + " e " + Math.trunc(config_info.max) + " (separados por vírgula)";
					break;
				case "T":
					new_helper.innerText = "Texto com tamanho entre " + Math.trunc(config_info.min) + " e " + Math.trunc(config_info.max) + " caracteres";
					break;
				case "I":
					new_helper.innerText = "Valor numérico inteiro entre " + Math.trunc(config_info.min) + " e " + Math.trunc(config_info.max);
					break;
				case "F":
					new_helper.innerText = "Valor numérico entre " + config_info.min + " e " + config_info.max;
					break;
				default:
					break;
			}
			
			new_field.appendChild(input_element);
			new_field.appendChild(new_helper);
		}
		
		if(form_element === null) {
			form_element = document.getElementById("configuration-page-form0");
		}
		
		form_element.appendChild(new_field);
	}
	
	if(config_info.type !== "B") {
		input_element.style.borderColor = "red";
		input_element.style.borderStyle = "solid";
		input_element.style.borderWidth = "0px";
	}
	
	input_element.dataset.config_name = config_info.name;
	input_element.dataset.config_type = config_info.type;
	input_element.dataset.config_min = config_info.min;
	input_element.dataset.config_max = config_info.max;
	input_element.dataset.config_req_restart = config_info.req_restart;
}

function updateConfigurationValue(name, value) {
	var input_element = document.getElementById("configuration-input-" + name);
	
	if(input_element !== null) {
		input_element.dataset.config_value = value;
		input_element.value = value;
		
		if(input_element.dataset.config_type == "B") {
			input_element.checked = (value === "1");
		}
	}
}

function createModuleCards() {
	var original = document.getElementById("siimple-card-base");
	
	for (let i = 0; i < 32; ++i) {
		let cloned = original.cloneNode(true);
		let new_list_element = document.createElement("div");
		
		cloned.id = "diagnostic-mode-card-" + i;
		cloned.className = "siimple-grid-col siimple-grid-col--4 siimple-grid-col--lg-6 siimple-grid-col--sm-12";
		cloned.style.display = "none";
		
		new_list_element.className = "siimple-list siimple--mb-0";
		
		cloned.getElementsByClassName("siimple-card-body")[0].appendChild(new_list_element);
		
		document.getElementById("diagnostic-mode-card-row").appendChild(cloned);
	}
}

function populateModuleCard(module_info) {
	var module_card = document.getElementById('diagnostic-mode-card-' + module_info.address);
	
	document.getElementById('diagnostic-mode-no-module-warning').style.display = "none";
	
	module_card.style.display = "";
	module_card.dataset.modulename = module_info.name;
	
	module_card.getElementsByClassName("siimple-card-header")[0].innerHTML = module_info.address + " | " + module_info.name;
	
	var module_card_channel_list = module_card.getElementsByClassName("siimple-list")[0];
	
	while (module_card_channel_list.lastChild) {
		module_card_channel_list.removeChild(module_card_channel_list.lastChild);
	}
	
	for(let c_n = 0; c_n < module_info.channels.length; c_n++) {
		for(let v_n = 0; v_n < module_info.channels[c_n].port_qty; v_n++) {
			let list_item = document.createElement("div");
			
			list_item.className = "siimple-list-item";
			list_item.id = "port_m" + module_info.address + "_c" + c_n + "_v" + v_n;
			list_item.innerHTML = module_info.channels[c_n].name + ((module_info.channels[c_n].port_qty > 1) ? v_n : "");
			
			let list_item_value = document.createElement("span");
			
			list_item_value.id = "portvalue_m" + module_info.address + "_c" + c_n + "_v" + v_n;
			list_item_value.className = "siimple-tag siimple-tag--rounded ";
			
			list_item_value.dataset.address = module_info.address;
			list_item_value.dataset.channel = c_n;
			list_item_value.dataset.port = v_n;
			list_item_value.dataset.vtype = module_info.channels[c_n].type;
			list_item_value.dataset.vmin = module_info.channels[c_n].min;
			list_item_value.dataset.vmax = module_info.channels[c_n].max;
			
			if(module_info.channels[c_n].writable == "Y") {
				list_item_value.className += "siimple-tag--primary";
				list_item_value.style.cursor = "pointer";
				list_item_value.addEventListener("click", function() {changeChannelValue(this);}, false);
			} else {
				list_item_value.className += "siimple-tag--light";
			}
			
			list_item.appendChild(list_item_value);
			
			module_card_channel_list.appendChild(list_item);
		}
	}
}

function updateModuleValues(module_data_array) {
	if(typeof module_data_array.length === "undefined") {
		return;
	}
	
	for(let m_n = 0; m_n < module_data_array.length; m_n++) {
		for(let c_n = 0; c_n < module_data_array[m_n].values.length; c_n++) {
			for(let v_n = 0; v_n < module_data_array[m_n].values[c_n].length; v_n++) {
				
				let receivedValue = module_data_array[m_n].values[c_n][v_n];
				let valueElement = document.getElementById("portvalue_m" + module_data_array[m_n].address + "_c" + c_n + "_v" + v_n);
				
				if(valueElement !== null && valueElement.innerHTML !== receivedValue) {
					valueElement.innerHTML = receivedValue;
				}
			}
		}
	}
}

function addDashboardLines(qty, titles) {
	var dashboard_div = document.getElementById("regular-dashboard");
	
	if(typeof qty !== "number") {
		return;
	}
	
	for(let line_n = 0; line_n < qty; line_n++) {
		if(document.getElementById("regular-dashboard-line" + line_n) === null) {
			let new_line = document.createElement("div");
			
			new_line.id = "regular-dashboard-line" + line_n;
			new_line.className = "siimple-grid-row";
			
			if(typeof titles[line_n] !== "undefined" && titles[line_n] !== "") {
				let new_line_title = document.createElement("div");
				let new_line_rule = document.createElement("div");
				
				new_line_title.className = "siimple-h5";
				new_line_rule.className = "siimple-rule";
				
				new_line_title.innerText = titles[line_n];
				
				dashboard_div.appendChild(new_line_title);
				dashboard_div.appendChild(new_line_rule);
			}
			
			dashboard_div.appendChild(new_line);
		}
	}
}

function addDashboardItem(item_info) {
	var line_element = document.getElementById("regular-dashboard-line" + item_info.line);
	
	var item_element;
	var item_card_body;
	
	var valid_widths = [1, 2, 3, 4, 6, 12];
	
	var ditem_width = item_info.width;
	
	if(line_element === null) {
		line_element = document.getElementById("regular-dashboard-line0");
	}
	
	if(valid_widths.includes(ditem_width[0]) === false || valid_widths.includes(ditem_width[1]) === false || valid_widths.includes(ditem_width[2]) === false) {
		return;
	}
	
	if(item_info.type === "button") {
		item_element = document.createElement("div");
	} else {
		item_element = document.getElementById("siimple-card-base").cloneNode(true);
		
		item_element.getElementsByClassName("siimple-card-header")[0].innerText = item_info.dname;
		
		item_card_body = item_element.getElementsByClassName("siimple-card-body")[0];
	}
	
	item_element.id = "regular-dashboard-item" + item_info.number;
	
	item_element.classList.add("siimple-grid-col");
	item_element.classList.add("siimple-grid-col--" + ditem_width[0]);
	item_element.classList.add("siimple-grid-col--lg-" + ditem_width[1]);
	item_element.classList.add("siimple-grid-col--sm-" + ditem_width[2]);
	
	item_element.dataset.dashboard_item_type = item_info.type;
	
	if(item_info.type === "vertical_gauge") {
		let new_vgauge_outer = document.createElement("div");
		let new_vgauge_inner = document.createElement("div");
		let new_text = document.createElement("div");
		
		new_vgauge_outer.classList.add("dashboard-vertical-gauge-outer");
		new_vgauge_inner.classList.add("dashboard-vertical-gauge-inner");
		
		new_text.classList.add("dashboard-vertical-gauge-text");
		new_text.classList.add("siimple--text-center");
		
		new_vgauge_outer.appendChild(new_vgauge_inner);
		
		item_card_body.appendChild(new_vgauge_outer);
		item_card_body.appendChild(new_text);
		
	} else if(item_info.type === "text") {
		let new_text = document.createElement("div");
		
		new_text.classList.add("dashboard-text");
		new_text.classList.add("siimple--text-center");
		new_text.classList.add("siimple-h4");
		
		item_card_body.appendChild(new_text);
		
	} else if(item_info.type === "button") {
		let new_button = document.createElement("div");
		
		new_button.classList.add("dashboard-button");
		new_button.classList.add("siimple-btn");
		new_button.classList.add("siimple-btn--fluid");
		new_button.classList.add("siimple-btn--primary");
		
		if(logged === false) {
			new_button.classList.add("siimple-btn--disabled");
		}
		
		new_button.innerText = item_info.dname;
		
		new_button.addEventListener("click", function() {dashboardButtonCommand(this);}, false);
		
		item_element.appendChild(new_button);
	}
	
	line_element.appendChild(item_element);
	
	dashboardItemUpdateParameters(item_info.number, item_info.parameters);
}

function dashboardItemUpdateParameters(itemNumber, parameters) {
	let item_element = document.getElementById("regular-dashboard-item" + itemNumber);
		
	if(item_element === null) {
		return;
	}
	
	switch(item_element.dataset.dashboard_item_type) {
		case "vertical_gauge":
			if(typeof parameters[0] === "number" && parameters[0] <= 100.0 && parameters[0] >= 0.0) {
				item_element.getElementsByClassName("dashboard-vertical-gauge-inner")[0].style.height = parameters[0] + "%";
			}
			
			if(typeof parameters[1] === "string" && parameters[1] !== "") {
				item_element.getElementsByClassName("dashboard-vertical-gauge-text")[0].innerText = parameters[1];
			}
			
			break;
		case "text":
			if(typeof parameters[0] === "string" && parameters[0] !== "") {
				item_element.getElementsByClassName("dashboard-text")[0].innerText = parameters[0];
			}
			
			if(typeof parameters[1] === "string" && parameters[1] !== "") {
				item_element.getElementsByClassName("dashboard-text")[0].style.color = parameters[1];
			}
			
			break;
		case "button":
			if(typeof parameters[0] === "string" && parameters[0] !== "") {
				item_element.getElementsByClassName("dashboard-button")[0].dataset.button_command = parameters[0];
			}
			
			break;
		
		default:
			break;
	}
}

function changeChannelValue(valueElement) {
	var key = localStorage.getItem("access_key");
	var valueType = valueElement.dataset.vtype;
	var valueMin = valueElement.dataset.vmin;
	var valueMax = valueElement.dataset.vmax;
	var actualValue = valueElement.innerText;
	var newValue;
	var promptText;
	var parameterString;
	
	if(key === null) {
		return;
	}
	
	if(valueType === "B") {
		newValue = (actualValue === "0") ? "1" : "0";
		
	} else {
		if(valueType === "I") {
			promptText = "Valor numérico inteiro entre " + Math.trunc(valueMin) + " e " + Math.trunc(valueMax);
		} else if(valueType === "F") {
			promptText = "Valor numérico entre " + valueMin + " e " + valueMax;
		} else if(valueType === "T") {
			promptText = "Texto com até " + Math.trunc(valueMax) + " caracteres";
		}
		
		newValue = window.prompt(promptText, actualValue);
		
		if(newValue === null || newValue === "") {
			return;
		}
	}
	
	parameterString = "MWR:" + valueElement.dataset.address + ":" + valueElement.dataset.channel + ":" + valueElement.dataset.port + ":" + newValue;
	
	ws.send(JSON.stringify({"key":key,"op":"action","parameters":parameterString}));

}

function setDashboardButtonsEnabledState(state) {
	var buttonArray = document.getElementsByClassName("dashboard-button");
	
	for(let b_n = 0; b_n < buttonArray.length; b_n++) {
		if(state === true) {
			buttonArray[b_n].classList.remove("siimple-btn--disabled");
		} else {
			buttonArray[b_n].classList.add("siimple-btn--disabled");
		}
	}
}

function dashboardButtonCommand(button_element) {
	var key = localStorage.getItem("access_key");
	var parameterString;
	
	if(key === null || logged === false) {
		return;
	}
	
	if(typeof button_element.dataset.button_command === "undefined") {
		return
	}
	
	parameterString = "DBTN:" + button_element.dataset.button_command;
	
	ws.send(JSON.stringify({"key":key,"op":"action","parameters":parameterString}));
	
	showLoadingModal(2, "Enviando comando...");
}

function updateSystemTime() {
	var key = localStorage.getItem("access_key");
	var parameterString;
	
	if(key === null) {
		return;
	}
	
	if(confirm("Você vai ajustar o relógio do sistema para o horário atual do seu dispositivo. Continuar?") == false) {
		return;
	}
	
	var dateobj = new Date();
	
	parameterString = "TUP:" + dateobj.getTime() / 1000;
	
	ws.send(JSON.stringify({"key":key,"op":"action","parameters":parameterString}));
}

function restartSystem() {
	var key = localStorage.getItem("access_key");
	
	if(key === null) {
		return;
	}
	
	if(confirm("Você vai reiniciar o sistema. Continuar?") == false) {
		return;
	}
	
	ws.send(JSON.stringify({"key":key,"op":"action","parameters":"RST:"}));
}

function firmwareUpdate() {
	var key = localStorage.getItem("access_key");
	var fileHash;
	var urlText;
	var url;
	var parameterString;
	
	if(key === null) {
		return;
	}
	
	urlText = window.prompt("Informe a URL do arquivo de atualização.");
	
	if(urlText === null || urlText === "") {
		return;
	}
	
	url = new URL(urlText);
	
	if(typeof url === "undefined" || url.protocol === "" || url.hostname === "" || url.pathname === "" || url.hash === "") {
		addPageAlert("error", "URL inválida.");
		return;
	}
	
	if(url.protocol === "https:") {
		addPageAlert("error", "HTTPS não é suportado.");
		return;
	}
	
	parameterString = "OTA:" + url.hostname + ":" + ((url.port === "") ? "80" : url.port) + ":" + url.pathname + ":" + url.hash.replace("#", "");
	
	if(parameterString.length > 149) {
		addPageAlert("error", "URL muito longa.");
		return;
	}
	
	ws.send(JSON.stringify({"key":key,"op":"action","parameters":parameterString}));
	
	clearTimeout(receiveTimeoutInterval);
	
	showLoadingModal(-1, "Atualizando o firmware...");
}

function requestSystemInfo() {
	var key = localStorage.getItem("access_key");
	
	if(key === null) {
		return;
	}
	
	ws.send(JSON.stringify({"key":key,"op":"action","parameters":"SYSI:"}));
}

function closeSysInfoModal() {
	document.getElementById("sysinfo-modal").style.display = "none";
}

function showSystemInfo(system_info) {
	var uptimeDays = Math.trunc(system_info.uptime / 86400.0);
	var uptimeHours = Math.trunc((system_info.uptime % 86400.0) / 3600.0);
	var uptimeMinutes = Math.trunc(((system_info.uptime % 86400.0) % 3600.0) / 60.0);
	var uptimeSeconds = system_info.uptime % 60.0;
	var uptimeText = uptimeDays + " dias, " + uptimeHours.toString().padStart(2, "0") + ":" + uptimeMinutes.toString().padStart(2, "0") + ":" + uptimeSeconds.toString().padStart(2, "0");
	
	document.getElementById("sysinfo-fw-ver").innerText = system_info.fw_ver + " / " + system_info.cc_ver;
	document.getElementById("sysinfo-free-heap").innerText = system_info.free_heap + " bytes";
	document.getElementById("sysinfo-uptime").innerText = uptimeText;
	
	document.getElementById("sysinfo-cyd-http").innerText = system_info.cycle_duration[0] + " ms";
	document.getElementById("sysinfo-cyd-mm").innerText = system_info.cycle_duration[1] + " ms";
	document.getElementById("sysinfo-cyd-cc").innerText = system_info.cycle_duration[2] + " ms";
	document.getElementById("sysinfo-cyd-mqtt").innerText = system_info.cycle_duration[3] + " ms";
	
	document.getElementById("sysinfo-shwm-http").innerText = system_info.task_shwm[0] + " words";
	document.getElementById("sysinfo-shwm-mm").innerText = system_info.task_shwm[1] + " words";
	document.getElementById("sysinfo-shwm-cc").innerText = system_info.task_shwm[2] + " words";
	document.getElementById("sysinfo-shwm-mqtt").innerText = system_info.task_shwm[3] + " words";
	
	document.getElementById("sysinfo-modal").style.display = "";
	
	console.info(JSON.stringify(system_info));
}

function requestConfigInfo() {
	var key = localStorage.getItem("access_key");
	
	if(key === null) {
		return;
	}
	
	ws.send(JSON.stringify({"key":key,"op":"action","parameters":"CFGI:"}));
}

function checkConfigIntList(value, min, max) {
	if(typeof value !== "string") {
		return false;
	}
	
	var listArray = value.split(",");
	
	for(let i = 0; i < listArray.length; i++) {
		let asNumber = parseFloat(listArray[i]);
		
		if(asNumber != listArray[i] || asNumber > max || asNumber < min) {
			return false;
		}
	}
	
	return true;
}

function checkConfigNumber(value, min, max) {
	if(typeof value !== "string") {
		return false;
	}
	
	let asNumber = parseFloat(value);
	
	if(asNumber != value || asNumber > max || asNumber < min) {
		return false;
	}
	
	return true;
}

function checkConfigText(value, min, max) {
	if(typeof value !== "string") {
		return false;
	}
	
	let textByteCount = (new Blob([value]).size);
	
	if(textByteCount > max || textByteCount < min) {
		return false;
	}
	
	return true;
}

function saveConfig() {
	var key = localStorage.getItem("access_key");
	var input_elements = document.getElementsByClassName("configuration-input");
	var invalidValues = 0;
	var sentValues = 0;
	var requireRestart = false;
	
	if(key === null) {
		return;
	}
	
	if(configPageDone === false) {
		return;
	}
	
	for(let i_n = 0; i_n < input_elements.length; i_n++) {
		let value = input_elements[i_n].value;
		let min = input_elements[i_n].dataset.config_min;
		let max = input_elements[i_n].dataset.config_max;
		let result = false;
		
		if(input_elements[i_n].dataset.config_value === value || value.length === 0) {
			continue;
		}
		
		switch(input_elements[i_n].dataset.config_type) {
			case "B":
				result = true;
				break;
			case "T":
				result = checkConfigText(value, min, max);
				break;
			case "L":
				result = checkConfigIntList(value, min, max);
				break;
			case "I":
			case "F":
				result = checkConfigNumber(value, min, max);
				break;
		}
		
		if(result === false) {
			invalidValues++;
			input_elements[i_n].style.borderWidth = "1px";
		}
	}
	
	if(invalidValues > 0) {
		addPageAlert("error", "Existem campos de configuração estão com valores invalidos. (" + invalidValues + ")");
		
		return;
	}
	
	for(let i_n = 0; i_n < input_elements.length; i_n++) {
		let parameterString;
		
		input_elements[i_n].style.borderWidth = "0px";
		
		if(input_elements[i_n].dataset.config_value === input_elements[i_n].value) {
			continue;
		}
		
		parameterString = "CFGW:" + input_elements[i_n].dataset.config_name + ":" + input_elements[i_n].value;
		
		ws.send(JSON.stringify({"key":key,"op":"action","parameters":parameterString}));
		
		sentValues++;
		
		if(input_elements[i_n].dataset.config_req_restart === "Y" || input_elements[i_n].value.length === 0) {
			requireRestart = true;
		}
	}
	
	if(sentValues > 0) {
		showLoadingModal(3, "Salvando configurações...");
		
		if(requireRestart === true) {
			addPageAlert("warning", "Algumas configurações modificadas só terão efeito depois que o sistema for reiniciado.");
		}
	}
}
