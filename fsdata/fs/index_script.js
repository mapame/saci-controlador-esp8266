var ws;
var configPageShown = false;

window.onload = function() {
	document.getElementById("configuration-page").style.display = "none";
	
	if(localStorage.getItem("access_key") === null) {
		document.getElementById("navbar-logged").style.display = "none";
	} else {
		document.getElementById("navbar-login").style.display = "none";
	}
	
	createRawModuleCards();
	
	if(location.host.length > 1) {
		wsOpen();
	
		setInterval(testAccessKey, 2000);
	}
}

function wsOpen() {
	if (typeof ws === "undefined" || ws.readyState != WebSocket.CONNECTING) {
		ws = new WebSocket("ws://" + location.host);
		
		setConnectionStatus("connecting");
		
		ws.onopen = function(evt) {
			setConnectionStatus("connected");
			
			if(localStorage.getItem("access_key") !== null) {
				testAccessKey();
			}
		};
		
		ws.onerror = function(evt) {
			setConnectionStatus("error");
			ws.close();
		};
		
		ws.onclose = function(evt) {
			setConnectionStatus("closed");
		};
		
		ws.onmessage = function(evt) {
			var received = JSON.parse(evt.data);
			//console.log(received);
			
			if(typeof received.key !== "undefined") {
				document.getElementById("navbar-login").style.display = "none";
				document.getElementById("navbar-logged").style.display = "";
				
				localStorage.setItem("access_key", received.key);
			}
			
			if(typeof received.debug_message !== "undefined") {
				console.log(received.debug_message);
			}
			
			if(typeof received.alert_message !== "undefined") {
				alert(received.alert_message);
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
			
			if(typeof received.free_heap !== "undefined") {
				document.getElementById('system-memory').innerHTML = (received.free_heap * 100 / 81920).toFixed(0) + ' %';
				document.getElementById('system-memory').title = received.free_heap + " bytes livres";
			}
			
			if(typeof received.temperature !== "undefined") {
				document.getElementById('temperature').innerHTML = received.temperature + ' °C';
			}
			
			if(typeof received.module_data !== "undefined") {
				let module_card = document.getElementById('diagnose-mode-card-' + received.module_data.address);
				
				document.getElementById('diagnose-mode-text').style.display = "none";
				
				module_card.style.display = "";
				
				module_card.getElementsByClassName("diagnose-mode-card-header")[0].innerHTML = received.module_data.address + " | " + received.module_data.name;
				
				let module_card_channel_list = module_card.getElementsByClassName("diagnose-mode-card-channel-list")[0];
				
				while (module_card_channel_list.lastChild) {
					module_card_channel_list.removeChild(module_card_channel_list.lastChild);
				}
				
				for(let c_n = 0; c_n < received.module_data.channels.length; c_n++) {
					for(let v_n = 0; v_n < received.module_data.channels[c_n].values.length; v_n++) {
						let list_item = document.createElement("div");
						let list_item_value = document.createElement("span");
						
						list_item.className = "siimple-list-item";
						/*list_item.id = "port_m" + received.module_data.address + "_c" + c_n + "_v" + v_n;*/
						list_item.innerHTML = received.module_data.channels[c_n].name + ((received.module_data.channels[c_n].values.length > 1) ? v_n : "");
						
						if(received.module_data.channels[c_n].writable)
						
						list_item_value.className = "siimple-tag siimple-tag--rounded ";
						
						if(received.module_data.channels[c_n].writable == "Y") {
							list_item_value.className += "siimple-tag--primary";
							list_item_value.style.cursor = "pointer";
							list_item_value.addEventListener("click", function() {changeChannelValue(received.module_data.channels[c_n].type, received.module_data.address, c_n, v_n);}, false);
						} else {
							list_item_value.className += "siimple-tag--light";
						}
						
						list_item_value.dataset.address = received.module_data.address;
						list_item_value.dataset.channel = c_n;
						list_item_value.dataset.port = v_n;
						
						list_item_value.dataset.value_type = received.module_data.channels[c_n].type;
						
						list_item_value.innerHTML = received.module_data.channels[c_n].values[v_n];
						
						list_item.appendChild(list_item_value);
						
						module_card_channel_list.appendChild(list_item);
					}
				}
			}
		};
	}
}

function setConnectionStatus(status) {
	var indicator = document.getElementById('connection-status');
	
	switch(status) {
		case "connecting":
			indicator.style.backgroundColor = "gray";
			indicator.title = "Conectando";
			break;
		case "connected":
			indicator.style.backgroundColor = "green";
			indicator.title = "Conectado";
			break;
		case "error":
			indicator.style.backgroundColor = "orange";
			indicator.title = "Erro";
			break;
		case "closed":
			indicator.style.backgroundColor = "red";
			indicator.title = "Desconectado";
			break;
		default:
			break;
	}
}

function addPageAlert(type, text) {
	let newalert = document.createElement("div");
	
	newalert.innerHTML = text;
	
	newalert.className = "siimple-alert siimple-alert--" + type;
	
	newalert.addEventListener("click", function() {this.remove()}, false);
	
	document.getElementById('alert-inner-container').appendChild(newalert);
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
	
	if(ws.readyState != 1) {
		return;
	}
	
	ws.send(JSON.stringify({"op":"login","password":password_field.value}));
}

function logout() {
	var key = localStorage.getItem("access_key");
	
	if(ws.readyState != 1) {
		return;
	}
	
	ws.send(JSON.stringify({"op":"logout", "key":key}));
}

function testAccessKey() {
	if(localStorage.getItem("access_key") === null) {
		return;
	}
	
	if(typeof ws === "undefined" || ws.readyState != WebSocket.OPEN) {
		return;
	}
	
	var key = localStorage.getItem("access_key");
	
	if(ws.readyState != 1) {
		return;
	}
	
	ws.send(JSON.stringify({"op":"nop","key":key}));
}

function createRawModuleCards() {
	var original = document.getElementById("diagnose-mode-card-base");
	
	for (let i = 0; i < 32; ++i) {
		let cloned = original.cloneNode(true);
		
		cloned.id = "diagnose-mode-card-" + i;
		cloned.style.display = "none";
		
		document.getElementById("diagnose-mode-card-row").appendChild(cloned);
	}
}

function changeChannelValue(vtype, address, channel, port) {
	let newValue = window.prompt("Informe um valor do tipo " + vtype);
	
	if(newValue !== null || newValue !== "") {
		
	}
}
