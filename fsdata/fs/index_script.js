var ws;

window.onload = function() {
	document.getElementById("configpage-button").style.display = "none";
	
	document.getElementById("login-button").addEventListener("click", function () {
		document.getElementById("navbar-login").style.display = "none";
		document.getElementById("configpage-button").style.display = "";
	});
	
	createRawModuleCards();
	
	wsOpen();
}

function setMsg(cls, text) {
	sbox = document.getElementById('mainAlert');
	sbox.className = "siimple-alert siimple-alert--" + cls;
	sbox.innerHTML = text;
	console.log(text);
}

function wsOpen() {
	if (ws === undefined || ws.readyState != 0) {
		setMsg("info", "Opening WebSocket..");
		
		ws = new WebSocket("ws://" + location.host);
		
		ws.onopen = function(evt) {
			setMsg("success", "WebSocket is open.");
		};
		
		ws.onerror = function(evt) {
			setMsg("error", "WebSocket error!");
		};
		
		ws.onclose = function(evt) {
			setMsg("warning", "WebSocket is closed");
		};
		
		ws.onmessage = function(evt) {
			var received = JSON.parse(evt.data);
			//console.log(received);
			if(typeof received.free_heap !== "undefined") {
				document.getElementById('heapAlert').innerHTML = received.free_heap + ' free bytes of heap';
			}
			
			if(typeof received.module_data !== "undefined") {
				let module_card = document.getElementById('raw-module-card-' + received.module_data.address);
				
				module_card.style.display = "";
				
				module_card.getElementsByClassName("raw-module-card-header")[0].innerHTML = received.module_data.address + " | " + received.module_data.name;
				
				let module_card_channel_list = module_card.getElementsByClassName("raw-module-card-channel-list")[0];
				
				while (module_card_channel_list.lastChild) {
					module_card_channel_list.removeChild(module_card_channel_list.lastChild);
				}
				
				for(let c_n = 0; c_n < received.module_data.channels.length; c_n++) {
					for(let v_n = 0; v_n < received.module_data.channels[c_n].values.length; v_n++) {
						let list_item = document.createElement("div");
						let list_item_value = document.createElement("span");
						
						list_item.className = "siimple-list-item";
						list_item.id = received.module_data.address + "_" + c_n + "_" + v_n;
						list_item.innerHTML = received.module_data.channels[c_n].name + v_n;
						
						list_item_value.className = "siimple-tag siimple-tag--rounded siimple-tag--primary";
						list_item_value.innerHTML = received.module_data.channels[c_n].values[v_n];
						
						list_item.appendChild(list_item_value);
						
						module_card_channel_list.appendChild(list_item);
					}
				}
			}
		};
	}
}

function createRawModuleCards() {
	var original = document.getElementById("raw-module-card-base");
	
	for (let i = 0; i < 32; ++i) {
		let cloned = original.cloneNode(true);
		
		cloned.id = "raw-module-card-" + i;
		cloned.style.display = "none";
		
		document.getElementById("raw-module-card-row").appendChild(cloned);
	}
}
