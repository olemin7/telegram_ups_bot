

function logs_on_load(){
	logs_request("");
}

function logs_request(reply) {
	setTimeout(()=>{ SendGetHttp("/getlogs", logs_answer, logs_request);},300);
}

function logs_answer(response_text) {
	var curLine=document.createTextNode(response_text);
	document.getElementById("log_content").appendChild(curLine);
	logs_autoscroll();
	logs_request();
}

function logs_autoscroll() {
    if (document.getElementById('id_logs_autoscroll').checked == true){
    	document.getElementById('log_content').scrollTop = document.getElementById('log_content').scrollHeight;
    }
}

function logs_Clear() {
    var list=document.getElementById("log_content");
    while (list.hasChildNodes()) {  
      list.removeChild(list.firstChild);
    }
}

