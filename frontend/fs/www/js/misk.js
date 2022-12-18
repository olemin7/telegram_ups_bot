	function reboot_request(){
		if (confirm("Confirm Restart") === true) {
			var url="/restart";
			SendGetHttp(url,function(resp){
			$('#id_restart').html("Restarting...")
			setTimeout(() => location.reload(true), 1000);
		},on_ResponceErrorLog);
		}
	}
