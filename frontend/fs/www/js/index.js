function index_on_load(){
    SendGetHttp("/about", index_answer_about,on_ResponceErrorLog);
    SendGetHttp("/status", index_answer_state,on_ResponceErrorLog);
}

function index_answer_about(response_text) {
    try { 
        var response = JSON.parse(response_text);
        console.log(response);
        document.title = response.deviceName +" ui"
    } catch (e) {
        console.error("Parsing error:", e);
    }
}

function index_answer_state(response_text) {
    try { 
        var response = JSON.parse(response_text);
        console.log(response);
    } catch (e) {
        console.error("Parsing error:", e);
    }
}
