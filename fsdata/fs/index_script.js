var ws;
var receiveTimeoutInterval;
var configInfoRequested = false;
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
			
			window.scrollTo(0, 0);
		};
		
		ws.onmessage = function(evt) {
			var received = JSON.parse(evt.data);
			
			clearTimeout(receiveTimeoutInterval);
			secondaryNavbarChangeType("success");
			
			if(typeof received.key !== "undefined") {
				document.getElementById("navbar-login").style.display = "none";
				document.getElementById("navbar-logged").style.display = "";
				
				localStorage.setItem("access_key", received.key);
				
				requestConfigInfo();
			}
			
			if(typeof received.debug_message !== "undefined") {
				console.log(received.debug_message);
			}
			
			if(typeof received.page_alert !== "undefined") {
				addPageAlert(received.page_alert.type, received.page_alert.message);
			}
			
			if(typeof received.server_notification !== "undefined") {
				switch(received.server_notification) {
					case "diagnostic_mode":
						document.getElementById("regular-dashboard").style.display = "none";
						document.getElementById("diagnostic-dashboard").style.display = "";
						break;
						
					case "loading_done":
						document.getElementById("loading-modal").style.display = "none";
						break;
						
					case "config_info_done":
						configPageDone = true;
						document.getElementById("loading-modal").style.display = "none";
						break;
						
					case "key_ok":
						document.getElementById("navbar-login").style.display = "none";
						document.getElementById("navbar-logged").style.display = "";
						
						requestConfigInfo();
						
						break;
					
					case "restart":
						addPageAlert("warning", "O controlador foi reiniciado, atualizando a página automaticamente em 5 segundos...");
						setTimeout(function(){ document.location.reload(false); }, 5000);
						break;
						
					default:
						console.log("Received unkown server notification: " + received.server_notification);
						
						break;
				}
			}
			
			if(typeof received.error !== "undefined") {
				switch(received.error) {
					case "wrong_password":
						document.getElementById("login-input").value = "";
						addPageAlert("warning", "Senha incorreta!");
						break;
						
					case "invalid_key":
						localStorage.removeItem("access_key");
						
						document.getElementById("navbar-login").style.display = "";
						document.getElementById("navbar-logged").style.display = "none";
						
						if(configPageShown === true) {
							toggleConfigPage();
						}
						
						break;
						
					default:
						break;
				}
			}
			
			if(typeof received.adv_system_status !== "undefined") {
				let statusText;
				
				statusText = "Versão do firmware: " + received.adv_system_status.fw_ver;
				statusText += "\nMemória livre: " + received.adv_system_status.free_mem + "bytes";
				statusText += "\nCiclo HTTP: " + received.adv_system_status.http_cycle_duration + " ms";
				statusText += "\nCiclo MM: " + received.adv_system_status.mm_cycle_duration + " ms";
				
				setTimeout(function(){ alert(statusText); }, 10);
			}
			
			if(typeof received.uptime !== "undefined") {
				let uptimeDays = Math.trunc(received.uptime / 86400.0);
				let uptimeHours = Math.trunc((received.uptime % 86400.0) / 3600.0);
				let uptimeMinutes = Math.trunc(((received.uptime % 86400.0) % 3600.0) / 60.0);
				let uptimeSeconds = received.uptime % 60.0;
				
				document.getElementById('page-title').title = "Uptime: " + uptimeDays + " dias, " + uptimeHours.toString().padStart(2, "0") + ":" + uptimeMinutes.toString().padStart(2, "0") + ":" + uptimeSeconds.toString().padStart(2, "0");
			}
			
			if(typeof received.temperature !== "undefined") {
				document.getElementById('system-temperature').innerHTML = Math.trunc(received.temperature) + ' °C';
				document.getElementById('system-temperature').title = received.temperature + ' °C';
			}
			
			if(typeof received.time !== "undefined") {
				let moduleDate = new Date(received.time * 1000);
				let localDate = new Date();
				let timeElement = document.getElementById('system-time');
				
				if(received.time == 0) {
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
			
			if(typeof received.module_info !== "undefined") {
				let module_card = document.getElementById('diagnostic-mode-card-' + received.module_info.address);
				
				document.getElementById('diagnostic-mode-no-module-warning').style.display = "none";
				
				module_card.style.display = "";
				module_card.dataset.modulename = received.module_info.name;
				
				module_card.getElementsByClassName("diagnostic-mode-card-header")[0].innerHTML = received.module_info.address + " | " + received.module_info.name;
				
				let module_card_channel_list = module_card.getElementsByClassName("diagnostic-mode-card-channel-list")[0];
				
				while (module_card_channel_list.lastChild) {
					module_card_channel_list.removeChild(module_card_channel_list.lastChild);
				}
				
				for(let c_n = 0; c_n < received.module_info.channels.length; c_n++) {
					for(let v_n = 0; v_n < received.module_info.channels[c_n].port_qty; v_n++) {
						let list_item = document.createElement("div");
						
						list_item.className = "siimple-list-item";
						list_item.id = "port_m" + received.module_info.address + "_c" + c_n + "_v" + v_n;
						list_item.innerHTML = received.module_info.channels[c_n].name + ((received.module_info.channels[c_n].port_qty > 1) ? v_n : "");
						
						let list_item_value = document.createElement("span");
						
						list_item_value.id = "portvalue_m" + received.module_info.address + "_c" + c_n + "_v" + v_n;
						list_item_value.className = "siimple-tag siimple-tag--rounded ";
						
						list_item_value.dataset.address = received.module_info.address;
						list_item_value.dataset.channel = c_n;
						list_item_value.dataset.port = v_n;
						list_item_value.dataset.vtype = received.module_info.channels[c_n].type;
						list_item_value.dataset.vmin = received.module_info.channels[c_n].min;
						list_item_value.dataset.vmax = received.module_info.channels[c_n].max;
						
						if(received.module_info.channels[c_n].writable == "Y") {
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
			
			if(typeof received.module_data !== "undefined" && typeof received.module_data.length !== "undefined") {
				for(let m_n = 0; m_n < received.module_data.length; m_n++) {
					for(let c_n = 0; c_n < received.module_data[m_n].values.length; c_n++) {
						for(let v_n = 0; v_n < received.module_data[m_n].values[c_n].length; v_n++) {
							
							let receivedValue = received.module_data[m_n].values[c_n][v_n];
							let valueElement = document.getElementById("portvalue_m" + received.module_data[m_n].address + "_c" + c_n + "_v" + v_n);
							
							if(valueElement !== null && valueElement.innerHTML !== receivedValue) {
								valueElement.innerHTML = receivedValue;
							}
						}
					}
				}
			}
			
			if(typeof received.config_info !== "undefined") {
				let form = document.getElementById("custom-configuration-form");
				
				let input_element = document.getElementById("configuration_input_" + received.config_info.name);
				
				if(input_element === null) {
					let new_label = document.createElement("div");
					let new_field = document.createElement("div");
					
					input_element = document.createElement("input");
					
					input_element.id = "configuration_input_" + received.config_info.name;
					
					new_field.className = "siimple-field";
					new_label.className = "siimple-field-label";
					
					new_label.innerText = received.config_info.fname;
					
					new_field.appendChild(new_label);
					
					input_element.className = "configuration-input";
					
					if(received.config_info.type === "B") {
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
						input_element.type = "text";
						
						input_element.classList.add("siimple-input");
						input_element.classList.add("siimple-input--fluid");
						
						new_field.appendChild(input_element);
					}
					
					form.appendChild(new_field);
				}
				
				if(received.config_info.type !== "B") {
					input_element.style.borderColor = "red";
					input_element.style.borderStyle = "solid";
					input_element.style.borderWidth = "0px";
				}
				
				input_element.dataset.config_name = received.config_info.name;
				input_element.dataset.config_type = received.config_info.type;
				input_element.dataset.config_min = received.config_info.min;
				input_element.dataset.config_max = received.config_info.max;
				input_element.dataset.config_req_restart = received.config_info.req_restart;
			}
			
			if(typeof received.config_data !== "undefined") {
				let input_element = document.getElementById("configuration_input_" + received.config_data.name);
				
				if(input_element !== null) {
					input_element.dataset.config_value = received.config_data.value;
					input_element.value = received.config_data.value;
					
					if(input_element.dataset.config_type == "B") {
						input_element.checked = (received.config_data.value === "1");
					}
				}
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
			document.getElementById("loading-modal").style.display = "";
		}
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

function createModuleCards() {
	var original = document.getElementById("diagnostic-mode-card-base");
	
	for (let i = 0; i < 32; ++i) {
		let cloned = original.cloneNode(true);
		
		cloned.id = "diagnostic-mode-card-" + i;
		cloned.style.display = "none";
		
		document.getElementById("diagnostic-mode-card-row").appendChild(cloned);
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
	
	ws.send(JSON.stringify({"key":key,"op":"client-action","parameters":parameterString}));

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
	
	ws.send(JSON.stringify({"key":key,"op":"client-action","parameters":parameterString}));
}

function restartSystem() {
	var key = localStorage.getItem("access_key");
	
	if(key === null) {
		return;
	}
	
	if(confirm("Você vai reiniciar o sistema. Continuar?") == false) {
		return;
	}
	
	ws.send(JSON.stringify({"key":key,"op":"client-action","parameters":"RST:"}));
}

function firmwareUpdate() {
	var key = localStorage.getItem("access_key");
	var fileHash;
	var host;
	var parameterString;
	
	if(key === null) {
		return;
	}
	
	host = window.prompt("Informe o host para download do arquivo.");
	
	if(host === null && host === "") {
		addPageAlert("error", "Host invalido!");
		return;
	}
	
	parameterString = "OTA:" + host;
	
	ws.send(JSON.stringify({"key":key,"op":"client-action","parameters":parameterString}));
	
	clearTimeout(receiveTimeoutInterval);
}

function showSystemStats() {
	var key = localStorage.getItem("access_key");
	
	if(key === null) {
		return;
	}
	
	ws.send(JSON.stringify({"key":key,"op":"client-action","parameters":"SYSS:"}));
}

function requestConfigInfo() {
	var key = localStorage.getItem("access_key");
	
	if(key === null) {
		return;
	}
	
	if(configInfoRequested === true) {
		return;
	}
	
	configInfoRequested = true;
	
	ws.send(JSON.stringify({"key":key,"op":"client-action","parameters":"CFGI:"}));
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
		let valueType = input_elements[i_n].dataset.config_type;
		let valueAsNumber = parseFloat(input_elements[i_n].value);
		let valueToCompare;
		
		if(valueType === "B" || input_elements[i_n].dataset.config_value === input_elements[i_n].value) {
			continue;
		}
		
		valueToCompare = (valueType === "T") ? (new Blob([input_elements[i_n].value]).size) : input_elements[i_n].value;
		
		if(((valueType === "I" || valueType === "F") && (isNaN(valueAsNumber) === true || valueAsNumber != input_elements[i_n].value)) || valueToCompare > input_elements[i_n].dataset.config_max || valueToCompare < input_elements[i_n].dataset.config_min) {
			invalidValues++;
			
			input_elements[i_n].style.borderWidth = "1px";
		}
	}
	
	if(invalidValues > 0) {
		addPageAlert("error", "Existem campos de configuração estão com valores invalidos. (Quantidade: " + invalidValues + ")");
		
		return;
	}
	
	for(let i_n = 0; i_n < input_elements.length; i_n++) {
		let parameterString;
		
		input_elements[i_n].style.borderWidth = "0px";
		
		if(input_elements[i_n].dataset.config_value === input_elements[i_n].value) {
			continue;
		}
		
		parameterString = "CFGW:" + input_elements[i_n].dataset.config_name + ":" + input_elements[i_n].value;
		
		ws.send(JSON.stringify({"key":key,"op":"client-action","parameters":parameterString}));
		
		sentValues++;
		
		if(input_elements[i_n].dataset.config_req_restart === "Y") {
			requireRestart = true;
		}
	}
	
	if(sentValues > 0) {
		document.getElementById("loading-modal").style.display = "";
		setTimeout(function(){ document.getElementById("loading-modal").style.display = "none"; }, 3000);
		
		if(requireRestart === true) {
			addPageAlert("warning", "Algumas configurações modificadas só terão efeito depois que o sistema for reiniciado.");
		}
	}
}
