var ws;
var receiveTimeoutInterval;
var configPageShown = false;
var timeAlertShown = false;

window.onload = function() {
	document.getElementById("configuration-page").style.display = "none";
	
	if(localStorage.getItem("access_key") === null) {
		document.getElementById("navbar-logged").style.display = "none";
	} else {
		document.getElementById("navbar-login").style.display = "none";
	}
	
	createModuleCards();
	
	if(location.host.length > 1) {
		wsOpen();
	
		setInterval(testAccessKey, 5000);
	}
}

function wsOpen() {
	if (typeof ws === "undefined" || ws.readyState != WebSocket.CONNECTING) {
		ws = new WebSocket("ws://" + location.host);
		
		secondaryNavbarChangeType("light");
		
		ws.onopen = function(evt) {
			if(localStorage.getItem("access_key") !== null) {
				testAccessKey();
			}
			
			receiveTimeoutInterval = setTimeout(function(){ secondaryNavbarChangeType("warning"); }, 4000);
		};
		
		ws.onerror = function(evt) {
			console.error("WebSocket error:", evt);
			ws.close();
		};
		
		ws.onclose = function(evt) {
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
			}
			
			if(typeof received.debug_message !== "undefined") {
				console.log(received.debug_message);
			}
			
			if(typeof received.page_alert !== "undefined") {
				addPageAlert(received.page_alert.type, received.page_alert.message);
			}
			
			if(typeof received.server_notification !== "undefined") {
				switch(received.server_notification) {
					case "restart":
						addPageAlert("warning", "O controlador foi reiniciado, atualizando a página em breve.");
						setTimeout(function(){ document.location.reload(false); }, 3000);
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
						document.getElementById("navbar-login").style.display = "";
						document.getElementById("navbar-logged").style.display = "none";
						
						localStorage.removeItem("access_key");
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
				
				document.getElementById('system-time').innerHTML = moduleDate.getDate().toString().padStart(2, "0") + "/" + (moduleDate.getMonth() + 1).toString().padStart(2, "0") + "/" + moduleDate.getFullYear() + " " + moduleDate.getHours().toString().padStart(2, "0") + ":" + moduleDate.getMinutes().toString().padStart(2, "0") + ":" + moduleDate.getSeconds().toString().padStart(2, "0");
				
				if(received.time == 0) {
					timeElement.style.color = "red";
					timeElement.title = "O relógio do sistema parou.";
					
					if(timeAlertShown == false) {
						addPageAlert("error", "O relógio interno parou, pode ser necessário fazer a substituição da bateria (CR2032) no módulo central. Se a bateria já foi substituída, faça login e então atualize o relógio.");
						
						timeAlertShown = true;
					}
				} else if(Math.abs(localDate.getTime() - moduleDate.getTime()) > 30 * 1000) {
					timeElement.style.color = "orange";
					timeElement.title = "O relógio do sistema não corresponde com o horário do seu dispositivo.";
				} else {
					timeElement.style.color = "";
				}
			}
			
			if(typeof received.module_info !== "undefined") {
				let module_card = document.getElementById('diagnose-mode-card-' + received.module_info.address);
				
				document.getElementById('diagnose-mode-no-module-warning').style.display = "none";
				
				module_card.style.display = "";
				module_card.dataset.modulename = received.module_info.name;
				
				module_card.getElementsByClassName("diagnose-mode-card-header")[0].innerHTML = received.module_info.address + " | " + received.module_info.name;
				
				let module_card_channel_list = module_card.getElementsByClassName("diagnose-mode-card-channel-list")[0];
				
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
			
			if(typeof received.module_data !== "undefined") {
				for(let c_n = 0; c_n < received.module_data.values.length; c_n++) {
					for(let v_n = 0; v_n < received.module_data.values[c_n].length; v_n++) {
						let receivedValue = received.module_data.values[c_n][v_n];
						let valueElement = document.getElementById("portvalue_m" + received.module_data.address + "_c" + c_n + "_v" + v_n);
						
						if(valueElement !== null && valueElement.innerHTML !== receivedValue) {
							valueElement.innerHTML = receivedValue;
						}
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
	
	ws.send(JSON.stringify({"op":"nop","key":key}));
}

function createModuleCards() {
	var original = document.getElementById("diagnose-mode-card-base");
	
	for (let i = 0; i < 32; ++i) {
		let cloned = original.cloneNode(true);
		
		cloned.id = "diagnose-mode-card-" + i;
		cloned.style.display = "none";
		
		document.getElementById("diagnose-mode-card-row").appendChild(cloned);
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
		
		if(newValue === null && newValue === "") {
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
	
	ws.send(JSON.stringify({"key":key,"op":"client-action","parameters":"RST"}));
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

function showAdvancedSystemStats() {
	var key = localStorage.getItem("access_key");
	
	if(key === null) {
		return;
	}
	
	ws.send(JSON.stringify({"key":key,"op":"client-action","parameters":"SYS"}));
}
